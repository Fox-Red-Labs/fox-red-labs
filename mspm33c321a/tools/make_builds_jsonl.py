#!/usr/bin/env python3
"""Turn sweep/build.csv (mspm33c321a/tools/build_sweep.sh) into the builds.jsonl that tools/board_sweep/run_all.py reads.
An image counts as ok only when the .out exists and its map shows BCR_CONFIG and BSL_CONFIG with 0 bytes used
(the boot configuration in NONMAIN must never be written: plan, "Sweep lists")."""
import csv, glob, json, os, re, sys
R = os.path.abspath("out/mspm33c321a")
SDK = "/Applications/ti/mspm33_sdk_1_04_00_00/examples/nortos/LP_MSPM33C321A"
out = []
for row in csv.DictReader(open(R + "/sweep/build.csv")):
    if row["kind"] not in ("nortos", "rtos") or row["result"] != "built":
        continue
    d = R + "/sweep/" + (row["category"] + "/" + row["example"]).replace("/", "_")
    outs = glob.glob(d + "/ticlang/*.out") + glob.glob(d + "/freertos/ticlang/*.out")
    maps = glob.glob(d + "/ticlang/*.map") + glob.glob(d + "/freertos/ticlang/*.map")
    ok = bool(outs)
    why = ""
    for m in maps:
        for l in open(m, errors="replace"):
            t = l.split()
            if len(t) >= 4 and t[0] in ("BCR_CONFIG", "BSL_CONFIG") and re.fullmatch(r"[0-9a-f]{8}", t[3]) and int(t[3], 16) != 0:
                ok = False; why = "%s used %s" % (t[0], t[3])
    out.append(dict(sdk="mspm33_sdk_1_04_00_00", category=row["category"], example=row["example"], rtos=row["kind"], route="make",
                    path=SDK + "/" + row["category"] + "/" + row["example"], ok=ok, out=outs[0] if outs else None, first_error=why or None))
with open(R + "/sweep/builds.jsonl", "w") as f:
    for r in out:
        f.write(json.dumps(r) + "\n")
print(len(out), "images,", sum(r["ok"] for r in out), "ok;", [r["example"] for r in out if r["first_error"]])
