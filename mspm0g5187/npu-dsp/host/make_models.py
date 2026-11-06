#!/usr/bin/env python3
"""Fixed-weight DSP graphs for TI's Neural Network Compiler (NNC), no training.

Builds ONNX files in the integer-in-float form that TI's NPU quantization flow exports (read from
mspm0g3507/mcu-bench/edgeai/uci-room-occupancy/occ_cnn_500/model_tinpu_int8.onnx):
  input quantizer: Add(bias) Mul(scale) Mul(2^-shift) Floor Clip
  layer:           Conv/MatMul with integer weights, then Add(bias) Mul(mult) Mul(2^-k) Floor Clip
Each graph is also computed in plain integer numpy (the reference the board is checked against) and run
through onnxruntime to prove the ONNX file and the reference agree.

Run with the venv that has onnx, onnxruntime and numpy:
  ~/tinyml-bench/venv/bin/python make_models.py OUTDIR
Writes OUTDIR/<name>/model_tinpu_int8.onnx, OUTDIR/vectors.h (input and expected output of every model),
OUTDIR/report.json (accuracy numbers).
"""
import sys, os, json
import numpy as np
import onnx
from onnx import helper, TensorProto, numpy_helper as nh

BLOCK = 256          # output samples per block
TAPS = 64
NIN = BLOCK + TAPS - 1   # 319 input samples give 256 valid outputs

# ---------------------------------------------------------------- filter design (numpy only)
def sinc(x):
    return np.sinc(x)   # numpy's sinc is sin(pi x)/(pi x)

def lowpass(fc, n=TAPS):
    k = np.arange(n) - (n - 1) / 2
    h = 2 * fc * sinc(2 * fc * k) * np.hamming(n)
    return h / h.sum()          # unity gain at DC

def bandpass(fl, fh, n=TAPS):
    k = np.arange(n) - (n - 1) / 2
    h = (2 * fh * sinc(2 * fh * k) - 2 * fl * sinc(2 * fl * k)) * np.hamming(n)
    f0 = (fl + fh) / 2
    g = abs(np.sum(h * np.exp(-2j * np.pi * f0 * np.arange(n))))
    return h / g                # unity gain at the band centre

def quant_taps(h, limit=127):
    """Largest power-of-two scale 2^m with max|round(h*2^m)| <= limit. Returns (int taps, m)."""
    m = 0
    while np.max(np.abs(np.round(h * 2 ** (m + 1)))) <= limit:
        m += 1
    return np.round(h * 2 ** m).astype(np.int64), m

LP_FC = [0.10, 0.05, 0.15, 0.20]                       # channel 0 is the required low-pass; the others fill the 4-channel minimum
BANK_EDGES = [(0.02 + 0.06 * i, 0.08 + 0.06 * i) for i in range(8)]
BANK_EDGES[-1] = (0.44, 0.49)
TRIP = [15] * 8               # counts, on the int8 band output

# ---------------------------------------------------------------- test signal (deterministic)
def _noise(n):
    out = np.empty(n); s = 12345
    for i in range(n):
        s = (1103515245 * s + 12345) & 0x7FFFFFFF
        out[i] = ((s >> 16) % 9) - 4
    return out

def signal_a(n=NIN):
    """FIR test: two tones in the pass band of the 0.1 fs low-pass, two in the stop band, plus +-4 count noise."""
    t = np.arange(n)
    x = 40 * np.sin(2 * np.pi * 0.02 * t) + 30 * np.sin(2 * np.pi * 0.05 * t + 0.7) \
        + 30 * np.sin(2 * np.pi * 0.31 * t + 1.3) + 20 * np.sin(2 * np.pi * 0.43 * t + 2.1)
    return np.clip(np.round(x + _noise(n)), -128, 127).astype(np.int64)

BAND_AMP = [26, 8, 22, 10, 24, 6, 18, 12]
def signal_b(n=NIN):
    """Filter bank test: one tone at the centre of each band, different amplitudes, no noise."""
    t = np.arange(n)
    x = np.zeros(n)
    for i, (a, b) in enumerate(BANK_EDGES):
        x += BAND_AMP[i] * np.sin(2 * np.pi * ((a + b) / 2) * t + 0.4 * i)
    return np.clip(np.round(x), -128, 127).astype(np.int64)

def signal_c(n=NIN):
    """DFT test: tones on exact bins 3 and 10 of a 64-sample frame."""
    t = np.arange(n)
    x = 40 * np.sin(2 * np.pi * 3 * t / 64 + 0.3) + 30 * np.sin(2 * np.pi * 10 * t / 64 + 1.1)
    return np.clip(np.round(x), -128, 127).astype(np.int64)

# ---------------------------------------------------------------- integer reference
def bnorm_np(acc, bias, mult, k, lo, hi):
    """floor((acc + bias) * mult / 2^k) then clip, per channel along axis 1."""
    bshape = (1, -1) + (1,) * (acc.ndim - 2)
    v = (acc + bias.reshape(bshape)) * mult.reshape(bshape)
    v = np.floor_divide(v, (2 ** k).reshape(bshape))
    return np.clip(v, lo, hi)

def conv_valid(x, w, stride=1):
    """x [1,1,H,1] int, w [O,1,K,1] int -> [1,O,Hout,1]. ONNX Conv is correlation."""
    O, _, K, _ = w.shape
    H = x.shape[2]
    Ho = (H - K) // stride + 1
    out = np.zeros((1, O, Ho, 1), dtype=np.int64)
    for i in range(Ho):
        seg = x[0, 0, i * stride:i * stride + K, 0]
        out[0, :, i, 0] = w[:, 0, :, 0] @ seg
    return out

def maxpool_h(y, k):
    N, C, H, W = y.shape
    Ho = (H - k) // k + 1
    return y[:, :, :Ho * k, :].reshape(N, C, Ho, k, W).max(axis=3)

# ---------------------------------------------------------------- ONNX writers
def const(name, arr, dt=np.float32):
    return nh.from_array(np.asarray(arr, dtype=dt), name)

class G:
    def __init__(self, name, in_shape):
        self.name = name; self.nodes = []; self.init = []; self.cur = "input"
        self.in_shape = in_shape; self.n = 0
    def add(self, op, ins, **attr):
        self.n += 1; out = f"t{self.n}"
        self.nodes.append(helper.make_node(op, ins, [out], name=f"{op}_{self.n}", **attr))
        return out
    def c(self, nm, arr):
        self.init.append(const(nm, arr)); return nm
    def quant_in(self, shape_b):
        # bias 0, scale 2, 2^-1: an identity on integer-valued input that keeps the five-operator shape NNC expects
        b = self.c("in.bias", np.zeros(shape_b)); m = self.c("in.mult", np.full(shape_b, 2.0)); s = self.c("in.shift", np.full(shape_b, 0.5))
        lo = self.c("in.lo", np.float32(-128)); hi = self.c("in.hi", np.float32(127))
        x = self.add("Add", ["input", b]); x = self.add("Mul", [x, m]); x = self.add("Mul", [x, s]); x = self.add("Floor", [x])
        self.cur = self.add("Clip", [x, lo, hi])
    def bnorm(self, tag, x, bias, mult, k, shape_b, unsigned=False):
        b = self.c(f"{tag}.offset", np.reshape(bias, shape_b)); m = self.c(f"{tag}.mult", np.reshape(mult, shape_b))
        s = self.c(f"{tag}.shift_mult", np.reshape(2.0 ** -np.asarray(k, dtype=np.float64), shape_b))
        x = self.add("Add", [x, b]); x = self.add("Mul", [x, m]); x = self.add("Mul", [x, s]); x = self.add("Floor", [x])
        if unsigned:   # the pattern TI's flow writes for a layer that feeds an unsigned tensor
            lo = self.c(f"{tag}.lo", np.float32(-255)); hi = self.c(f"{tag}.hi", np.float32(255))
            x = self.add("Clip", [x, lo, hi]); x = self.add("Relu", [x])
            lo2 = self.c(f"{tag}.lo2", np.float32(0)); hi2 = self.c(f"{tag}.hi2", np.float32(255))
            return self.add("Clip", [x, lo2, hi2])
        lo = self.c(f"{tag}.lo", np.float32(-128)); hi = self.c(f"{tag}.hi", np.float32(127))
        return self.add("Clip", [x, lo, hi])
    def finish(self, out_name, out_shape, path):
        out = helper.make_tensor_value_info(out_name, TensorProto.FLOAT, out_shape)
        inp = helper.make_tensor_value_info("input", TensorProto.FLOAT, self.in_shape)
        g = helper.make_graph(self.nodes, self.name, [inp], [out], self.init)
        m = helper.make_model(g, opset_imports=[helper.make_opsetid("", 17)]); m.ir_version = 8
        onnx.checker.check_model(m)
        os.makedirs(os.path.dirname(path), exist_ok=True); onnx.save(m, path)
        # rename the output to a stable name
        return m

def build_fir_like(name, H_taps_q, ms, stride, outdir, tail=None):
    """Conv(FCONV) + bnorm [+ tail]. H_taps_q [O,TAPS] int; ms per-channel shift exponents."""
    O = H_taps_q.shape[0]
    g = G(name, [1, 1, NIN, 1]); g.quant_in((1, 1, 1, 1))
    w = np.flip(H_taps_q, axis=1).reshape(O, 1, TAPS, 1)   # flip: ONNX Conv is correlation, FIR is convolution
    wn = g.c("conv.weight", w)
    y = g.add("Conv", [g.cur, wn], dilations=[1, 1], group=1, kernel_shape=[TAPS, 1], pads=[0, 0, 0, 0], strides=[stride, 1])
    y = g.bnorm("conv", y, np.zeros(O), np.ones(O), ms, (1, O, 1, 1))
    return g, y, w

def run_ort(path, x_int, in_shape):
    import onnxruntime as ort
    s = ort.InferenceSession(path, providers=["CPUExecutionProvider"])
    return s.run(None, {"input": x_int.astype(np.float32).reshape(in_shape)})[0]

def snr(ref, test):
    ref = np.asarray(ref, dtype=np.float64); err = np.asarray(test, dtype=np.float64) - ref
    return 10 * np.log10(np.sum(ref ** 2) / max(np.sum(err ** 2), 1e-30))

def main(outdir):
    x = signal_a(); xin = x.reshape(1, 1, NIN, 1)
    xb = signal_b(); xbin = xb.reshape(1, 1, NIN, 1)
    xc = signal_c()
    models = {}      # name -> dict(expected int array, input array)
    report = {"taps": TAPS, "block": BLOCK, "nin": NIN, "models": {}}

    # ---- fir64: four low-passes, channel 0 is the 64-tap FIR with cutoff 0.1 fs
    hs = [lowpass(fc) for fc in LP_FC]
    qs, ms = zip(*[quant_taps(h) for h in hs]); Q = np.array(qs); ms = np.array(ms)
    g, y, w = build_fir_like("fir64", Q, ms, 1, outdir)
    path = f"{outdir}/fir64/model_tinpu_int8.onnx"; g.finish(y, [1, 4, BLOCK, 1], path)
    acc = conv_valid(xin, w[:, :, :, :]); exp = bnorm_np(acc, np.zeros(4, dtype=np.int64), np.ones(4, dtype=np.int64), ms, -128, 127)
    ort_out = run_ort(path, x, [1, 1, NIN, 1]).astype(np.int64)
    assert np.array_equal(ort_out, exp), "fir64: onnxruntime and integer reference differ"
    models["fir64"] = (exp, [1, 4, BLOCK, 1])
    r = {}
    for c in range(4):
        ideal = np.convolve(x.astype(np.float64), hs[c], "valid")                  # float taps, float arithmetic, no rounding
        wonly = np.convolve(x.astype(np.float64), Q[c] / 2.0 ** ms[c], "valid")     # int8 taps, still float arithmetic
        r[f"ch{c}"] = {"fc": LP_FC[c], "tap_shift_m": int(ms[c]), "max_tap": int(np.abs(Q[c]).max()),
                       "snr_weights_only_db": round(snr(ideal, wonly), 2),
                       "snr_total_db": round(snr(ideal, exp[0, c, :, 0]), 2),
                       "out_rms_counts": round(float(np.sqrt(np.mean(ideal ** 2))), 1)}
    report["models"]["fir64"] = r

    # ---- fir64_dec4: the same four filters, stride 4 (decimation by 4 as a strided convolution)
    g, y, w = build_fir_like("fir64_dec4", Q, ms, 4, outdir)
    path = f"{outdir}/fir64_dec4/model_tinpu_int8.onnx"; g.finish(y, [1, 4, (NIN - TAPS) // 4 + 1, 1], path)
    acc = conv_valid(xin, w, 4); exp4 = bnorm_np(acc, np.zeros(4, dtype=np.int64), np.ones(4, dtype=np.int64), ms, -128, 127)
    assert np.array_equal(run_ort(path, x, [1, 1, NIN, 1]).astype(np.int64), exp4)
    assert np.array_equal(exp4[0, :, :, 0], exp[0, :, ::4, 0][:, :exp4.shape[2]])
    models["fir64_dec4"] = (exp4, list(exp4.shape))

    # ---- bank8: eight 64-tap band-pass filters
    hb = [bandpass(a, b) for a, b in BANK_EDGES]
    qb, mb = zip(*[quant_taps(h) for h in hb]); QB = np.array(qb); mb = np.array(mb)
    g, y, wb = build_fir_like("bank8", QB, mb, 1, outdir)
    path = f"{outdir}/bank8/model_tinpu_int8.onnx"; g.finish(y, [1, 8, BLOCK, 1], path)
    accb = conv_valid(xbin, wb); expb = bnorm_np(accb, np.zeros(8, dtype=np.int64), np.ones(8, dtype=np.int64), mb, -128, 127)
    assert np.array_equal(run_ort(path, xb, [1, 1, NIN, 1]).astype(np.int64), expb)
    models["bank8"] = (expb, [1, 8, BLOCK, 1])
    r = {}
    for c in range(8):
        ideal = np.convolve(xb.astype(np.float64), hb[c], "valid")
        wonly = np.convolve(xb.astype(np.float64), QB[c] / 2.0 ** mb[c], "valid")
        r[f"band{c}"] = {"edges": [round(BANK_EDGES[c][0], 3), round(BANK_EDGES[c][1], 3)], "tap_shift_m": int(mb[c]),
                         "max_tap": int(np.abs(QB[c]).max()), "snr_weights_only_db": round(snr(ideal, wonly), 2),
                         "snr_total_db": round(snr(ideal, expb[0, c, :, 0]), 2), "out_rms_counts": round(float(np.sqrt(np.mean(ideal ** 2))), 1)}
    report["models"]["bank8"] = r

    # ---- bank8_trip: bank, max pool over the whole block, 1x1 comparator (bias = -127*threshold, clip to unsigned)
    TRIP_K = 1          # shift 2^-1: a shift of exactly 2^0 made the compiler drop a multiply and miss the layer pattern
    def trip_graph(name, pools, unsigned):
        g, y, wb2 = build_fir_like(name, QB, mb, 1, outdir)
        for i, k in enumerate(pools):
            y = g.add("MaxPool", [y], ceil_mode=0, dilations=[1, 1], kernel_shape=[k, 1], pads=[0, 0, 0, 0], strides=[k, 1])
        wt = np.zeros((8, 8, 1, 1), dtype=np.int64)
        for c in range(8): wt[c, c, 0, 0] = 127
        wn = g.c("trip.weight", wt)
        y = g.add("Conv", [y, wn], dilations=[1, 1], group=1, kernel_shape=[1, 1], pads=[0, 0, 0, 0], strides=[1, 1])
        bias = -127 * np.array(TRIP, dtype=np.int64)
        y = g.bnorm("trip", y, bias, np.ones(8), np.full(8, TRIP_K), (1, 8, 1, 1), unsigned=unsigned)
        return g, y, wt
    for name, pools, unsigned in (("bank8_trip", [BLOCK], True), ("bank8_trip2", [16, 16], True), ("bank8_trip3", [16], True), ("bank8_trips", [BLOCK], False)):
        g, y, wt = trip_graph(name, pools, unsigned)
        Hout = BLOCK // int(np.prod(pools))
        path = f"{outdir}/{name}/model_tinpu_int8.onnx"; g.finish(y, [1, 8, Hout, 1], path)
        p = expb
        for k in pools: p = maxpool_h(p, k)
        acc = np.einsum("oi,nihw->nohw", wt[:, :, 0, 0], p)
        lo, hi = (0, 255) if unsigned else (-128, 127)
        expt = bnorm_np(acc, -127 * np.array(TRIP, dtype=np.int64), np.ones(8, dtype=np.int64), np.full(8, TRIP_K), lo, hi)
        assert np.array_equal(run_ort(path, xb, [1, 1, NIN, 1]).astype(np.int64), expt), name
        models[name] = (expt, list(expt.shape))
        report["models"][name] = {"band_amplitudes": BAND_AMP, "peak_per_band_counts": [int(v) for v in maxpool_h(expb, BLOCK)[0, :, 0, 0]],
                                  "trip_counts": TRIP, "out": [int(v) for v in expt.ravel()]}

    # ---- dft64: one 64-sample frame, bins 0..31, cos and sin rows as a fully connected layer
    N = 64; K = 32
    n = np.arange(N); kk = np.arange(K)
    cosw = np.cos(2 * np.pi * np.outer(kk, n) / N); sinw = -np.sin(2 * np.pi * np.outer(kk, n) / N)
    Wf = np.round(127 * np.vstack([cosw, sinw])).astype(np.int64)      # [64 outputs, 64 inputs]
    g = G("dft64", [1, N]); g.quant_in((1, 1))
    wn = g.c("fc.weight", Wf.T)
    y = g.add("MatMul", [g.cur, wn])
    # 127*sum(x) up to 127*127*64: divide by 2^9 to land near the int8 range for a half-scale input
    y = g.bnorm("fc", y, np.zeros(N), np.ones(N), np.full(N, 11), (1, N))
    path = f"{outdir}/dft64/model_tinpu_int8.onnx"; g.finish(y, [1, N], path)
    xf = xc[:N].reshape(1, N)
    accd = xf @ Wf.T; expd = bnorm_np(accd, np.zeros(N, dtype=np.int64), np.ones(N, dtype=np.int64), np.full(N, 11), -128, 127)
    assert np.array_equal(run_ort(path, xc[:N], [1, N]).astype(np.int64), expd)
    models["dft64"] = (expd, [1, N])
    X = np.fft.rfft(xc[:N].astype(np.float64))[:K]
    ideal = np.concatenate([np.real(X), np.imag(X)]) * 127 / 2 ** 11
    report["models"]["dft64"] = {"snr_total_db": round(snr(ideal, expd.ravel()), 2), "note": "float DFT of the same 64 samples at the same scale"}

    # ---- vectors.h
    with open(f"{outdir}/vectors.h", "w") as f:
        f.write("/* Generated by make_models.py: the test signal and the integer reference output of every model. */\n#include <stdint.h>\n")
        f.write(f"#define DSP_NIN {NIN}\n#define DSP_BLOCK {BLOCK}\n#define DSP_TAPS {TAPS}\n")
        for nm_, arr_ in (("a", x), ("b", xb), ("c", xc)):
            f.write(f"static const int8_t dsp_input_{nm_}[DSP_NIN] = {{" + ",".join(str(int(v)) for v in arr_) + "};\n")
        for nm, (e, shp) in models.items():
            flat = e.ravel()
            t = "int8_t" if (nm == "bank8_trips" or not nm.startswith("bank8_trip")) else "uint8_t"
            f.write(f"#define EXP_{nm.upper()}_N {flat.size}\n")
            f.write(f"static const {t} exp_{nm}[{flat.size}] = {{" + ",".join(str(int(v)) for v in flat) + "};\n")
        # plain-C baseline taps: channel 0 (single FIR) and all four, tap order as convolution
        f.write("static const int8_t taps_fir64[4][DSP_TAPS] = {" + ",".join("{" + ",".join(str(int(v)) for v in row) + "}" for row in Q) + "};\n")
        f.write("static const uint8_t taps_fir64_m[4] = {" + ",".join(str(int(v)) for v in ms) + "};\n")
        f.write("static const int8_t taps_bank8[8][DSP_TAPS] = {" + ",".join("{" + ",".join(str(int(v)) for v in row) + "}" for row in QB) + "};\n")
        f.write("static const uint8_t taps_bank8_m[8] = {" + ",".join(str(int(v)) for v in mb) + "};\n")
    json.dump(report, open(f"{outdir}/report.json", "w"), indent=1)
    np.savez(f"{outdir}/ref.npz", x=x, xb=xb, xc=xc, **{k: v[0] for k, v in models.items()})
    print(json.dumps(report, indent=1))

if __name__ == "__main__":
    main(sys.argv[1])
