#!/usr/bin/env python3
"""E105: toggle the ESPHome switch from Home Assistant and time it. Optionally read the LED pin (DIO16) from the chip.
  python3 ports/esphome-ti/toggle_bench.py --entity switch.esphome_frl_fit_test --n 20 --out log.txt [--pin-ccxml CCXML --pin-every 4]
Times the REST call and the wait until the entity state reads the new value (polled every 5 ms), one toggle a second.
With --pin-ccxml, every --pin-every-th toggle is followed 1.5 s later by a DSLite read of GPIO DOUT31_0 (0x40023200) bit 16."""
import argparse, statistics, subprocess, struct, sys, time, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools")); import ha
a = argparse.ArgumentParser(); a.add_argument("--entity", required=True); a.add_argument("--n", type=int, default=20); a.add_argument("--out", required=True)
a.add_argument("--pin-ccxml"); a.add_argument("--pin-bit", type=int, default=16); a.add_argument("--pin-every", type=int, default=4); ns = a.parse_args(); out = open(ns.out, "w")
def log(s): line = f"[{time.strftime('%H:%M:%S')}.{int(time.time()*1000)%1000:03d}] {s}"; print(line, flush=True); out.write(line + "\n"); out.flush()
def pin():
    subprocess.run(["/Applications/ti/uniflash_9.6.0/dslite.sh", "--mode", "memory", "-c", ns.pin_ccxml, "-r", "0x40023200,1", "-o", "/tmp/dout_e105.bin"], capture_output=True)
    return (struct.unpack("<I", open("/tmp/dout_e105.bin", "rb").read()[:4])[0] >> ns.pin_bit) & 1
rows = []; pins = []
for i in range(1, ns.n + 1):
    want = "on" if i % 2 else "off"; t0 = time.time()
    ha.req("POST", f"/api/services/switch/turn_{want}", {"entity_id": ns.entity}); t1 = time.time(); ok = False
    while time.time() - t0 < 5:
        if ha.req("GET", f"/api/states/{ns.entity}")[1]["state"] == want: ok = True; break
        time.sleep(0.005)
    t2 = time.time(); rows.append((round((t1-t0)*1000), round((t2-t0)*1000), ok)); extra = ""
    if ns.pin_ccxml and i % ns.pin_every == 0:
        time.sleep(1.5); p = pin(); pins.append(p == (1 if want == "on" else 0)); extra = f"  pin DIO{ns.pin_bit}={p} {'match' if pins[-1] else 'MISMATCH'}"
    log(f"{i:2d} -> {want:3s} call {rows[-1][0]:5d} ms  state {rows[-1][1]:5d} ms  {'ok' if ok else 'FAIL'}{extra}")
    time.sleep(max(0, 1.0 - (time.time() - t0)))
st = [r[1] for r in rows if r[2]]; fails = sum(1 for r in rows if not r[2])
log(f"{len(rows)} toggles, {fails} failed. Service call ms: median {statistics.median(r[0] for r in rows):.0f}. HA state settled ms: median {statistics.median(st) if st else 'n/a'}, min {min(st) if st else 'n/a'}, max {max(st) if st else 'n/a'}. Pin reads {sum(pins)} of {len(pins)} matched")
