#!/usr/bin/env python3
"""Summarize data/*.jsonl: built counts, size ratios, failures by cause. Prints markdown tables."""
import json, glob, statistics as st, collections, re, sys, os
here = os.path.dirname(os.path.abspath(__file__))
rows = collections.defaultdict(dict)
for f in glob.glob(here + "/data/*.jsonl"):
    for l in open(f):
        r = json.loads(l); rows[(r["board"], r["example"])][r["toolchain"]] = r
def cause(r):
    if r.get("cause"): return r["cause"]
    e = r.get("first_error") or ""
    if "undefined reference" in e: return "undefined_reference"
    if "No such file" in e: return "missing_file"
    if "error:" in e: return "compile_error"
    return "other"
out = {}
for b in sorted({k[0] for k in rows}):
    ex = {k[1]: v for k, v in rows.items() if k[0] == b}
    gcc_total = len(ex); has = [e for e, v in ex.items() if v["gcc"].get("cause") != "no_makefile_shipped"]
    ok = [e for e in has if ex[e]["gcc"]["built"]]
    both = [e for e in ok if ex[e].get("ticlang", {}).get("built")]
    ti_ok = [e for e in ex if ex[e]["ticlang"]["built"]]
    print(f"\n## {b}: examples {gcc_total}, with gcc makefile {len(has)}, GCC built {len(ok)}, ticlang built {len(ti_ok)}, both {len(both)}")
    rat = [ex[e]["gcc"]["text"] / ex[e]["ticlang"]["text"] for e in both]
    ft = [ex[e]["gcc"]["text"] + ex[e]["gcc"]["data"] for e in both]
    tt = [ex[e]["ticlang"]["text"] + ex[e]["ticlang"]["data"] for e in both]
    fl = [a / c for a, c in zip(ft, tt)]
    if rat: print(f"text ratio gcc/ticlang: median {st.median(rat):.2f} min {min(rat):.2f} max {max(rat):.2f}; flash(text+data) median {st.median(fl):.2f}")
    d = [ex[e]["gcc"]["text"] - ex[e]["ticlang"]["text"] for e in both]; print("median text delta bytes", st.median(d))
    print("gcc bss median", st.median(ex[e]["gcc"]["bss"] for e in both), "ticlang bss median", st.median(ex[e]["ticlang"]["bss"] for e in both))
    fails = collections.defaultdict(list)
    for e in has:
        if not ex[e]["gcc"]["built"]: fails[cause(ex[e]["gcc"])].append(e)
    for c, l in fails.items():
        print(f"- gcc {c}: {len(l)}: {', '.join(l)}\n    e.g. {ex[l[0]]['gcc']['first_error']}")
    for e in ex:
        if not ex[e]["ticlang"]["built"]: print(f"- ticlang fail {e}: {ex[e]['ticlang'].get('first_error')}")
    print("no gcc makefile:", [e for e in ex if e not in has])
