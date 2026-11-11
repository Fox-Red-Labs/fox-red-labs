#!/usr/bin/env python3
"""Build components/cc27xx/pindata.json: every DIO of the LP-EM-CC2755P10 and LP-EM-CC2340R5, what it reaches on the
BoosterPack header, what the board already uses it for, and which peripheral signals the chip can route to it.

Sources (all TI, nothing measured here):
  * the board description in the SimpleLink Low Power F3 SDK (source/ti/boards/.meta/<board>.syscfg.json: header pin
    numbers, LEDs, buttons, backchannel UART, SPI flash, SWD),
  * SysConfig's device pin-mux data for the chip (deviceData/<chip>/<chip>.json): which signals each DIO can carry,
  * cross-checked by hand against the LP-EM-CC2340R5 quick start guide (SWRU588A) and the CC2755x10 data sheet (SWRS306E).
Usage: gen_pins.py [SDK dir] [SysConfig dir]   (writes next to components/cc27xx/pindata.json)
"""
import json, re, sys
from pathlib import Path

SDK = Path(sys.argv[1] if len(sys.argv) > 1 else "/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS")
SYS = Path(sys.argv[2] if len(sys.argv) > 2 else "/Applications/ti/sysconfig_1.27.0")
OUT = Path(__file__).parent / "components" / "cc27xx" / "pindata.json"

BOARDS = {"LP_EM_CC2755P10": "CC2755P105RHA", "LP_EM_CC2340R5": "CC2340R5RKP"}


def jsonc(path):
    text = path.read_text()
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"(?m)^\s*//.*$", "", text)
    text = re.sub(r'(?<=[,\d"\]\}])\s*//[^\n"]*$', "", text, flags=re.M)
    return json.loads(text)


def dio_of(name):
    m = re.match(r"DIO(\d+)", name)
    return int(m.group(1)) if m else None


out = {}
for board, chip in BOARDS.items():
    dev = json.loads((SYS / f"dist/deviceData/{chip}/{chip}.json").read_text())
    dp, pp = dev["devicePins"], dev["peripheralPins"]
    funcs, label = {}, {}
    for m in dev["muxes"]:
        n = dp[m["devicePinID"]]["name"]
        d = dio_of(n)
        if d is None:
            continue
        label[d] = n
        funcs[d] = sorted({pp[s["peripheralPinID"]]["name"] for s in m["muxSetting"]
                           if not re.match(r"(GPIO|DTB|LRFD|LPC|LPCO|LFCI|LFXT|AREF|CKMIN|EXTCI|SWO)", pp[s["peripheralPinID"]]["name"])})
    b = jsonc(SDK / "source/ti/boards/.meta" / f"{board}.syscfg.json")
    pkg = next(iter(dev["packages"].values()))["packagePin"]
    ball2dio = {p["ball"]: dio_of(dp[p["devicePinID"]]["name"]) for p in pkg if dio_of(dp[p["devicePinID"]]["name"]) is not None}
    header = {}
    for h in b["headers"]:
        for p in h["pins"]:
            d = dio_of(p["name"]) if p["name"].startswith("DIO") else ball2dio.get(str(p.get("ball")))
            if d is not None:
                header[d] = p["number"]
    use = {}
    comp = b["components"]
    def conn(name, key):
        c = comp.get(name, {}).get("connections", {})
        return c.get(key)
    # the board file names header balls, not DIOs, in some places: map ball -> DIO through the package table
    pkg = next(iter(dev["packages"].values()))["packagePin"]
    ball2dio = {p["ball"]: dio_of(dp[p["devicePinID"]]["name"]) for p in pkg if dio_of(dp[p["devicePinID"]]["name"]) is not None}
    def mark(ball, what):
        if ball is None:
            return
        d = ball2dio.get(str(ball))
        if d is not None:
            use.setdefault(d, []).append(what)
    mark(conn("LED_RED", "OUTPUT"), "red LED")
    mark(conn("LED_GREEN", "OUTPUT"), "green LED")
    mark(conn("BTN-1", "INPUT"), "button BTN-1")
    mark(conn("BTN-2", "INPUT"), "button BTN-2")
    mark(conn("XDS110UART", "TXD"), "backchannel UART: chip TX")
    mark(conn("XDS110UART", "RXD"), "backchannel UART: chip RX")
    sub = comp.get("MX25R8035F", {}).get("subComponents", {})
    mark(sub.get("SELECT", {}).get("connections", {}).get("CSN"), "SPI flash chip select")
    for k, w in (("SCLK", "SPI flash and BoosterPack SPI clock"), ("PICO", "SPI flash and BoosterPack SPI data out"), ("POCI", "SPI flash and BoosterPack SPI data in")):
        mark(comp.get("LP_SPI_BUS", {}).get("connections", {}).get(k), w)
    mark(comp.get("LP_SPI", {}).get("subComponents", {}).get("SELECT", {}).get("connections", {}).get("CSN"), "BoosterPack SPI chip select")
    for d, n in label.items():
        if "SWD" in n:
            use.setdefault(d, []).append("SWD (debug)")
        if "X32" in n:
            use.setdefault(d, []).append("32 kHz crystal pad")
    pins = {}
    for d in sorted(label):
        pins[str(d)] = dict(name=label[d], header=header.get(d), use=use.get(d, []), signals=funcs.get(d, []))
    out[board] = dict(chip=chip, pins=pins)
OUT.write_text(json.dumps(out, indent=1, sort_keys=True) + "\n")
print("wrote", OUT)
for bd, v in out.items():
    print(bd, len(v["pins"]), "DIOs")
    for d, p in v["pins"].items():
        print(f"  DIO{d:>2} header {p['header']!s:>4}  {', '.join(p['use']) or '-':45}  {' '.join(p['signals'])}")
