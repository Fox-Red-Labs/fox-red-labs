#!/usr/bin/env python3
"""analyze.py -> data/analysis.json: per board counts, built list, failures by cause, filtered by category.
For 'runtime filter' rows the reason is the sample's own `filter:` expression, read from its sample.yaml."""
import json, os, re, sys, collections, yaml
HERE = os.path.dirname(os.path.abspath(__file__))
ZB = os.path.expanduser(os.environ.get("ZP", "~/zephyrproject")) + "/zephyr"
rows = [json.loads(l) for l in open(f"{HERE}/data/results.jsonl")]
cache = {}
def filt(path, scen):
    key = (path, scen)
    if key in cache: return cache[key]
    out = None
    for f in ("sample.yaml", "testcase.yaml"):
        p = f"{ZB}/{path}/{f}"
        if os.path.exists(p):
            y = yaml.safe_load(open(p))
            t = (y.get("tests") or {}).get(scen) or {}
            c = y.get("common") or {}
            out = t.get("filter") or c.get("filter"); break
    cache[key] = out
    return out
def category(r):
    why = r["reason"]
    if why.startswith("Not in testsuite platform allow list"): return "sample lists other platforms only (platform_allow)"
    if why.startswith("No hardware support for"): return "board lacks a required peripheral or devicetree node"
    if why == "runtime filter":
        f = filt(r["path"], r["scenario"]) or "?"
        return "sample filter expression false: " + re.sub(r"\s+", " ", str(f))[:110]
    if "arch allow" in why: return "sample lists other architectures only"
    if why.startswith("Not enough") or "RAM" in why or "ROM" in why or "flash" in why.lower(): return "does not fit (min_ram/min_flash)"
    return why or "(none)"
out = {}
for r in rows:
    b = out.setdefault(r["board"], {"scenarios": 0, "built": 0, "error": 0, "filtered": 0, "skipped": 0,
        "built_scenarios": [], "built_samples": set(), "failed": [], "filtered_by_category": collections.Counter(),
        "filtered_samples": set(), "all_samples": set()})
    b["scenarios"] += 1; b["all_samples"].add(r["path"])
    s = r["status"]; b[s if s in b else "skipped"] += 1
    if s == "built":
        b["built_scenarios"].append(r["scenario"]); b["built_samples"].add(r["path"])
    elif s in ("error", "failed"):
        b["failed"].append({"scenario": r["scenario"], "path": r["path"], "first_error": r.get("first_error"), "tail": r.get("last_lines")})
    elif s == "filtered":
        b["filtered_by_category"][category(r)] += 1
    else:
        b.setdefault("other", []).append({"scenario": r["scenario"], "status": s, "reason": r["reason"]})
for b in out.values():
    b["built_samples"] = sorted(b["built_samples"]); b["all_samples"] = len(b["all_samples"])
    b["filtered_by_category"] = dict(b["filtered_by_category"].most_common())
    b.pop("filtered_samples")
json.dump(out, open(f"{HERE}/data/analysis.json", "w"), indent=1)
for k, b in out.items():
    print(k, {x: b[x] for x in ("scenarios", "built", "error", "filtered", "skipped")}, "samples built:", len(b["built_samples"]))
