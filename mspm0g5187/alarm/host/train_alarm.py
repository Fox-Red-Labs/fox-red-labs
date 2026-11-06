#!/usr/bin/env python3
"""Train the alarm listener's CNN (other, smoke, co) on synthetic scenes (synth.py) plus any real
recordings, quantize it with TI's NPU 8-bit QAT, export the ONNX file TI's NNC compiles, and score it.

  train_alarm.py OUTDIR [--streams 2000] [--real real.npz] [--epochs 30]

Datasets are cached in out/alarm/sets/. Scores written to OUTDIR/result.json:
  window accuracy and confusion on held-out scenes (decoy sources the model never heard),
  stream level with the board's decision rule (DECIDE_K of the last DECIDE_M inferences, one inference
  every INFER_HOP frames): false trips per hour of alarm-free audio, and alarms caught with their delay.
Same recipe as tools/dataset/train.py: float training, TINPUTinyMLQATFxModule, convert, export, fold.py.
"""
import argparse
import json
import os
import subprocess
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "../../.."))
EDGE = os.path.join(REPO, "mspm0g3507/mcu-bench/edgeai/tools")
sys.path.insert(0, HERE)
import features as F  # noqa: E402
import synth  # noqa: E402

BANK = os.path.abspath("out/alarm/bank")
SETS = os.path.abspath("out/alarm/sets")
INFER_HOP = 16          # frames between inferences on the board (1.03 s)
DECIDE_M, DECIDE_K = 8, 6
REAL_WEIGHT = 20
# (cout, kernel, maxpool after): channel counts multiples of 4, kernels 3 or 5 (TI NPU rules)
SPEC = [(16, 5, True), (16, 5, True), (32, 5, True), (32, 5, True), (32, 3, False)]   # sees about 123 of the 128 frames


def get_set(name, split, n, seed, hop=8, hard=False):
    os.makedirs(SETS, exist_ok=True)
    p = os.path.join(SETS, f"{name}_{n}_{seed}.npz")
    if not os.path.exists(p):
        t = time.time()
        d = synth.make_set(BANK, split, n, seed, hop, hard=hard)
        np.savez_compressed(p, **{k: (v.astype(np.float16) if k == "frames" else v) for k, v in d.items()})
        print(f"built {p} in {time.time()-t:.0f} s", flush=True)
    z = np.load(p)
    return {k: z[k].astype(np.float32) if k == "frames" else z[k] for k in z.files}


def windows(d, idx):
    st, sid = d["start"][idx], d["stream"][idx]
    ar = st[:, None] + np.arange(F.T)[None, :]
    return F.normalize(d["frames"][sid[:, None], ar])          # (n, C, T)


def decide(pred_seq):
    """Board decision rule on one stream's inference sequence: first index where K of last M agree."""
    for i in range(len(pred_seq)):
        last = pred_seq[max(0, i - DECIDE_M + 1):i + 1]
        for c in (1, 2):
            if (last == c).sum() >= DECIDE_K:
                return i, c
    return None, 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("outdir")
    ap.add_argument("--streams", type=int, default=2000)
    ap.add_argument("--epochs", type=int, default=30)
    ap.add_argument("--qat-epochs", type=int, default=10)
    ap.add_argument("--hard", type=int, default=0, help="extra scenes heavy on look-alike beeping")
    ap.add_argument("--real", help="npz of real recordings in the same format (frames, y, w, start, stream)")
    a = ap.parse_args()

    import torch
    import torch.nn as nn
    import onnxruntime as ort
    if not hasattr(torch.compiler, "is_compiling"):
        torch.compiler.is_compiling = lambda: False
    _onnx_export = torch.onnx.export

    def _export_no_dynamo(*args, **kw):
        kw.pop("dynamo", None)
        return _onnx_export(*args, **kw)
    torch.onnx.export = _export_no_dynamo
    from tinyml_torchmodelopt.quantization import TINPUTinyMLQATFxModule, TinyMLQConfigType

    torch.manual_seed(0)
    np.random.seed(0)
    torch.set_num_threads(4)
    os.makedirs(a.outdir, exist_ok=True)
    tr = get_set("train", "train", a.streams, 1)
    if a.hard:
        h = get_set("hard", "train", a.hard, 4, hard=True)
        h["stream"] = h["stream"] + tr["frames"].shape[0]
        tr = dict(frames=np.concatenate([tr["frames"], h["frames"]]), y=np.concatenate([tr["y"], h["y"]]),
                  w=np.concatenate([tr["w"], h["w"]]), start=np.concatenate([tr["start"], h["start"]]),
                  stream=np.concatenate([tr["stream"], h["stream"]]))
    va = get_set("val", "train", max(200, a.streams // 10), 2)
    te = get_set("test", "test", 600, 3, hop=INFER_HOP)
    sets = [tr]
    if a.real:
        z = np.load(a.real)
        sets.append({k: z[k] for k in z.files})

    def all_idx(d):
        return np.where(d["w"] > 0)[0]

    class Net(nn.Module):
        def __init__(self):
            super().__init__()
            layers = [nn.BatchNorm2d(F.C)]
            c = F.C
            for cout, k, pool in SPEC:
                layers += [nn.Conv2d(c, cout, (k, 1), padding=(k // 2, 0)), nn.BatchNorm2d(cout), nn.ReLU()]
                if pool:
                    layers += [nn.MaxPool2d((3, 1), stride=(2, 1), padding=(1, 0))]
                c = cout
            layers += [nn.AdaptiveAvgPool2d((1, 1)), nn.Flatten(), nn.Linear(c, 3)]
            self.body = nn.Sequential(*layers)

        def forward(self, x):
            return self.body(x)

    def batches(bs=128):
        pools = [(d, all_idx(d)) for d in sets]
        sizes = np.array([len(p[1]) for p in pools], float)
        sizes[1:] *= REAL_WEIGHT                     # real recordings are few: sample them more often
        n = int(sizes.sum()) // bs
        for _ in range(n):
            k = np.random.choice(len(pools), p=sizes / sizes.sum()) if len(pools) > 1 else 0
            d, idx = pools[k]
            sel = np.random.choice(idx, bs)
            x = windows(d, sel)
            x[:, :10] += np.random.normal(0, 0.03, x[:, :10].shape).astype(np.float32)   # small level jitter
            yield torch.tensor(x[..., None]), torch.tensor(d["y"][sel])

    cnt = np.bincount(tr["y"][all_idx(tr)], minlength=3)
    wts = torch.tensor((cnt.sum() / np.maximum(cnt, 1)) ** 0.5, dtype=torch.float32)
    wts /= wts.mean()
    lossf = nn.CrossEntropyLoss(weight=wts)

    def run(model, opt, epochs, sched=None):
        for ep in range(epochs):
            model.train()
            tl = 0
            for xb, yb in batches():
                opt.zero_grad()
                l = lossf(model(xb), yb)
                l.backward()
                opt.step()
                tl += l.item()
            if sched:
                sched.step()
            print(f"  epoch {ep+1}/{epochs} loss {tl:.1f} val acc {acc(model, va):.4f}", flush=True)

    def predict(model, d, idx, bs=512):
        model.eval()
        out = []
        with torch.no_grad():
            for i in range(0, len(idx), bs):
                out.append(model(torch.tensor(windows(d, idx[i:i + bs])[..., None])).numpy())
        return np.concatenate(out)

    def acc(model, d):
        idx = all_idx(d)
        return float((predict(model, d, idx).argmax(1) == d["y"][idx]).mean())

    model = Net()
    t0 = time.time()
    opt = torch.optim.Adam(model.parameters(), 2e-3)
    run(model, opt, a.epochs, torch.optim.lr_scheduler.CosineAnnealingLR(opt, a.epochs))
    t_float = time.time() - t0
    ex = torch.rand((1, F.C, F.T, 1))
    qcfg = TinyMLQConfigType(weight_bitwidth=8, activation_bitwidth=8, auto_quantization=False)
    qmodel = TINPUTinyMLQATFxModule(model, example_inputs=ex, qconfig_type=qcfg, total_epochs=a.qat_epochs)
    opt = torch.optim.Adam(qmodel.parameters(), 2e-4)
    t1 = time.time()
    run(qmodel, opt, a.qat_epochs)
    t_qat = time.time() - t1
    qmodel.eval()
    qmodel = qmodel.convert()
    raw = os.path.join(a.outdir, "model_unsimplified.onnx")
    onnx_path = os.path.join(a.outdir, "model_tinpu_int8.onnx")
    qmodel.export(ex, raw, input_names=["input"], simplify=False)
    subprocess.run([sys.executable, os.path.join(EDGE, "fold.py"), raw, onnx_path], check=True)
    os.remove(raw)

    # ---- held-out scoring with the int8 ONNX model (what NNC compiles) ----
    sess = ort.InferenceSession(onnx_path)
    idx = np.arange(len(te["y"]))
    X = windows(te, idx)[..., None]
    logits = np.concatenate([sess.run(None, {"input": X[i:i + 1]})[0] for i in range(len(X))])
    pred = logits.argmax(1)
    m = te["w"] > 0
    cm = np.zeros((3, 3), int)
    for t_, p_ in zip(te["y"][m], pred[m]):
        cm[t_, p_] += 1
    win_acc = float((pred[m] == te["y"][m]).mean())
    # stream level
    fdur = F.FRAME / F.FS
    trips, caught, missed, wrong, delays, neg_s = 0, 0, 0, 0, [], 0.0
    trip_streams = []
    for s in np.unique(te["stream"]):
        k = np.where(te["stream"] == s)[0]
        k = k[np.argsort(te["start"][k])]
        truth = te["y"][k]
        i, c = decide(pred[k])
        has = truth.max()
        if has == 0:
            neg_s += (len(k) + F.T / INFER_HOP) * INFER_HOP * fdur
            if c:
                trips += 1
                trip_streams.append(int(s))
        else:
            first_pos = np.where(truth == has)[0]
            if c == has:
                caught += 1
                delays.append(float((i - first_pos[0]) * INFER_HOP * fdur))
            elif c:
                wrong += 1
            else:
                missed += 1
    res = dict(spec=SPEC, classes=synth.CLASSES, streams_train=a.streams, epochs=a.epochs, qat_epochs=a.qat_epochs,
               real=a.real, window_acc_int8=win_acc, confusion_int8=cm.tolist(),
               decision=dict(M=DECIDE_M, K=DECIDE_K, infer_hop_frames=INFER_HOP),
               false_trips=trips, alarm_free_hours=round(neg_s / 3600, 2), trip_streams=trip_streams,
               alarms_caught=caught, alarms_missed=missed, alarms_wrong_class=wrong,
               delay_after_first_positive_window_s=dict(median=float(np.median(delays)) if delays else None,
                                                        max=float(np.max(delays)) if delays else None),
               train_seconds=dict(float=round(t_float), qat=round(t_qat)), onnx_bytes=os.path.getsize(onnx_path),
               params=int(sum(p.numel() for p in model.parameters())))
    json.dump(res, open(os.path.join(a.outdir, "result.json"), "w"), indent=1)
    print(json.dumps({k: res[k] for k in ("window_acc_int8", "confusion_int8", "false_trips", "alarm_free_hours",
                                         "alarms_caught", "alarms_missed", "alarms_wrong_class",
                                         "delay_after_first_positive_window_s", "params")}), flush=True)
    # test vectors for the board: 2 per class from held-out windows, inputs as int16 Q11 (x 2048)
    sel = []
    rng = np.random.default_rng(0)
    for c in range(3):
        ii = np.where((te["y"] == c) & m)[0]
        sel += list(rng.choice(ii, 2, replace=False))
    q = lambda v: int(np.clip(np.round(v * 2048), -32768, 32767))
    with open(os.path.join(a.outdir, "test_vectors.h"), "w") as f:
        f.write("/* Held-out windows from train_alarm.py: input [C][T] as int16 Q11 (divide by 2048), and the class\n"
                " * and int8 logits onnxruntime gives for the int8 ONNX model on the Q11-rounded input. */\n")
        f.write(f"#define TV_N {len(sel)}\n#define TV_Q 2048.0f\n")
        f.write(f"static const int16_t tv_x[{len(sel)}][{F.C * F.T}] = {{\n")
        xs = []
        for i in sel:
            xq = np.array([q(v) for v in X[i].ravel()], np.int16)
            xs.append(xq)
            f.write("{" + ",".join(str(int(v)) for v in xq) + "},\n")
        f.write("};\n")
        lq = [sess.run(None, {"input": (x.astype(np.float32) / 2048).reshape(1, F.C, F.T, 1)})[0][0] for x in xs]
        f.write(f"static const uint8_t tv_label[{len(sel)}] = {{{','.join(str(int(te['y'][i])) for i in sel)}}};\n")
        f.write(f"static const uint8_t tv_class[{len(sel)}] = {{{','.join(str(int(l.argmax())) for l in lq)}}};\n")
        f.write(f"static const float tv_logits[{len(sel)}][3] = {{{','.join('{' + ','.join(f'{float(v):.4f}f' for v in l) + '}' for l in lq)}}};\n")


if __name__ == "__main__":
    main()
