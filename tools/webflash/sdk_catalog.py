#!/usr/bin/env python3
"""Predict LaunchPad kits the bench has never seen, from TI's own files on this Mac (open aperture).

  python3 tools/webflash/sdk_catalog.py            writes site/flash/bench/boards-predicted.json
  python3 tools/webflash/sdk_catalog.py --check    exit 1 if the file is stale or the cross-check fails

Sources:
  CCStudio's GEL scripts hold the DEVICEIDs: ccs_base/emulation/gel/mspm0*.gel (one per part) and cc23xx_util.gel.
  The MSPM0 SDK holds each kit's flash and SRAM (ticlang/device_linker.cmd), LED pins (ti_msp_dl_config.h) and
  whether it programs with ECC (flashctl_multiple_size_write.c).
  The SimpleLink F3 SDK holds each kit's part and LED pins (boards/.meta/*.syscfg.json) and the device's flash and
  RAM (devices/*/linker_files/*.cmd).
  CCStudio's target database gives the CFG-AP register map (PARTID at offset 0x4).

Rules: a kit already in boards.json is never predicted (the tested entry wins). The generator must reproduce every
tested MSPM0 entry's ID, flash end, RAM, RAM start and CMDBYTEN before its predictions are written; a kit with a
missing source is left out and named. Nothing here writes a board: every entry is read-only until it climbs the
three rungs in the research note. NONMAIN, BCR and BSL ranges are never emitted.
"""
import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "site" / "flash" / "bench" / "boards-predicted.json"
TI = Path("/Applications/ti")
CCS_GEL = TI / "ccs2101" / "ccs" / "ccs_base" / "emulation" / "gel"
MSPM0_SDK = TI / "mspm0_sdk_2_11_00_07"
F3_SDK = TI / "simplelink_lowpower_f3_sdk_9_21_00_36_LTS"
H = lambda v: "0x%08X" % v  # noqa: E731
GPIO_BASE = {"A": 0x400A0000, "B": 0x400A2000, "C": 0x400A4000}

# F3 kit -> the device linker file that sizes it (the SDK's own board-to-device mapping is in the syscfg "gpn").
F3_DEVICE = {"CC2340R2": "cc23x0r2/linker_files/cc23x0r2.cmd", "CC2340R5": "cc23x0r5/linker_files/cc23x0r5.cmd",
             "CC2340R53": "cc23x0r5/linker_files/cc23x0r53.cmd", "CC2745R10": "cc27xxx10/linker_files/cc27xxx10.cmd",
             "CC2755R10": "cc27xxx10/linker_files/cc27xxx10.cmd", "CC2755P10": "cc27xxx10/linker_files/cc27xxx10.cmd",
             "CC2755P20": "cc27xxx20/linker_files/cc27xxx20.cmd"}
# From cc23xx_util.gel: the DEVICEID each Is...() test checks, and whether PARTID's variant byte is needed to tell siblings apart.
F3_IDS = {"CC2340R2": ("0x0BB8502F", "id-known"), "CC2340R5": ("0x0BB8402F", "id-known"), "CC2340R53": ("0x0BBAE02F", "id-known"),
          "CC2340R22": ("0x0BB8402F", "id-pattern"), "CC2755P20": ("0x0BBC502F", "id-known"),
          "CC2745R10": ("0x0BB9802F", "id-pattern"), "CC2755R10": ("0x0BB9802F", "id-pattern"), "CC2755P10": ("0x0BB9802F", "id-pattern")}
F3_VARIANT = {"CC2340R22": "0x9E"}   # cc23xx_util.gel IsCC23x0R22(): PARTID bits 23:16


def gel_ids():
    """{DEVICEID: [parts]} from CCStudio's per-part MSPM0 GEL files; the GEL masks bit 0, so ...2E means ...2F on the chip."""
    ids = {}
    for g in sorted(CCS_GEL.glob("mspm0*.gel")):
        m = re.search(r"0x([0-9A-F]{6})2E", g.read_text(errors="replace"))
        if not m:
            continue
        ids.setdefault("0x%s2F" % m.group(1), []).append(g.stem.upper().replace("MSPM0", "MSPM0"))
    return ids


def linker(path):
    t = path.read_text()
    fl = re.search(r"FLASH\s*\(RX\)\s*:\s*origin\s*=\s*(0x[0-9A-Fa-f]+),\s*length\s*=\s*(0x[0-9A-Fa-f]+)", t)
    # SRAM, or SRAM_BANK0 and SRAM_BANK1 on the G3519 (contiguous banks; the second is lost in deep sleep, the chip has both)
    banks = re.findall(r"SRAM(?:_BANK\d)?\s*\(RWX\)\s*:\s*origin\s*=\s*(0x[0-9A-Fa-f]+),\s*length\s*=\s*(0x[0-9A-Fa-f]+)", t)
    if not (fl and banks):
        raise ValueError(f"{path}: no FLASH or SRAM line")
    banks = sorted((int(o, 16), int(l, 16)) for o, l in banks)
    for (o1, l1), (o2, _) in zip(banks, banks[1:]):
        if o1 + l1 != o2:
            raise ValueError(f"{path}: SRAM banks are not contiguous")
    # the L1306's linker keeps the last 8 bytes back (length 0xFFF8); the chip's flash is whole sectors of 1 kB
    flen = -(-int(fl.group(2), 16) // 1024) * 1024
    return int(fl.group(1), 16), flen, banks[0][0], sum(l for _, l in banks)


def leds(path):
    """[(label, port, pin)] from TI's ti_msp_dl_config.h: a shared GPIO_LEDS_PORT or one _PORT per LED."""
    t = path.read_text()
    shared = re.search(r"#define GPIO_LEDS_PORT\s+\(GPIO([A-C])\)", t)
    out = []
    for m in re.finditer(r"#define GPIO_LEDS_(USER_LED_\d)_PIN\s+\(DL_GPIO_PIN_(\d+)\)", t):
        name, pin = m.group(1), int(m.group(2))
        pm = re.search(r"#define GPIO_LEDS_%s_PORT\s+\(GPIO([A-C])\)" % name, t)
        port = pm.group(1) if pm else (shared.group(1) if shared else None)
        if port is None:
            raise ValueError(f"{path}: no port for {name}")
        out.append((name.replace("USER_LED_", "LED"), port, pin))
    if not out:
        raise ValueError(f"{path}: no LEDs")
    return out


def mspm0_kits(ids):
    kits, left_out = [], []
    by_part = {p: did for did, parts in ids.items() for p in parts}
    for kd in sorted((MSPM0_SDK / "examples" / "nortos").glob("LP_MSPM0*")):
        part = kd.name.replace("LP_", "")
        ex = kd / "driverlib" / "gpio_toggle_output"
        fw = kd / "driverlib" / "flashctl_multiple_size_write" / "flashctl_multiple_size_write.c"
        try:
            did = by_part[part]
            fo, fl, so, sl = linker(ex / "ticlang" / "device_linker.cmd")
            ls = leds(ex / "ti_msp_dl_config.h")
            ecc = "WithECCGenerated" in fw.read_text()
        except (KeyError, FileNotFoundError, ValueError) as e:
            left_out.append(f"{kd.name}: {e if not isinstance(e, KeyError) else 'no DEVICEID in the CCS GEL files'}")
            continue
        sib = [p for p in ids[did] if p != part]
        kits.append({
            "id": part.lower(), "kit": kd.name.replace("_", "-"), "printed": kd.name.replace("_", "-"), "part": part,
            "status": "predicted", "programmable": False,
            "confidence": "id-known" if not sib else "id-shared",
            "identity": {"addr": "0x41C40000", "deviceid": did, "deviceidMask": "0x0FFFFFFF"},
            "siblings": sib,
            "flash": {"start": H(fo), "end": H(fo + fl)}, "ram": sl, "ramStart": H(so),
            "cmdbyten": "0x1FF" if ecc else "0xFF", "gpio": {"dout": "0x1280", "doe": "0x12C0"},
            "leds": [{"label": f"{n} (P{p}{pin})", "port": p, "pin": pin, "gpio": H(GPIO_BASE[p])} for n, p, pin in ls],
            "check": {"sramflashKb": fl // 1024, "sramKb": sl // 1024},
            "note": ("Predicted, not tested here: this page reads it and checks its flash size against the device tables. It does not write it yet."
                     + (f" The ID is shared with {', '.join(sib)}; the flash size tells them apart." if sib else "")),
            "basis": {"deviceid": f"ccs2101 {CCS_GEL.name}/{part.lower()}.gel", "memory": str((ex / 'ticlang' / 'device_linker.cmd').relative_to(MSPM0_SDK)),
                      "leds": str((ex / 'ti_msp_dl_config.h').relative_to(MSPM0_SDK)), "ecc": str(fw.relative_to(MSPM0_SDK)), "sdk": MSPM0_SDK.name},
            "unknown": ["LED polarity (the example says only which LEDs start on)", "jumpers (prose in the kit README)", "console baud and jumpers", "the board picture"],
        })
    return kits, left_out


def strip_jsonc(s):
    """TI's kit files carry // and /* */ comments and trailing commas (SysConfig reads them, json does not); strings keep their //."""
    out, i, n = [], 0, len(s)
    while i < n:
        c = s[i]
        if c == '"':
            j = i + 1
            while j < n and s[j] != '"':
                j += 2 if s[j] == "\\" else 1
            out.append(s[i:j + 1]); i = j + 1
        elif s.startswith("//", i):
            i = s.find("\n", i); i = n if i < 0 else i
        elif s.startswith("/*", i):
            i = s.find("*/", i + 2); i = n if i < 0 else i + 2
        else:
            out.append(c); i += 1
    return re.sub(r",(\s*[}\]])", r"\1", "".join(out))


def f3_kits():
    """One entry per DEVICEID among the SimpleLink F3 kits; package and grade siblings (Q1, WCSP, MODA, BG, CS-EVM) share it."""
    found, left_out = {}, []
    for sj in sorted((F3_SDK / "source" / "ti" / "boards" / ".meta").glob("*.syscfg.json")):
        d = json.loads(strip_jsonc(sj.read_text()), strict=False)
        name = d["name"]
        base = re.sub(r"(_Q1|_WCSP|_BG|MODA|_CS_EVM)$", "", re.sub(r"^LP_EM_", "", name))
        if base not in F3_IDS:
            left_out.append(f"{name}: no DEVICEID branch for {base} in cc23xx_util.gel")
            continue
        did, conf = F3_IDS[base]
        sibling = base != re.sub(r"^LP_EM_", "", name)
        comps = d.get("components", {})
        ls = [{"label": comps[k]["displayName"], "dio": comps[k]["connections"]["OUTPUT"]} for k in ("LED_RED", "LED_GREEN") if k in comps and "connections" in comps[k]]
        kit = name.replace("_", "-")
        if sibling and (F3_SDK / "source" / "ti" / "boards" / ".meta" / f"LP_EM_{base}.syscfg.json").exists():
            found.setdefault(base, {}).setdefault("siblings", []).append(kit)
            continue
        if sibling and "id" in found.get(base, {}):
            found[base].setdefault("siblings", []).append(kit); continue
        e = found.setdefault(base, {})
        e.update({"id": base.lower(), "kit": kit, "printed": kit, "part": d.get("variant") or d.get("gpn") or base,
                  "status": "predicted", "programmable": False, "confidence": conf,
                  "identity": {"kind": "cfgap", "ap": 1, "deviceids": [did], "deviceidMask": "0x0FFFFFFF", "partid": {"offset": "0x4", "variantMask": "0x00FF0000"}},
                  "leds": ls,
                  "basis": {"deviceid": f"ccs2101 {CCS_GEL.name}/cc23xx_util.gel", "kit": str(sj.relative_to(F3_SDK)), "sdk": F3_SDK.name,
                            "partid": "ccs2101 targetdb/Modules/CC23xx/CC23x0R5/AP/CFGAP.xml and CC27xx/AP/CFGAP.xml: PARTID at offset 0x4"},
                  "unknown": ["which PARTID variant byte each kit carries (the SDK does not say)", "the writable flash range (TI's security firmware sits at the top on CC27xx)", "the board picture"]})
        e.setdefault("siblings", [])
        if base in F3_DEVICE:
            lt = (F3_SDK / "source" / "ti" / "devices" / F3_DEVICE[base]).read_text()
            g = lambda k: int(re.search(r"#define %s\s+(0x[0-9A-Fa-f]+)" % k, lt).group(1), 16)  # noqa: E731
            e["memory"] = {"flashLinker": H(g("FLASH_BASE") + g("FLASH_SIZE")), "ram": g("RAM_SIZE"), "ramStart": H(g("RAM_BASE")), "basis": F3_DEVICE[base],
                           "note": "the linker's flash size is the device, not the writable range; never used as a write map"}
        if base in F3_VARIANT:
            e["identity"]["variant"] = F3_VARIANT[base]
    kits = []
    for base, e in found.items():
        if "id" not in e:
            left_out.append(f"{base}: only sibling kits, no base kit file"); continue
        e["note"] = ("Predicted, not tested here: this page reads its ID through the chip's configuration port. It does not write it."
                     + (" Siblings share this ID; the PARTID variant byte would tell them apart, and this page shows it but has not verified it on this kit." if e["confidence"] == "id-pattern" else "")
                     + (f" The same chip ships on the {', '.join(e['siblings'])}." if e["siblings"] else ""))
        kits.append(e)
    return kits, left_out


def cross_check(kits, tested):
    """The generator must reproduce every tested MSPM0 entry before anything predicted is believed."""
    bad = []
    for t in tested:
        if t["identity"].get("kind") == "cfgap" or not t.get("flash") or t["identity"].get("addr") != "0x41C40000":
            continue
        k = next((x for x in kits if x["id"] == t["id"]), None)
        if not k:
            bad.append(f"{t['id']}: not generated"); continue
        # the tested C1104 entry keeps the version nibble it read (0x3...); the page masks it, so compare masked
        for f, a, b in (("deviceid", int(k["identity"]["deviceid"], 16) & 0x0FFFFFFF, int(t["identity"]["deviceid"], 16) & 0x0FFFFFFF), ("flash end", k["flash"]["end"], t["flash"]["end"]),
                        ("ram", k["ram"], t["ram"]), ("ramStart", k["ramStart"], t["ramStart"]), ("cmdbyten", k["cmdbyten"], t["cmdbyten"])):
            if str(a).upper() != str(b).upper():
                bad.append(f"{t['id']} {f}: generated {a}, tested {b}")
    return bad


def build(tested=None):
    tested = tested if tested is not None else json.loads((ROOT / "site" / "flash" / "bench" / "boards.json").read_text())["boards"]
    ids = gel_ids()
    m0, out1 = mspm0_kits(ids)
    bad = cross_check(m0, tested)
    if bad:
        raise SystemExit("sdk_catalog: the generator does not reproduce the tested boards; nothing written:\n  " + "\n  ".join(bad))
    f3, out2 = f3_kits()
    # a tested entry wins: a predicted kit whose every ID a tested entry already claims is dropped (the page would never reach it)
    known_ids = {t["id"] for t in tested}
    known_dids = {d.upper() for t in tested for d in t["identity"].get("deviceids", [])}
    predicted = [k for k in m0 + f3 if k["id"] not in known_ids and not (k["identity"].get("deviceids") and all(d.upper() in known_dids for d in k["identity"]["deviceids"]))]
    reproduced = [k["id"] for k in m0 if k["id"] in {t["id"] for t in tested}]
    return {"note": "Written by tools/webflash/sdk_catalog.py from CCStudio's GEL files and TI's SDKs on this Mac. Do not edit by hand. "
                    "Every board here is read-only until a hardware run moves it into boards-draft/.",
            "sources": {"ccs": "ccs2101", "mspm0_sdk": MSPM0_SDK.name, "f3_sdk": F3_SDK.name},
            "idTable": {did: parts for did, parts in sorted(ids.items())},
            "reproduced": reproduced, "leftOut": out1 + out2, "boards": predicted}


def write(tested=None):
    """Write the file when TI's files are here; otherwise keep the committed one. Returns the predicted boards."""
    if not (CCS_GEL.is_dir() and MSPM0_SDK.is_dir() and F3_SDK.is_dir()):
        return json.loads(OUT.read_text())["boards"] if OUT.exists() else []
    doc = build(tested)
    OUT.write_text(json.dumps(doc, indent=1, ensure_ascii=False) + "\n")
    return doc["boards"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    if not CCS_GEL.is_dir() or not MSPM0_SDK.is_dir() or not F3_SDK.is_dir():
        print("sdk_catalog: TI's files are not installed here; keeping the committed boards-predicted.json")
        return 0 if OUT.exists() else 1
    doc = build()
    text = json.dumps(doc, indent=1, ensure_ascii=False) + "\n"
    if a.check:
        if not OUT.exists() or OUT.read_text() != text:
            print("sdk_catalog: boards-predicted.json is stale; run python3 tools/webflash/sdk_catalog.py"); return 1
        print("sdk_catalog: OK"); return 0
    OUT.write_text(text)
    print(f"wrote {OUT.relative_to(ROOT)}: {len(doc['boards'])} predicted boards, {len(doc['reproduced'])} tested boards reproduced, "
          f"{len(doc['idTable'])} IDs over {sum(len(v) for v in doc['idTable'].values())} MSPM0 parts; left out: {len(doc['leftOut'])}")
    for l in doc["leftOut"]:
        print("  left out:", l)
    return 0


if __name__ == "__main__":
    sys.exit(main())
