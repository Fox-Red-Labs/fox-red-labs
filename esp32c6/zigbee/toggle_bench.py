#!/usr/bin/env python3
"""Zigbee join and toggle bench through Home Assistant's ZHA (same method as cc2340r5/zigbee/toggle-bench.txt).

  python3 esp32c6/zigbee/toggle_bench.py join   --out log.txt            open ZHA for 180 s, wait for a new light.* entity
  python3 esp32c6/zigbee/toggle_bench.py toggle --entity light.x --n 20 --out log.txt

join: snapshots light.* entities, calls zha.permit (duration 180), then polls /api/states every 0.5 s and stamps the
first new light entity (host time); the flash-end stamp comes from the serial capture, so join = entity time minus it.
toggle: alternates light.turn_on / light.turn_off, timing the REST service call and the time until the entity state
reads the new value (polled every 5 ms), one toggle a second; prints the same table as the CC2340R5 run.
Uses tools/ha.py's req() (URL and token from ~/.claude.json, never printed).
"""
import argparse, statistics, sys, time, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools")); import ha
a = argparse.ArgumentParser(); a.add_argument("mode", choices=["join", "toggle"]); a.add_argument("--entity"); a.add_argument("--n", type=int, default=20); a.add_argument("--out", required=True)
ns = a.parse_args(); out = open(ns.out, "w")
def log(s): line = f"[{time.strftime('%H:%M:%S')}.{int(time.time()*1000)%1000:03d}] {s}"; print(line, flush=True); out.write(line + "\n"); out.flush()
def lights(): return {s["entity_id"]: s for s in ha.req("GET", "/api/states")[1] if s["entity_id"].startswith("light.")}
if ns.mode == "join":
    before = set(lights()); log(f"{len(before)} light entities before"); ms, r = ha.req("POST", "/api/services/zha/permit", {"duration": 180}); log(f"zha.permit 180 s called ({ms} ms)")
    end = time.time() + 200
    while time.time() < end:
        new = [e for e in lights() if e not in before]
        if new: log(f"NEW ENTITY {new} "); break
        time.sleep(0.5)
    else: log("no new light entity in 200 s")
else:
    e = ns.entity; rows = []
    for i in range(1, ns.n + 1):
        want = "on" if i % 2 else "off"; t0 = time.time()
        ms, _ = ha.req("POST", f"/api/services/light/turn_{want}", {"entity_id": e}); t1 = time.time()
        ok = False
        while time.time() - t0 < 5:
            if ha.req("GET", f"/api/states/{e}")[1]["state"] == want: ok = True; break
            time.sleep(0.005)
        t2 = time.time(); rows.append((round((t1-t0)*1000), round((t2-t0)*1000), ok))
        log(f"{i:2d} -> {want:3s} call {rows[-1][0]:6d} ms  state {rows[-1][1]:6d} ms  {'ok' if ok else 'FAIL'}")
        time.sleep(max(0, 1.0 - (time.time() - t0)))
    st = [r[1] for r in rows if r[2]]; fails = sum(1 for r in rows if not r[2])
    log(f"{len(rows)} toggles, {fails} failed. Service call ms: median {statistics.median(r[0] for r in rows):.0f}. HA state settled ms: median {statistics.median(st) if st else 'n/a'}, min {min(st) if st else 'n/a'}, max {max(st) if st else 'n/a'}")
out.close()
