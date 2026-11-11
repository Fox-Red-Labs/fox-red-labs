#!/usr/bin/env python3
"""Write site/flash/bench/pinouts.json: every pin of each MSPM0 LaunchPad kit's chip package, for the bench's pin panel.

  python3 tools/webflash/build_pinouts.py

Sources, nothing typed by hand: the package of each kit is the --package line at the top of TI's gpio_toggle_output.syscfg
for the kit in the MSPM0 SDK; the pin numbers and names (PA0, VDD, VSS, NRST, ...) are the package table of SysConfig's
device data (dist/deviceData/<family>/<family>.json). The page marks the pins a demo uses; it adds no names.
"""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SDK = Path("/Applications/ti/mspm0_sdk_2_11_00_07/examples/nortos")
SYSCONFIG = Path("/Applications/ti/sysconfig_1.28.0/dist/deviceData")
BOARDS = {"mspm0l1306": "LP_MSPM0L1306", "mspm0g3519": "LP_MSPM0G3519", "mspm0g5187": "LP_MSPM0G5187", "mspm0g3507": "LP_MSPM0G3507", "mspm0c1104": "LP_MSPM0C1104"}


def main():
    out = {"note": "Written by tools/webflash/build_pinouts.py from TI's SysConfig device data and the MSPM0 SDK kit examples. Do not edit by hand.", "boards": {}}
    for bid, kit in BOARDS.items():
        syscfg = (SDK / kit / "driverlib" / "gpio_toggle_output" / "gpio_toggle_output.syscfg").read_text()
        m = re.search(r'--device "([^"]+)" --package "([^"]+)"', syscfg)
        fam, pkg = m.groups()
        d = json.loads((SYSCONFIG / fam / f"{fam}.json").read_text())
        p = next(v for v in d["packages"].values() if v["name"] == pkg)
        pins = []
        for pp in p["packagePin"]:
            dp = d["devicePins"][pp["devicePinID"]]
            pins.append({"n": pp["ball"], "name": dp["name"], "pincm": (dp.get("attributes") or {}).get("iomux_pincm", "")})
        pins.sort(key=lambda x: (not x["n"].isdigit(), int(x["n"]) if x["n"].isdigit() else 0, x["n"]))
        out["boards"][bid] = {"package": pkg, "family": fam, "pins": pins,
                              "source": f"TI's SysConfig 1.28.0 device data for the {fam}, package {pkg}, as the MSPM0 SDK 2.11.00.07 LED example for the {kit} uses it"}
        print(bid, pkg, len(pins), "pins")
    (ROOT / "site" / "flash" / "bench" / "pinouts.json").write_text(json.dumps(out, separators=(",", ":")) + "\n")


if __name__ == "__main__":
    main()
