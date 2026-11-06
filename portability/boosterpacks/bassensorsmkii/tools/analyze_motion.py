#!/usr/bin/env python3
"""Clean a motion_stream capture to a CSV and report the achieved rate.
  python3 analyze_motion.py <raw.txt> <out.csv>
Keeps the header, drops comment lines (# ...) and lines that are cut or corrupt, writes the rest as CSV. Reports:
  samples, the span on the microcontroller's own microsecond clock (host_us), the rate it gives, gaps over 15 ms,
  and, when sensor_ticks moves (a BMI160), the rate from the sensor's own clock (39.0625 us per tick, 24 bit)."""
import sys, statistics
raw, out = sys.argv[1], sys.argv[2]
hdr = "seq,host_us,sensor_ticks,ax,ay,az,gx,gy,gz"
rows, bad, notes = [], 0, []
for ln in open(raw, errors="replace").read().replace("\r", "").split("\n"):
    ln = ln.strip()
    if not ln or ln == hdr: continue
    if ln.startswith("#"): notes.append(ln); continue
    f = ln.split(",")
    try:
        if len(f) != 9: raise ValueError
        rows.append([int(x) for x in f])
    except ValueError:
        bad += 1
with open(out, "w") as fh:
    fh.write(hdr + "\n")
    for r in rows: fh.write(",".join(map(str, r)) + "\n")
n = len(rows)
if n < 2: print("samples", n, "bad lines", bad); sys.exit(1)
us = [r[1] for r in rows]
dts = [(b - a) & 0xFFFFFFFF for a, b in zip(us, us[1:])]
span = sum(dts)
print("samples", n, "bad lines", bad, "comment lines", len(notes))
print("seq first/last", rows[0][0], rows[-1][0], "missing seq", rows[-1][0] - rows[0][0] + 1 - n)
print("host clock: span %.3f s, rate %.3f Hz, spacing median %.0f us, max %.0f us, gaps over 15 ms: %d" % (span / 1e6, (n - 1) / (span / 1e6), statistics.median(dts), max(dts), sum(d > 15000 for d in dts)))
tk = [r[2] for r in rows]
d_tk = [(b - a) & 0xFFFFFF for a, b in zip(tk, tk[1:])]
if any(d_tk):
    tot = sum(d_tk) * 39.0625e-6
    print("sensor clock: span %.3f s, rate %.3f Hz, repeated samples (same sensor time): %d" % (tot, (n - 1) / tot, sum(d == 0 for d in d_tk)))
else:
    print("sensor clock: not moving (stand-in or no sensor time)")
for l in notes[:5]: print(l)
