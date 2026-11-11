#!/usr/bin/env python3
"""Print the per-board DIO tables (README) from components/cc27xx/pindata.json."""
import json, re
from pathlib import Path
d = json.loads((Path(__file__).parent / "components/cc27xx/pindata.json").read_text())
names = {"LP_EM_CC2755P10": "LP-EM-CC2755P10 (cc27xx)", "LP_EM_CC2340R5": "LP-EM-CC2340R5 (cc23xx)"}
for b, v in d.items():
    print(f"### {names[b]}\n")
    print("| DIO | Header pin | Already used on the board for | Analog in | PWM (timer, channel) | Other signals |")
    print("|---|---|---|---|---|---|")
    for k, p in sorted(v["pins"].items(), key=lambda x: int(x[0])):
        sig = p["signals"]
        adc = next((s for s in sig if re.fullmatch(r"ADC\d+", s)), "")
        pwm = ", ".join(f"LGPT{m.group(1)} ch{m.group(2)}" for s in sig if (m := re.fullmatch(r"LGPT(\d)C(\d)", s)))
        other = " ".join(s for s in sig if not re.fullmatch(r"ADC\d+|LGPT.*", s))
        print(f"| {k} | {p['header'] or 'not on the header'} | {', '.join(p['use']) or 'free'} | {adc} | {pwm} | {other} |")
    print()
