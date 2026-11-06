#!/usr/bin/env python3
"""Write the cost-matrix rows (tests motor_core_flash and motor_core_ram) into data/results/<board>.csv from sizes.csv.
Rows with these two tests are replaced, other rows are left alone. Core class to board: Cortex-M0+ -> mspm0g3519,
Cortex-M33 -> mspm33c321a, C28x -> f2800137. A row is the delta of two object builds, not a measurement on the board.
Usage: rows.py sizes.csv [--dry]"""
import csv, sys
from pathlib import Path
REPO = Path(__file__).resolve().parents[4]
COLS = ["board", "test", "variant", "status", "value", "value_hi", "runs", "clock_mhz", "conditions", "date", "source", "checked", "note"]
BOARD = {"m0p": "mspm0g3519", "m33": "mspm33c321a", "f28": "f2800137"}
CLOCK = {"m0p": "80", "m33": "160", "f28": "120"}
VER = {"gcc": "arm-none-eabi-gcc 14.3.1", "clang": "tiarmclang 5.1.1.LTS", "c2000": "cl2000 25.11.1.LTS"}
CORE = {"m0p": "cortex-m0plus", "m33": "cortex-m33 hard float fpv5-sp-d16", "f28": "c28x fpu32 eabi"}
FEAT = [("shell", "A", None), ("guard", "B", "A"), ("sixstep-hall", "C", "A"), ("foc-sector", "D", "A"), ("hall-tracker", "E", "D"), ("guard-t2", "F", "E")]
NOTE = {"shell": "stamps, sample and scale, PWM write; HAL setup not counted", "guard": "inline ISR check plus guard.o",
        "sixstep-hall": "Hall decode and the 6-entry tables, over the shell", "foc-sector": "Clarke, Park, 2 PI, inverse Park, min-max, sincos, angle from the Hall sector middle, over the shell",
        "hall-tracker": "edge tracker over the sector-angle FOC", "guard-t2": "guard over the Hall FOC with tracker"}
rows = list(csv.DictReader(open(sys.argv[1])))
size = {(r["toolchain"], r["core"], r["num"], r["trig"], r["variant"]): (int(r["text"]), int(r["data"]) + int(r["bss"]), r["commit"]) for r in rows}
out = {b: [] for b in BOARD.values()}
for (tc, core, num, trig, v), _ in sorted(size.items()):
    if v != "A": continue
    b = BOARD[core]
    for name, var, base in FEAT:
        a = size[(tc, core, num, trig, var)]; z = size[(tc, core, num, trig, base)] if base else (0, 0, a[2])
        cond = (f"delta of two object builds of portability/motor/tests/cost/isr_body.c (+core/guard.c) at commit {a[2]}; {VER[tc]}; -O2; {CORE[core]}; "
                f"num={num} trig={trig}; not linked, no startup or library helpers; core class only, no {b} board attached; flags in sizes.csv")
        for test, i in (("motor_core_flash", 0), ("motor_core_ram", 1)):
            out[b].append({"board": b, "test": test, "variant": f"{name}-{num}-{trig}-{tc}", "status": "measured", "value": a[i] - z[i], "value_hi": "",
                           "runs": 2, "clock_mhz": CLOCK[core], "conditions": cond, "date": "2026-10-19", "source": "portability/motor/tests/cost/RESULTS.md",
                           "checked": "build-log", "note": NOTE[name] + "; two builds, identical sizes"})
import io
for b, new in out.items():
    p = REPO / "data" / "results" / f"{b}.csv"
    lines = p.read_text().splitlines(keepends=True)
    kept = [l for l in lines if not l.startswith(f"{b},motor_core_")]   # other rows stay byte for byte
    buf = io.StringIO(); w = csv.DictWriter(buf, fieldnames=COLS, lineterminator="\n"); w.writerows(new)
    print(b, "keeps", len(kept) - 1, "adds", len(new))
    if "--dry" in sys.argv: continue
    if kept and not kept[-1].endswith("\n"): kept[-1] += "\n"
    p.write_text("".join(kept) + buf.getvalue())
