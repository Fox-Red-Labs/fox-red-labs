#!/usr/bin/env python3
"""Load images on the LP-AM263P and capture UART0 until a regex appears.
run_imgs.py <runs> <done_regex> <wait_s> <label>=<image.out> ...   logs: $LOGDIR/<label>-r<N>.txt (default out/am263p/logs/)"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import capload
LOG = os.environ.get("LOGDIR", os.path.abspath("out/am263p/logs")); os.makedirs(LOG, exist_ok=True)
n, rx, wait = int(sys.argv[1]), sys.argv[2], float(sys.argv[3])
for spec in sys.argv[4:]:
    label, img = spec.split("=", 1)
    for i in range(1, n + 1):
        text, info, lines = capload.run(img, rx, wait)
        open(f"{LOG}/{label}-r{i}.txt", "w").write("\n".join("%.3f %s" % x for x in lines) + "\n")
        print("==", label, i, "load_ok=%s" % info[0], "bytes", len(text), flush=True)
        print("\n".join(l for _, l in lines if l.strip())[-1500:], flush=True)
