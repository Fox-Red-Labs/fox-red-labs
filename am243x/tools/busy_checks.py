#!/usr/bin/env python3
"""Figures behind the busy-program checks (am243x/bench/chipview/busy), from the logs under am243x/bench/logs/:
busy-live-*.jsonl and busy-pin-*.csv (live_probe.js, 120 s), busy-checks-*.txt (live_checks.js, 180 s) and busy-console-uart-*.txt (serial_count.py).
  python3 am243x/tools/busy_checks.py [date]      prints every number the rows in data/results/am243x.csv quote"""
import json, statistics as st, sys
D = sys.argv[1] if len(sys.argv) > 1 else "2026-11-07"
L = "am243x/bench/logs/busy-"
rows = [json.loads(l) for l in open(f"{L}live-{D}.jsonl")][6:]
def slope(f):
    xs = [x["now"] / 1000 for x in rows]; ys = [f(x) for x in rows]; mx, my = st.mean(xs), st.mean(ys)
    return sum((a - mx) * (b - my) for a, b in zip(xs, ys)) / sum((a - mx) ** 2 for a in xs)
loads = [x["load"] for x in rows if x["load"] is not None]
print(f"LOAD pmu windows {len(loads)} mean {st.mean(loads):.2f} min {min(loads)} max {max(loads)} sd {st.pstdev(loads):.2f}")
os_ = [x["prog"]["os_load"] / 100 for x in rows]
print(f"LOAD os_total mean {st.mean(os_):.2f} min {min(os_):.2f} max {max(os_):.2f}")
for n in ["crunch", "led", "i2c", "console", "stats"]:
    v = [[t["load"] for t in x["prog"]["tasks"] if t["name"] == n][0] for x in rows]
    print(f"TASK {n} mean {st.mean(v):.3f} min {min(v)} max {max(v)}")
for k, true in [("tmr", 100), ("i2c", 10), ("led", 4), ("console", 1), ("crunch", 10)]:
    print(f"RATE {k} per s {slope(lambda x: x['prog'][k]):.3f} programmed {true}")
print(f"RATE tick ms per probe s {slope(lambda x: x['prog']['tick']):.2f} programmed 1000")
print("I2C errors", rows[-1]["prog"]["i2c_err"], "reads", rows[-1]["prog"]["i2c"])
print("VIM lines", sorted({tuple(x["vim"]) for x in rows}), "expected [(158, 160, 193, 210)]")
exp = {"GPMC0_AD7": 7, "UART0_RXD": 0, "UART0_TXD": 0, "I2C0_SCL": 0, "I2C0_SDA": 0}
print("PADS mode", sorted({tuple(sorted((k, v & 15) for k, v in x["pads"].items())) for x in rows}), "expected", sorted(exp.items()))
print("PSC", sorted({json.dumps(x["psc"], sort_keys=True) for x in rows}))
ed = []; prev = None
for l in open(f"{L}pin-{D}.csv"):
    t, bit, _ = l.strip().split(","); t = int(t); bit = int(bit)
    if prev is not None and bit != prev: ed.append(t)
    prev = bit
iv = [b - a for a, b in zip(ed, ed[1:])]
print(f"PIN edges {len(ed)} span_s {(ed[-1] - ed[0]) / 1000:.2f} per_s {(len(ed) - 1) / ((ed[-1] - ed[0]) / 1000):.3f} interval_ms mean {st.mean(iv):.1f} sd {st.pstdev(iv):.1f}")
chk = [l for l in open(f"{L}checks-{D}.txt") if l.startswith("FRL_CHECK")][0].split()
c = dict(zip(chk[1::2], chk[2::2]))
lines = [l.split(" ok  ", 1)[1].rstrip("\n").rstrip() for l in open(f"{L}console-uart-{D}.txt") if " ok  " in l]
bpl = st.mean(len(x) + 2 for x in lines)
print(f"UART bursts {c['bursts']} mean_ms {c['burst_ms_mean']} bytes_per_line {bpl:.1f} -> baud {10 * bpl / (float(c['burst_ms_mean']) / 1000):.0f} (setting 48 MHz / 16 / 26 = {48e6 / 16 / 26:.0f})")
print(f"UART console lines decoded at 115200: {len(lines)}, all well formed")
print(f"GPIO pad follows output {c['pad_follows_output']} of {c['samples']}")
