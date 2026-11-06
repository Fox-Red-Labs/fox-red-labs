#!/usr/bin/env python3
"""Turn build.sh's sizes.csv into feature deltas (flash = text with rodata, RAM = data + bss) and print markdown tables.
Usage: summarize.py sizes.csv [--rows]   --rows also prints data/results rows (board-class columns, see RESULTS.md)."""
import csv, sys
rows = list(csv.DictReader(open(sys.argv[1])))
key = {(r["toolchain"], r["core"], r["num"], r["trig"], r["variant"]): (int(r["text"]), int(r["data"]) + int(r["bss"])) for r in rows}
CFG = [("m0p", "iq24", "table"), ("m0p", "q15", "table"), ("m33", "float", "table"), ("m33", "float", "libm"), ("m33", "iq24", "table"), ("f28", "float", "table"), ("f28", "iq24", "table")]
TC = {"m0p": ["gcc", "clang"], "m33": ["gcc", "clang"], "f28": ["c2000"]}
FEAT = [("Shell (A)", "A", None), ("Guard (B-A)", "B", "A"), ("Six-step Hall (C-A)", "C", "A"), ("FOC, sector angle (D-A)", "D", "A"), ("Edge tracker (E-D)", "E", "D"),
        ("Guard on T2 (F-E)", "F", "E")]
def get(tc, c, v): return key.get((tc, c[0], c[1], c[2], v))
def delta(tc, c, v, base):
    a = get(tc, c, v); b = get(tc, c, base) if base else (0, 0)
    return (a[0] - b[0], a[1] - b[1]) if a and b else None
out = []
for c in CFG:
    for tc in TC[c[0]]:
        out.append((c, tc))
print("| Feature | " + " | ".join(f"{c[0]} {c[1]}/{c[2]} {tc}" for c, tc in out) + " |")
print("|---|" + "---|" * len(out))
for name, v, base in FEAT:
    cells = []
    for c, tc in out:
        d = delta(tc, c, v, base)
        cells.append("n/a" if d is None else f"{d[0]} / {d[1]}")
    print(f"| {name} | " + " | ".join(cells) + " |")
print("\nCells are flash bytes / RAM bytes. Flash = text with read-only data; RAM = data + bss; objects, not linked.")
if "--rows" in sys.argv:
    print("\nraw sizes (text, ram):")
    for r in rows: print(r["toolchain"], r["core"], r["num"], r["trig"], r["variant"], r["text"], int(r["data"]) + int(r["bss"]))
