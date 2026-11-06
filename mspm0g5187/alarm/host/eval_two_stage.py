#!/usr/bin/env python3
"""Held-out scenes, stream level: the model alone against the model plus the cadence check.
  eval_two_stage.py MODEL_DIR"""
import sys, numpy as np
import train_alarm as TA, features as F
d = TA.get_set("test", "test", 600, 3, hop=TA.INFER_HOP)
L = np.load(sys.argv[1] + "/test_logits.npy")
Z = TA.windows(d, np.arange(len(d["y"])))
pred = L.argmax(1)
ok = np.array([pred[i] == 0 or F.cadence_ok(Z[i], pred[i]) for i in range(len(pred))])
fdur = F.FRAME / F.FS
def decide2(p, okk):
    """6 of the last 8 agree, and the cadence check passed on at least one of those 8 windows for that class."""
    for i in range(len(p)):
        last, lok = p[max(0, i - 7):i + 1], okk[max(0, i - 7):i + 1]
        for c in (1, 2):
            if (last == c).sum() >= 6 and ((last == c) & lok).any():
                return i, c
    return None, 0
for name, mode in (("model alone", 0), ("model + cadence", 1)):
    trips = caught = missed = wrong = 0; neg = 0.0; delays = []
    for s in np.unique(d["stream"]):
        k = np.where(d["stream"] == s)[0]; k = k[np.argsort(d["start"][k])]
        i, c = TA.decide(pred[k]) if mode == 0 else decide2(pred[k], ok[k]); truth = d["y"][k].max()
        if truth == 0:
            neg += len(k) * TA.INFER_HOP * fdur; trips += c > 0
        elif c == truth:
            caught += 1; delays.append((i - np.where(d["y"][k] == truth)[0][0]) * TA.INFER_HOP * fdur)
        elif c: wrong += 1
        else: missed += 1
    print(f"{name:16s} false trips {trips} in {neg/3600:.2f} h; alarms caught {caught}, missed {missed}, wrong class {wrong}; median delay {np.median(delays):.1f} s")
pos = d["y"] > 0
print("cadence check passes on", f"{ok[pos & (pred == d['y'])].mean():.3f}", "of correctly classified alarm windows")
