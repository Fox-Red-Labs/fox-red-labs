#!/usr/bin/env python3
"""Run CoreMark images on the LP-AM263P three times each and print the parsed results.
run_cm.py <runs> <label>=<image.out> ...   logs to $LOGDIR (default out/am263p/cm/)"""
import os, re, sys, hashlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import capload
LOG = os.environ.get("LOGDIR", os.path.abspath("out/am263p/cm")); os.makedirs(LOG, exist_ok=True)
n = int(sys.argv[1])
for spec in sys.argv[2:]:
    label, img = spec.split("=", 1)
    for i in range(1, n + 1):
        text, info, lines = capload.run(img, r"COREMARK END|All tests have passed|ERROR|Errors detected", 90)
        open(f"{LOG}/{label}-r{i}.txt", "w").write("\n".join("%.3f %s" % x for x in lines) + "\n")
        g = lambda k: (re.search(k + r"\s*:\s*([0-9.]+)", text) or [None, None])[1]
        ok = ("Correct operation validated" in text) or ("All tests have passed" in text)
        print(label, i, "load_ok=%s" % info[0], "iter/s", g("Iterations/Sec"), "secs", g("Total time \\(secs\\)"), "iters", g("Iterations"), "valid", ok, flush=True)
