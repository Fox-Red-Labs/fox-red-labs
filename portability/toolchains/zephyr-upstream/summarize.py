#!/usr/bin/env python3
"""summarize.py <twister outdir> <board> <group> <jsonl out> <log tar dir>
One jsonl row per (scenario, board) from twister.json plus the build.log of every built scenario
(flash/RAM use, first error). Compresses the build logs of built scenarios."""
import json, sys, re, glob, os, gzip
outdir, board, group, out, logdir = sys.argv[1:6]
jp = os.path.join(outdir, "twister.json")
if not os.path.exists(jp):
    print(board, group, "NO twister.json"); sys.exit(0)
d = json.load(open(jp))
ERR = re.compile(r"(CMake Error.*|.*(?:error:|Error:|FAILED:|fatal error|overflowed by|will not fit|undefined reference).*)")
MEM = re.compile(r"^\s*(FLASH|RAM|SRAM|IDT_LIST|ROM):\s+(\d+) (B|KB|MB)\s+(\d+) (B|KB|MB)\s+([\d.]+)%", re.M)
U = {"B": 1, "KB": 1024, "MB": 1048576}
rows, counts, enospc = [], {}, 0
os.makedirs(logdir, exist_ok=True)
for t in d["testsuites"]:
    r = {"board": board, "group": group, "scenario": t["name"], "path": t["path"],
         "status": t["status"], "reason": (t.get("reason") or "")[:300]}
    if t["status"] == "not run": r["status"] = "built"
    bl = glob.glob(os.path.join(outdir, "*", "*", t["path"], t["name"], "build.log"))
    if bl:
        log = open(bl[0], errors="replace").read()
        for m in MEM.finditer(log):
            k = m.group(1).lower()
            r[k + "_used"] = int(float(m.group(2)) * U[m.group(3)])
            r[k + "_size"] = int(float(m.group(4)) * U[m.group(5)])
        if re.search(r"No space left|Cannot open file for write|write error|Disk quota", log):
            enospc += 1; r["enospc"] = True
        if r["status"] in ("error", "failed"):
            es = [m.group(0).strip() for m in ERR.finditer(log)]
            r["first_error"] = (es[0] if es else r["reason"])[:300]
            r["last_lines"] = log.strip().splitlines()[-6:]
        name = f"{board}__{t['name']}__{t['path'].replace('/', '_')}.log.gz"
        with gzip.open(os.path.join(logdir, name), "wt") as g:
            g.write(log if r["status"] != "built" else "\n".join(log.splitlines()[-60:]))
    elif r["status"] in ("error", "failed"):
        r["first_error"] = r["reason"]
    rows.append(r)
    counts[r["status"]] = counts.get(r["status"], 0) + 1
if enospc:
    print("ENOSPC", board, group, enospc, flush=True); sys.exit(3)
with open(out, "a") as f:
    for r in rows: f.write(json.dumps(r) + "\n")
print(board, group, counts, flush=True)
