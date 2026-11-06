import json, glob, os, re, statistics as st, collections
here = os.path.dirname(os.path.abspath(__file__)); d = os.path.join(here, "data")
def load(tc, b):
    p = f"{d}/{tc}-{b}.jsonl"
    return {r["example"]: r for r in map(json.loads, open(p))} if os.path.exists(p) else {}
def klass(r):
    e = r.get("first_error") or ""
    for pat, name in [(r"No such file or directory", "missing file"), (r"undefined reference", "link: undefined reference"),
                      (r"unknown (type|argument)|unrecognized|invalid", "compiler flag or syntax"),
                      (r"cannot find -l|cannot find", "link: library not found"), (r"DeviceFamily", "DeviceFamily undefined"),
                      (r"No rule to make target", "make: missing source")]:
        if re.search(pat, e, re.I): return name
    return "other"
tot = collections.Counter()
for b in ["LP_EM_CC2340R5", "LP_EM_CC2755P10"]:
    g, t = load("gcc", b), load("ticlang", b)
    shipg = {e: r for e, r in g.items() if r.get("cause") != "no_makefile_shipped"}
    shipt = {e: r for e, r in t.items() if r.get("cause") != "no_makefile_shipped"}
    gok = {e for e, r in shipg.items() if r["built"]}; tok = {e for e, r in shipt.items() if r["built"]}
    print(f"\n== {b}: examples {len(g)}, ship gcc makefile {len(shipg)}, gcc built {len(gok)}, ticlang makefile {len(shipt)}, ticlang built {len(tok)}")
    for k in ("rtos", "nortos"):
        sg = [e for e in shipg if e.startswith(k)]
        print(f"   {k}: gcc built {sum(shipg[e]['built'] for e in sg)} of {len(sg)}")
    print("   gcc failures:")
    for e, r in sorted(shipg.items()):
        if not r["built"]: print("    ", klass(r), e, "|", r["first_error"])
    both = sorted(gok & tok)
    for field in ("text", "data", "bss"):
        pass
    rat = [shipg[e]["text"] / shipt_e for e in both if (shipt_e := t[e]["text"])]
    if rat:
        q = st.quantiles(rat, n=4)
        diff = [shipg[e]["text"] - t[e]["text"] for e in both]
        print(f"   text GCC/ticlang over {len(both)}: median {st.median(rat):.2f} q1 {q[0]:.2f} q3 {q[2]:.2f} min {min(rat):.2f} max {max(rat):.2f}; GCC larger in {sum(x>0 for x in diff)}, smaller {sum(x<0 for x in diff)}; median diff {st.median(diff):.0f} B")
        for k in ("rtos", "nortos"):
            rk = [shipg[e]["text"] / t[e]["text"] for e in both if e.startswith(k)]
            if rk: print(f"   {k}: n {len(rk)} median {st.median(rk):.2f}")
        print("   data median gcc/ticlang:", st.median(shipg[e]["data"] for e in both), st.median(t[e]["data"] for e in both),
              " bss:", st.median(shipg[e]["bss"] for e in both), st.median(t[e]["bss"] for e in both))
        rs = sorted(both, key=lambda e: shipg[e]["text"] / t[e]["text"])
        print("   smallest ratios:", [(e, round(shipg[e]["text"]/t[e]["text"],2)) for e in rs[:3]])
        print("   largest ratios:", [(e, round(shipg[e]["text"]/t[e]["text"],2)) for e in rs[-4:]])
    print("   gcc warnings total", sum(r.get("warnings", 0) for r in shipg.values()), "in", sum(1 for r in shipg.values() if r.get("warnings")), "examples")
    print("   built gcc secs median", st.median(r["secs"] for r in shipg.values() if r["built"]) if gok else None)
    print("   ticlang failures:", [(e, r["first_error"]) for e, r in shipt.items() if not r["built"]][:10])
    print("   gcc built but ticlang not / ticlang missing:", sorted(gok - tok)[:10])
    print("   no gcc folder but ticlang:", sorted(e for e, r in g.items() if r.get("cause") == "no_makefile_shipped")[:60])
