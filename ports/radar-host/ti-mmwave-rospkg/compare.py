#!/usr/bin/env python3
"""Compare what ti_mmwave_rospkg published (points.json from record_points.py) with the recording.
The driver maps radar (x, y, z) to ROS (y, -x, z) and drops points with radar y = 0 (its x != 0 filter).
  compare.py <points.json> <walk.json>"""
import json, sys
got = json.load(open(sys.argv[1])); walk = json.load(open(sys.argv[2]))
want = [[(p[1], -p[0], p[2]) for p in (f.get("points") or []) if p[1] != 0] for f in walk["frames"]]
want = [w for w in want if w]
msgs = got["messages"]
n_want, n_got = sum(map(len, want)), sum(map(len, msgs))
match = worst = 0
for g, w in zip(msgs, want):
    if len(g) != len(w): continue
    d = max(abs(a - b) for gp, wp in zip(g, w) for a, b in zip(gp[:3], wp))
    worst = max(worst, d); match += d < 1e-4
print(f"recording: {len(want)} frames with points, {n_want} points; driver: {len(msgs)} messages, {n_got} points; "
      f"fields {got['fields']}; frames identical {match}; largest difference {worst:.2e}")
