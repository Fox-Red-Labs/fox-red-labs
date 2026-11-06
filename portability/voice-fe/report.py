#!/usr/bin/env python3
"""Read a voice front-end report (board serial capture, probe read-back or host.log) and print the numbers.
  python3 -I report.py <board log> --host <host.log> [--sig <sig.npz>] [--mhz 160] [--json out.json]
Checks every FE,L line (per-pass CRC-32 of the final and echo-canceller output, and the three energies) against the host
reference run of the same code, and prints ms per 10 ms frame (mean, worst, best, 99th percentile from the 0.1 ms
histogram), the share of each stage, the heap high-water mark, and the echo return loss enhancement (ERLE) from the energies:
ERLE = 10 log10(energy into the canceller / energy out), over passes 2 to 8 (the first pass is convergence). With --sig
it also prints the ceiling set by the near-end noise: the echo-to-noise ratio at the beamformer output."""
import argparse, json, math, re

ap = argparse.ArgumentParser(); ap.add_argument("log"); ap.add_argument("--host", required=True); ap.add_argument("--sig")
ap.add_argument("--mhz", type=float, default=160); ap.add_argument("--json"); a = ap.parse_args()


def parse(p):
    t = open(p, "rb").read().decode("latin1").replace("\r", "")
    i = t.rfind("FE,CFG"); j = t.find("FE,DONE", i)        # the last complete report in the capture
    body = t[i:j + 7] + t[j + 7:].split("FE END")[0]
    L = {}; H = {}; d = {}
    for l in body.split("\n"):
        f = l.strip().split(",")
        if l.startswith("FE,L,"): L[int(f[2])] = f[3:8]
        elif l.startswith("FE,H,"): H[int(f[2])] = int(f[3])
        elif l.startswith("FE,T,"): d["T"] = f[2:]
        elif l.startswith("FE,MEM,") or l.startswith("FE,CFG,") or l.startswith("FE,STACK,"):
            for kv in f[2:]:
                if "=" in kv: k, v = kv.split("="); d[k] = int(v) if v.isdigit() else v
    return L, H, d


bl, bh, bd = parse(a.log); hl, _, hd = parse(a.host)
same = [bl.get(k) == hl.get(k) for k in sorted(hl)]
print("passes %d, equal to the host run: %d of %d (CRC of output, CRC of canceller output, three energies)" % (len(bl), sum(same), len(hl)))
T = bd["T"]; frames = int(T[0]); hz = int(bd["tick_hz"]); ms = lambda c: 1000.0 * c / hz
s_bf, s_aec, s_ns, s_tot = (int(x, 16) for x in T[1:5]); mx, mx_i, mx1, mn = (int(x) for x in T[5:9])
tot = 0; p99 = None
for b in sorted(bh):
    tot += bh[b]
    if p99 is None and tot >= 0.99 * frames: p99 = (b + 1) / 10.0
r = dict(frames=frames, mean_ms=ms(s_tot / frames), worst_ms=ms(mx), worst_frame=mx_i, worst_after_first_pass_ms=ms(mx1), best_ms=ms(mn), p99_ms_upper=p99,
         bf_ms=ms(s_bf / frames), aec_ms=ms(s_aec / frames), ns_ms=ms(s_ns / frames), heap=bd.get("heap"), heap_echo=bd.get("heap_echo"), ws=bd.get("ws"),
         stack=bd.get("used", bd.get("used_below_main")), cycles_mean=s_tot / frames, bit_exact=all(same) and len(bl) == len(hl))
r["load_pct"] = 100 * r["mean_ms"] / 10; r["load_worst_pct"] = 100 * r["worst_ms"] / 10
e = [[int(v, 16) for v in hl[k][2:5]] for k in sorted(hl)]
r["erle_aec_db"] = 10 * math.log10(sum(x[0] for x in e[1:]) / sum(x[1] for x in e[1:]))
r["erle_total_db"] = 10 * math.log10(sum(x[0] for x in e[1:]) / sum(x[2] for x in e[1:]))
r["erle_aec_db_per_pass"] = [round(10 * math.log10(x[0] / x[1]), 2) for x in e]
if a.sig:
    import numpy as np
    s = np.load(a.sig); taps = s["taps"].astype(float) / 32768
    bf = lambda m: sum(np.convolve(m[:, k], taps[k])[:m.shape[0]] for k in range(4)) / 4
    r["echo_to_noise_db_at_bf"] = 10 * math.log10(np.mean(bf(s["mic_echo"]) ** 2) / np.mean(bf(s["mic_noise"]) ** 2))
print("ms per 10 ms frame: mean %.3f (load %.1f %%), worst %.3f (frame %d; %.3f after pass 1), best %.3f, 99th percentile under %.1f" % (
    r["mean_ms"], r["load_pct"], r["worst_ms"], mx_i, r["worst_after_first_pass_ms"], r["best_ms"], p99))
print("stages: beamformer %.3f ms, echo canceller %.3f ms, noise suppressor %.3f ms; %.0f cycles per frame at %d Hz" % (r["bf_ms"], r["aec_ms"], r["ns_ms"], r["cycles_mean"], hz))
print("RAM: heap high-water %s B (echo canceller %s B), own buffers %s B, stack %s B" % (r["heap"], r["heap_echo"], r["ws"], r["stack"]))
print("ERLE passes 2-8: canceller %.1f dB, canceller and suppressor %.1f dB; per pass %s%s" % (r["erle_aec_db"], r["erle_total_db"], r["erle_aec_db_per_pass"],
      "; echo-to-noise at the beamformer output %.1f dB" % r["echo_to_noise_db_at_bf"] if "echo_to_noise_db_at_bf" in r else ""))
if a.json: json.dump(r, open(a.json, "w"), indent=1)
