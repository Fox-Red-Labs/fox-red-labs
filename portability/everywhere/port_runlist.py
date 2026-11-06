#!/usr/bin/env python3
"""Turn the config-port results (data/crossbuild/mspm0_mspm33_port.jsonl) into the builds list tools/board_sweep/run_all.py
takes: one line per built image with ok, out, example, category, rtos, path.   port_runlist.py <out.jsonl>"""
import json, sys, os
HERE = os.path.dirname(os.path.abspath(__file__))
rows = [json.loads(l) for l in open(os.path.join(HERE, "data/crossbuild/mspm0_mspm33_port.jsonl")) if l.strip()]
n = 0
with open(sys.argv[1], "w") as f:
    for r in rows:
        if not r.get("ok") or not r.get("out"): continue
        cat, ex = r["src"].split(":")[1].split("/")
        f.write(json.dumps(dict(sdk="mspm0 example config-ported to mspm33", category=cat, example=ex, rtos="nortos", route="config-port",
                                path=f"/Applications/ti/mspm0_sdk_2_11_00_07/examples/nortos/LP_MSPM0G3519/{cat}/{ex}/ticlang", ok=True, out=r["out"])) + "\n"); n += 1
print(n, "images listed")
