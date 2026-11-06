#!/usr/bin/env python3
"""E38 bench: how many of the ESPHome Zigbee end device's counter reports reach Home Assistant, and how long each takes.

  python3 esp32c6/zigbee/report_bench.py --entity sensor.esphome_frl_zigbee_test_test_counter --serial-log <capture.txt> --n 20 --out log.txt

Run together with `esp32c6/bench/capture.py ... --no-reset --out <capture.txt>` on the same Mac. The device prints `count=N` just before it
publishes N (every 2 s); this script polls the entity through tools/ha.py every 20 ms and stamps the Mac time each new value first
appears. Latency = Mac time of first appearance minus the Mac time of the device's log line (both on this Mac's clock; the serial
line is stamped when the Mac reads it, so a few ms of error). Prints how many of N consecutive counts arrived and in order.
"""
import argparse, re, statistics, sys, time, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools")); import ha
a = argparse.ArgumentParser(); a.add_argument("--entity", required=True); a.add_argument("--seconds", type=float, default=60); a.add_argument("--out", required=True)
ns = a.parse_args(); out = open(ns.out, "w")
def log(s): line = f"[{time.strftime('%H:%M:%S')}.{int(time.time()*1000)%1000:03d}] {s}"; print(line, flush=True); out.write(line + "\n"); out.flush()
seen = {}; last = None; end = time.time() + ns.seconds
while time.time() < end:
    try: v = ha.req("GET", f"/api/states/{ns.entity}")[1]["state"]
    except Exception as e: log(f"poll error {e}"); time.sleep(0.5); continue
    if v != last:
        last = v
        try: seen[int(float(v))] = time.time(); log(f"value {v}")
        except ValueError: pass
    time.sleep(0.02)
log(f"distinct values seen {len(seen)}: {sorted(seen)[:3]}..{sorted(seen)[-3:]}")
out.close()
import json; json.dump({str(k): v for k, v in seen.items()}, open(ns.out + ".json", "w"))
