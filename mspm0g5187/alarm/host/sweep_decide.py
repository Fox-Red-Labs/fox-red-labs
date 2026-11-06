#!/usr/bin/env python3
"""Sweep the board's decision rule on held-out scenes for one trained model:
K of the last M inferences agree on smoke or co, each with a softmax margin over 'other'.
  sweep_decide.py MODEL_DIR"""
import sys, numpy as np, onnxruntime as ort
import train_alarm as TA, features as F
d = TA.get_set("test", "test", 600, 3, hop=TA.INFER_HOP)
sess = ort.InferenceSession(sys.argv[1] + "/model_tinpu_int8.onnx")
X = TA.windows(d, np.arange(len(d["y"])))[..., None]
L = np.concatenate([sess.run(None, {"input": X[i:i+1]})[0] for i in range(len(X))]).astype(np.float64)
np.save(sys.argv[1] + "/test_logits.npy", L)
fdur = F.FRAME / F.FS
def run(M, K, margin):
    trips = caught = missed = wrong = 0; delays = []; neg = 0.0
    for s in np.unique(d["stream"]):
        k = np.where(d["stream"] == s)[0]; k = k[np.argsort(d["start"][k])]
        lg = L[k]; c = lg.argmax(1)
        c = np.where(lg.max(1) - lg[:, 0] >= margin, c, 0)
        hit = None
        for i in range(len(c)):
            last = c[max(0, i - M + 1):i + 1]
            for cl in (1, 2):
                if (last == cl).sum() >= K: hit = (i, cl); break
            if hit: break
        truth = d["y"][k].max()
        if truth == 0:
            neg += len(k) * TA.INFER_HOP * fdur
            trips += hit is not None
        elif hit and hit[1] == truth:
            caught += 1; delays.append((hit[0] - np.where(d["y"][k] == truth)[0][0]) * TA.INFER_HOP * fdur)
        elif hit: wrong += 1
        else: missed += 1
    return trips, neg / 3600, caught, missed, wrong, np.median(delays) if delays else None
for M, K in [(8, 6), (8, 7), (10, 8), (12, 9), (12, 10)]:
    for mg in (0, 1, 2, 3):
        t, h, c, m, w, dl = run(M, K, mg)
        print(f"M={M:2d} K={K:2d} margin={mg}: false trips {t:2d} in {h:.2f} h, caught {c}, missed {m}, wrong class {w}, median delay {dl:.1f} s")
