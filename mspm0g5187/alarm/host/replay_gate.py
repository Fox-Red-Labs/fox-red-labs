#!/usr/bin/env python3
"""Replay the board's decision on its own logged per-inference lines (cls, cad, tone_hz, tone_db), with a learned
alarm, exactly as detect.h decides: K of M agree, the timing check passed once among them, the current window
still hears it, then tone within max(80 Hz, 4 %), level no more than 10 dB below what was learned.
  replay_gate.py LIVE_LOG LIVE_JSON --learn-from SCENE"""
import json, re, sys
log, js = sys.argv[1], sys.argv[2]
learn_scene = sys.argv[sys.argv.index("--learn-from") + 1]
marks = json.load(open(js))["marks"]
rows = []
for ln in open(log):
    t, rest = ln.split(" ", 1)
    m = re.match(r"I n=\d+ cls=(\w+) .* cad=(\d) tone_hz=(-?\d+) tone_db=(-?[\d.]+)", rest)
    if m:
        rows.append((float(t), ["other", "smoke", "co"].index(m[1]), int(m[2]), int(m[3]), float(m[4])))
def scene(t):
    for i, (nm, st, en) in enumerate(marks):
        if st and st <= t < (next((x[1] for x in marks[i + 1:] if x[1]), en + 10)):
            return nm
    return None
L = [r for r in rows if scene(r[0]) == learn_scene and r[1] >= 1 and r[2] and r[3] > 0]
fs = sorted(r[3] for r in L); ls = sorted(r[4] for r in L)
cls = 1 if sum(r[1] == 1 for r in L) >= sum(r[1] == 2 for r in L) else 2
f0, l0 = fs[len(fs) // 2], ls[len(ls) // 2]
print(f"learned from '{learn_scene}' ({len(L)} inferences): class {['', 'smoke', 'co'][cls]}, tone {f0} Hz, level {l0:.1f} dB")
hist, okh, out = [0] * 8, [0] * 8, {}
for t, c, cad, f, l in rows:
    hist = hist[1:] + [c]; okh = okh[1:] + [cad]
    pat = 0
    for k in (1, 2):
        if sum(h == k for h in hist) >= 6 and any(h == k and o for h, o in zip(hist, okh)):
            pat = k
    if not pat or c != pat:
        continue
    tol = max(80, 0.04 * f0)
    why = (f"ALARM {['', 'smoke', 'co'][pat]}" if abs(f - f0) <= tol and l >= l0 - 10 else
           "IGNORED tone" if abs(f - f0) > tol else "IGNORED too quiet")
    out.setdefault(scene(t), []).append(f"{why} ({f} Hz, {l:.1f} dB)")
for nm, st, en in marks:
    if st:
        v = out.get(nm, [])
        print(f"  {nm:22s} {sorted(set(x.split(' (')[0] for x in v)) or '-'}  {v[:1]}")
