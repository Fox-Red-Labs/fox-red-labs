#!/usr/bin/env python3
"""Rerun the sweep's failed examples, each after a debugger System Reset (tools/sysreset.js), to separate a real failure
from peripheral state left by the example before it (a RAM load resets the CPU, not the peripherals).
Writes $OUT/sweep/recheck.jsonl and runs/<example>.recheck.txt."""
import json, os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import capload, sweep_run as sr
HERE = os.path.dirname(os.path.abspath(__file__))
out = []
for l in open(sr.RUNS):
    x = json.loads(l)
    if x["cls"] != "fail": continue
    r = subprocess.run(["/Applications/ti/ccs2101/ccs/scripting/run.sh", os.path.join(HERE, "sysreset.js")], capture_output=True, text=True, timeout=180)
    ok_reset = "FRL_SYSRESET done" in r.stdout
    img = os.path.join(sr.SW, x["example"].replace("/", "_") + ".out")
    text, info, lines = capload.run(img, r"All tests have passed|Some tests have failed|ASSERT", 30)
    body = "\n".join(t for _, t in lines if t.strip())
    open(os.path.join(sr.LOGS, x["example"].replace("/", "_") + ".recheck.txt"), "w").write(body + "\n")
    rec = {"example": x["example"], "first": x["cls"], "after_reset": sr.classify(body, info[0]), "reset_ok": ok_reset,
           "last": body.strip().splitlines()[-1][:160] if body.strip() else ""}
    out.append(rec); print(rec, flush=True)
open(os.path.join(sr.SW, "recheck.jsonl"), "w").write("".join(json.dumps(r) + "\n" for r in out))
