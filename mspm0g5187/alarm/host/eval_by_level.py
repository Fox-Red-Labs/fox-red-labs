#!/usr/bin/env python3
"""Alarms caught on held-out scenes, by how loud the alarm is against the bed and against the loudest
decoy (the product sits beside the alarm, so the alarm is normally the loudest sound).
  eval_by_level.py MODEL_DIR"""
import sys, numpy as np
import synth, train_alarm as TA, features as F
md = sys.argv[1]
d = TA.get_set("test", "test", 600, 3, hop=TA.INFER_HOP)
L = np.load(md + "/test_logits.npy"); pred = L.argmax(1)
Z = TA.windows(d, np.arange(len(d["y"])))
ok = np.array([pred[i] == 0 or F.cadence_ok(Z[i], pred[i]) for i in range(len(pred))])
def decide2(p, okk):
    for i in range(len(p)):
        last, lok = p[max(0, i - 7):i + 1], okk[max(0, i - 7):i + 1]
        for c in (1, 2):
            if (last == c).sum() >= 6 and ((last == c) & lok).any():
                return i, c
    return None, 0
rng = np.random.default_rng(3); bank = synth.Bank(TA.BANK, "test")
rows = []
for k in range(600):
    r = rng.random(); alarm = "t3" if r < 0.4 else ("t4" if r < 0.7 else None)
    x, b, info = synth.scene(rng, bank, alarm)
    if not alarm:
        continue
    idx = np.where(d["stream"] == k)[0]; idx = idx[np.argsort(d["start"][idx])]
    truth = d["y"][idx].max()
    if truth == 0:
        continue                       # the alarm never fills a window (too short or at the edges)
    i, c = decide2(pred[idx], ok[idx])
    over_bed = info["alarm_level"] - info["bed"]
    over_dec = info["alarm_level"] - max(info["decoy_levels"]) if info["decoy_levels"] else 99
    rows.append((over_bed, over_dec, c == truth))
rows = np.array(rows, float)
for lo, hi in ((0, 10), (10, 20), (20, 40), (40, 99)):
    m = (rows[:, 0] >= lo) & (rows[:, 0] < hi)
    print(f"alarm {lo:2d} to {hi:2d} dB over the room bed: caught {int(rows[m, 2].sum())} of {m.sum()}")
for lo, hi in ((-99, 0), (0, 10), (10, 99)):
    m = (rows[:, 1] >= lo) & (rows[:, 1] < hi)
    print(f"alarm {lo:3d} to {hi:2d} dB over the loudest decoy: caught {int(rows[m, 2].sum())} of {m.sum()}")
m = (rows[:, 0] >= 20) & (rows[:, 1] >= 0)
print(f"beside-the-alarm case (20 dB or more over the bed, louder than every decoy): caught {int(rows[m, 2].sum())} of {m.sum()}")
