#!/usr/bin/env python3
"""Flash each image in images.txt to the LP-MSPM0L1306 (probe ML130001, tty /dev/cu.usbmodemML1300011)
and capture its UART report until BENCH DONE. Writes results/<image>.txt and appends one JSON line per
image to results/runs.jsonl.   run_bench_l1306.py [images.txt] [timeout_s]
Copy of the G3507 run_bench.py. It reuses that suite's example-sweep/run_one.py helpers and points them at
the L1306 probe, serial port, OpenOCD config and memory map. NOT RUN by the agent that wrote it."""
import json, os, sys, time
ORIG = os.path.expanduser("<repo>/mspm0g3507")
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ORIG + "/example-sweep")
import run_one as ro
ro.PROBE = "ML130001"
ro.TTY = "/dev/cu.usbmodemML1300011"
ro.OCD_CFG = HERE + "/openocd_l1306.cfg"
ro.SAFE = [(0x00000000, 0x00010000), (0x20000000, 0x20001000)]   # MAIN flash 64 KB, SRAM 4 KB (not NONMAIN)

lst = sys.argv[1] if len(sys.argv) > 1 else HERE + "/images.txt"
limit = float(sys.argv[2]) if len(sys.argv) > 2 else 180
os.makedirs(HERE + "/results", exist_ok=True)
for out in [l.strip() for l in open(lst) if l.strip()]:
    name = os.path.basename(out)[:-4]
    if not ro.is_safe(ro.segments(out)):
        print(name, "refused: data outside MAIN flash or SRAM"); continue
    flog, fsecs, _ = ro.ocd(["reset halt", f"program {{{out}}} verify"], timeout=90)
    ok = "** Verified OK **" in flog
    u = ro.Uart(115200); u.start(); time.sleep(0.3)
    t0 = time.time()
    ro.ocd(["reset run"], timeout=30)
    while time.time() - t0 < limit and b"BENCH DONE" not in u.buf:
        time.sleep(0.5)
    u.stop = True; u.join(1)
    text = u.buf.decode(errors="replace").replace("\r", "")
    i = text.rfind("BENCH START"); text = text[i:] if i >= 0 else text
    open(f"{HERE}/results/{name}.txt", "w").write(text)
    rows = [l.split(",", 5) for l in text.splitlines() if l.startswith("R,")]
    rec = {"image": name, "flashed": ok, "flash_secs": round(fsecs, 1), "done": "BENCH DONE" in text,
           "secs": round(time.time() - t0, 1), "rows": len(rows), "fails": [r[1] + "/" + r[2] for r in rows if len(r) > 4 and r[4] != "ok"]}
    open(HERE + "/results/runs.jsonl", "a").write(json.dumps(rec) + "\n")
    print(json.dumps(rec), flush=True)
