#!/usr/bin/env python3
"""report.py results.jsonl -> JSON summary per board on stdout."""
import json, sys, collections
rows = [json.loads(l) for l in open(sys.argv[1])]
out = {}
for r in rows:
    b = out.setdefault(r["board"], {"counts": collections.Counter(), "filtered_by_reason": collections.Counter(),
                                    "failed": [], "sizes": {}})
    b["counts"][r["status"]] += 1
    if r["status"] == "filtered": b["filtered_by_reason"][r["reason"]] += 1
    if r["status"] in ("error", "failed"): b["failed"].append({"scenario": r["scenario"], "path": r["path"], "first_error": r.get("first_error")})
    if r["path"] in ("samples/hello_world", "samples/basic/blinky") and r["status"] == "built":
        b["sizes"][r["path"]] = {k: r.get(k) for k in ("flash_used", "flash_size", "ram_used", "ram_size", "sram_used") if r.get(k) is not None}
for b in out.values():
    b["counts"] = dict(b["counts"]); b["filtered_by_reason"] = dict(b["filtered_by_reason"])
    b["total_scenarios"] = sum(b["counts"].values())
print(json.dumps(out, indent=1))
