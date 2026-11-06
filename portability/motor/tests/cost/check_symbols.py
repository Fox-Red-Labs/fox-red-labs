#!/usr/bin/env python3
"""A feature-off build must not contain the feature's data: six-step tables absent from A, B, D, E; the sine table absent
from A, B, C; the Hall angle table (TAB) absent from A, B, C; the guard state (G) absent from every build without the guard.
Usage: check_symbols.py <out dir of build.sh>"""
import glob, os, subprocess, sys
out = sys.argv[1]
C28NM = "/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS/bin/nm2000"
RULES = {"frl_sixstep_fwd": "ABDE", "frl_sintab_q24": "ABC", "TAB": "ABC", "G": "ACDE", "GCFG": "ACDE"}
PRESENT = {"frl_sixstep_fwd": "C", "frl_sintab_q24": "DEF", "TAB": "DEF", "G": "BF", "GCFG": "BF"}   # positive controls
bad = n = 0
for d in sorted(glob.glob(os.path.join(out, "*-?"))):
    var = d[-1]; objs = glob.glob(d + "/*.o") + glob.glob(d + "/*.obj")
    nm = C28NM if d.split("/")[-1].startswith("c2000") else "arm-none-eabi-nm"
    syms = set()
    for o in objs:
        for line in subprocess.run([nm, o], capture_output=True, text=True).stdout.splitlines():
            p = line.split()
            if len(p) >= 2: syms.add(p[-1].split(".")[-1] if p[-1].startswith(".") else p[-1])
    for sym, absent_in in RULES.items():
        n += 1
        if var in absent_in and sym in syms:
            bad += 1; print("PRESENT in", d, sym)
        n += 1
        libm = "-libm-" in d + "-"
        if sym == "frl_sintab_q24" and libm:                  # the libm backend carries no table in any variant
            if sym in syms: bad += 1; print("PRESENT in", d, sym)
        elif var in PRESENT[sym] and sym not in syms:
            bad += 1; print("MISSING in", d, sym)
print(f"symbol checks: {n - bad} of {n} hold" + ("" if not bad else f", {bad} violated"))
sys.exit(1 if bad else 0)
