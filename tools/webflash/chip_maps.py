#!/usr/bin/env python3
"""Write the live chip view's register maps from the MSPM0 SDK device headers: one JSON per board.

  python3 tools/webflash/chip_maps.py            writes site/flash/chip/maps/<board>.json and index.json

Each map: identity, flash and RAM, every peripheral with its base address, interrupt line and kind, and every pin control
register with its port, pin number and the functions the header names. The power-enable register of a peripheral is at
base + 0x800. LED labels come from the bench's board drafts. Nothing is invented: a fact the header does not give is left out.
"""
import json, re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SDK = Path("/Applications/ti/mspm0_sdk_2_11_00_07/source/ti/devices/msp/m0p")
OUT = ROOT / "site/flash/chip/maps"
BOARDS = {"mspm0l1306": "mspm0l130x.h", "mspm0g3507": "mspm0g350x.h", "mspm0g3519": "mspm0g351x.h", "mspm0g5187": "mspm0g518x.h"}
SKIP = re.compile(r"^(FACTORYREGION|CPUSS|DEBUGSS|IOMUX|FLASHCTL|SYSCTL|WUC|.*_SRAM|.*_PERIPHERALREGIONSVT|.*_SVT|UC\d_(UART|I2CC|I2CT|SPI)|KEYSTORECTL|EVENTLP|SPG\d)$")
def kind(n):
    if n.startswith("GPIO"): return "io"
    if re.match(r"TIM[GA]\d", n): return "timer"
    if n.startswith("TIM"): return "timer-other"
    if re.match(r"UART\d", n): return "uart"
    if re.match(r"I2C\d", n): return "i2c"
    if re.match(r"SPI\d", n): return "spi"
    if re.match(r"(CANFD|I2S|USBFS|UC)\d", n): return "serial"
    if re.match(r"ADC\d", n): return "adc"
    if re.match(r"(DAC|COMP|OPA|VREF|GPAMP)", n): return "analog"
    if re.match(r"(AESADV|TRNG|CRC|MATHACL|NPU)", n): return "accel"
    return "system"
GROUP = {"io": "GPIO", "timer": "Timers", "timer-other": "Timers", "uart": "Serial", "i2c": "Serial", "spi": "Serial", "serial": "Serial",
         "adc": "Analog", "analog": "Analog", "accel": "Math and security", "system": "System"}

def build(board, header):
    s = (SDK / header).read_text()
    bases = {m[0]: int(m[1], 16) for m in re.findall(r"#define\s+(\w+)_BASE\s+\(0x([0-9A-Fa-f]+)U?\)", s)}
    irq = {}
    for n, v in re.findall(r"\s(\w+)_INT_IRQn\s*=\s*(\d+),", s): irq.setdefault(n, int(v))
    per = {}
    for n, b in sorted(bases.items(), key=lambda x: x[1]):
        if SKIP.match(n) or b < 0x40000000 or b >= 0x41000000: continue
        k = kind(n); per[n] = {"base": b, "kind": k, "group": GROUP[k]}
        if n in irq: per[n]["irq"] = irq[n]
    pins = {}
    for i, name, v in re.findall(r"#define\s+IOMUX_PINCM(\d+)_PF_(\w+)\s+\(\(uint32_t\)0[xX]0*([0-9A-Fa-f]*)\)", s):
        if name == "UNCONNECTED": continue
        p = pins.setdefault(int(i), {"fn": {}}); p["fn"][int(v or "0", 16)] = name
        m = re.match(r"GPIO([A-C])_DIO(\d+)", name)
        if m: p["port"], p["pin"] = m[1], int(m[2])
    bj = {b["id"]: b for b in json.loads((ROOT / "site/flash/bench/boards.json").read_text())["boards"]}[board]
    labels = {}
    draft = ROOT / "site/flash/bench/boards-draft" / f"{board}.json"
    if draft.exists():
        d = json.loads(draft.read_text())
        for c in d.get("check_channels", []):
            port = {0x400A0000: "A", 0x400A2000: "B", 0x400A4000: "C"}.get(int(c["gpio"], 16))
            if port: labels[f"P{port}{c['bit']}"] = c.get("label", "LED")
    return {"board": board, "kit": bj["kit"], "chip": header.replace(".h", "").upper().replace("X", "x"), "header": header,
            "deviceid": int(bj["identity"]["deviceid"], 16), "mask": int(bj["identity"]["deviceidMask"], 16),
            "flash": int(bj["flash"]["end"], 16) - int(bj["flash"]["start"], 16), "ram": {"start": int(bj["ramStart"], 16), "bytes": bj["ram"]},
            "iomux": bases["IOMUX"], "sysctl": bases["SYSCTL"], "peripherals": per,
            "pins": {str(k): pins[k] for k in sorted(pins)}, "labels": labels}

OUT.mkdir(parents=True, exist_ok=True)
idx = []
for b, h in BOARDS.items():
    m = build(b, h); (OUT / f"{b}.json").write_text(json.dumps(m, indent=1) + "\n")
    idx.append({"board": b, "kit": m["kit"], "deviceid": m["deviceid"], "mask": m["mask"]})
    print(b, len(m["peripherals"]), "peripherals", len(m["pins"]), "pins", m["labels"])
(OUT / "index.json").write_text(json.dumps(idx, indent=1) + "\n")
