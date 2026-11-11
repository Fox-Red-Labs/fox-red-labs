#!/usr/bin/env python3
"""Write site/flash/bench/boosterpacks.json: the plug-on boards the no-code controls card can pick for the LP-MSPM0L1306.

  python3 tools/webflash/build_boosterpacks.py

Names, fit notes and what sits on the bench come from data/parts.json (entries with cat "plugon"); the header pins come from
docs/boards/<id>.json (read 2026-10-17). Each pick lists the rows it adds and the answer to expect. Every pick is "predicted" until
a run on the L1306 with that board plugged in is recorded: `tested_here` stays false and the page says "Predicted, not tested here".
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
_d = json.loads((ROOT / "data/parts.json").read_text())
parts = {p["id"]: p for p in (_d.get("parts", _d) if isinstance(_d, dict) else _d)}

PICKS = [
    {"id": "bp-bassensorsmkii", "fit": "Plugs on; I2C is on pins 9 and 10, the L1306's PA1 and PA0. The OPT3001 and TMP117 are powered from pins 27 and 28, which the L1306 does not have: two wires (27 to pin 12, PA23; 28 to pin 18, PA7) and seven jumpers out.",
     "pins": "docs/boards/bp-bassensorsmkii.json: I2C 9, 10 (10 kOhm pull-ups on the board); pin 27 powers the OPT3001 and 28 the TMP117 (high = on); 6 and 24 enable the HDC2080 and DRV5055 (low = on).",
     "rows": [
         {"kind": "pin", "pin": 23, "mode": "high", "why": "pin 27 by wire to PA23 (header pin 12): powers the OPT3001"},
         {"kind": "scan", "why": "who answers on the shared bus", "expect": "0x40 (HDC2080), 0x44 (OPT3001), 0x69 (BMI160), and 0x13 (BMM150) once awake; 0x48 only with the ribbon cable in and pin 28 high"},
         {"kind": "read", "addr": 0x44, "reg": 0x7E, "n": 2, "why": "OPT3001 manufacturer ID", "expect": "54 49 (0x5449, Texas Instruments)"},
         {"kind": "read", "addr": 0x40, "reg": 0xFC, "n": 2, "why": "HDC2080 manufacturer ID", "expect": "49 54 (0x5449, low byte first)"},
     ]},
    {"id": "boostxl-edumkii", "fit": "Plugs on with pins 27 to 34 not connected: no screen (pin 31), no S1 or S2. The light sensor is on I2C pins 9 and 10. Jumpers J4 to J6, J1, J11, J3, J12, J13 and J2 come out on the L1306 (our arrival plan).",
     "pins": "docs/boards/boostxl-edumkii.json: I2C 9, 10 (OPT3001 at 0x44, alert on 8); PWM 19, 37 to 40: servo, blue, green, red, buzzer.",
     "rows": [
         {"kind": "scan", "why": "who answers on the BoosterPack's bus", "expect": "0x44 only (OPT3001)"},
         {"kind": "read", "addr": 0x44, "reg": 0x7E, "n": 2, "why": "OPT3001 manufacturer ID", "expect": "54 49 (0x5449)"},
         {"kind": "pwm", "pin": 27, "hz": 1000, "duty": 25, "why": "header pin 37 drives the BoosterPack's blue LED (L1306 jumper J12 out)"},
     ]},
    {"id": "boostxl-sharp128", "fit": "Plugs on with no wires. The screen speaks SPI (pins 7 and 15, select on 6), which this card cannot drive; what it can show is the display's power and on/off pins.",
     "pins": "docs/boards/boostxl-sharp128.json: SPI 7, 15, select 6; GPIO 2 power and 5 on/off; SD card 8, 14, 17.",
     "rows": [
         {"kind": "pin", "pin": 24, "mode": "high", "why": "header pin 5 is the display's on/off line (L1306 jumper J5 joins PA24 to the light sensor; pull it)"},
         {"kind": "scan", "why": "the display has no I2C part", "expect": "nothing answers (the L1306's own pull-ups only)"},
     ]},
]
out = []
for p in PICKS:
    part = parts[p["id"]]
    assert part["cat"] == "plugon", p["id"]
    assert (ROOT / "docs/boards" / (p["id"] + ".json")).is_file(), p["id"]
    out.append({"id": p["id"], "name": part["name"], "fit": p["fit"], "pins": p["pins"], "rows": p["rows"], "tested_here": False,
                "bench": part["status"],
                "on_this_board": False,
                "note": "Predicted, not tested here: " + ("the Sensors BoosterPack is stacked on the LP-MSPM0G5187, not on the L1306." if p["id"] == "bp-bassensorsmkii" else
                        "the Education BoosterPack is stacked on the LP-MSPM33C321A, not on the L1306." if p["id"] == "boostxl-edumkii" else
                        "the display BoosterPack has not been plugged onto the L1306 yet.")})
(ROOT / "site/flash/bench/boosterpacks.json").write_text(json.dumps({"made_from": ["data/parts.json", "docs/boards/<id>.json"], "board": "mspm0l1306", "packs": out}, indent=1, ensure_ascii=False) + "\n")
print("OK", [o["id"] for o in out])
