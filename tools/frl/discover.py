"""Name the TI chip behind a probe with no --board: the wire protocols in turn, then the ID registers, matched
against data/chip-registry/.

  frl identify --probe S          (no --board)

Order, read-only throughout: (1) SWD with a generic Cortex-M target, reading each SWD family's ID register; a
chip whose reads all come back zero is asleep, so it is reset, halted, read again and run; (2) a JTAG scan,
whose TAP IDCODE names the CC13xx/CC26xx, C2000, Hercules, AM26x and K3 families; (3) TI's CC23xx and CC27xx
driver probe, which opens those parts' debug port. The SWD DPIDR is reported but never decides the family:
0x6BA02477 came back from MSPM0, MSPM33, AM13 and CC23xx alike (2026-10-24).
"""
import glob
import json
import re
import subprocess
from pathlib import Path

import toolfind
from core import ROOT

REGISTRY = ROOT / "data" / "chip-registry"
HEX = re.compile(r"0x[0-9A-Fa-f]+")
ICEPICK_MASK = 0x0FFFFFFF


def _hex(s):
    m = HEX.search(s or "") if isinstance(s, str) else None
    return int(m.group(0), 16) if m else None


def tables(registry=REGISTRY):
    """(mem, jtag) from the registry files.
    mem: {addr: [(family, part, partnum)]} for the families whose ID register holds PARTNUM in bits 27:12.
    jtag: {idcode & 0x0FFFFFFF: [(family, part)]}."""
    mem, jtag = {}, {}
    for f in sorted(glob.glob(str(Path(registry) / "*.json"))):
        for fam in json.loads(Path(f).read_text(encoding="utf-8"))["families"]:
            name = fam["family"]
            reads = [i for i in fam.get("identify", []) if i.get("method") == "mem_read" and _hex(i.get("addr"))]
            for m in fam.get("members", []):
                code = _hex(m.get("jtag_idcode"))
                if code:
                    jtag.setdefault(code & ICEPICK_MASK, []).append((name, m["part"]))
                pn = _hex(m.get("partnum"))
                if reads and pn is not None and _hex(reads[0]["addr"]) >= 0x10000:   # MSP430's 16-bit IDs need Spy-Bi-Wire, not SWD
                    mem.setdefault(_hex(reads[0]["addr"]), []).append((name, m["part"], pn & 0xFFFF))
    return mem, jtag


def _openocd(kind="openocd"):
    o = toolfind.find(kind, required=False)
    return (o, ["-s", o["scripts"]] if o and o.get("scripts") else []) if o else (None, [])


def _run(args, timeout=60):
    try:
        p = subprocess.run(args, capture_output=True, text=True, timeout=timeout)
        return p.stdout + p.stderr
    except subprocess.TimeoutExpired as e:
        return (e.stdout or "") if isinstance(e.stdout, str) else ""


def _base(o, scripts, serial, transport, speed):
    return [o["exe"], *scripts, "-c", "gdb_port disabled", "-c", "tcl_port disabled", "-c", "telnet_port disabled",   # TI's fork predates "gdb port"
            "-f", "interface/xds110.cfg", "-c", f"adapter serial {serial}", "-c", f"transport select {transport}",
            "-c", f"adapter speed {speed}"]


def parse_words(out):
    """{addr: first word} from OpenOCD `mdw` lines."""
    return {int(a, 16): int(w, 16) for a, w in re.findall(r"^0x([0-9a-f]{8}):\s+([0-9a-f]{8})", out or "", re.M)}


def parse_dpidr(out):
    m = re.search(r"SWD DPIDR (0x[0-9a-f]+)", out or "")
    return m.group(1).upper().replace("0X", "0x") if m else None


def parse_taps(out):
    return [int(x, 16) for x in re.findall(r"tap/device found: (0x[0-9a-f]+)", out or "")]


def match_mem(words, mem):
    """The families and parts whose ID register reads as a TI DEVICEID with a known PARTNUM."""
    hits = []
    for addr, cands in mem.items():
        w = words.get(addr)
        if not w or (w & 0xFFF) != 0x02F:                # bit 0 always 1, manufacturer 0x17: a TI DEVICEID
            continue
        pn = (w >> 12) & 0xFFFF
        parts = [(fam, part) for fam, part, p in cands if p == pn]
        fams = sorted({fam for fam, _, _ in cands})
        hits.append({"addr": f"0x{addr:08X}", "deviceid": f"0x{w:08X}", "partnum": f"0x{pn:04X}",
                     "families": sorted({f for f, _ in parts}) or fams, "parts": sorted({p for _, p in parts}),
                     "exact": bool(parts)})
    return hits


def match_jtag(codes, jtag):
    return [{"idcode": f"0x{c:08X}", "families": sorted({f for f, _ in jtag.get(c & ICEPICK_MASK, [])}),
             "parts": sorted({p for _, p in jtag.get(c & ICEPICK_MASK, [])}), "ti": (c & 0xFFF) == 0x02F} for c in codes]


# The family config that reads an ID register properly, by the address that answered the first, write-free pass.
# MSPM0 keeps its factory region dark while the program sleeps until ti_mspm0.cfg forces the power domain on
# (PWR_AP, its examine-start event); writing that on an unknown chip is not safe, so it runs only after the
# address answered.
FAMILY_CFG = {0x41C40004: ("openocd", "target/ti_mspm0.cfg"), 0x80111004: ("ti_openocd", "target/ti_mspm33.cfg"),
              0x60111004: ("ti_openocd", "target/ti_am13e230x.cfg")}


def readable(out):
    """Addresses an `mdw` read answered (value may be 0), apart from the ones OpenOCD failed to read."""
    failed = {int(a, 16) for a in re.findall(r"Failed to read memory at (0x[0-9a-fA-F]+)", out or "")}
    return {a: w for a, w in parse_words(out).items() if a not in failed}


def _read_with_cfg(serial, kind, cfg, addr):
    o, s = _openocd(kind)
    if not o or not o.get("scripts") or not (Path(o["scripts"]) / cfg).is_file():
        return None
    out = _run(_base(o, s, serial, "swd", 1000) + ["-f", cfg, "-c", "init", "-c", "catch {reset halt}",
               "-c", f"if {{[catch {{mdw 0x{addr:08X}}} r] == 0}} {{echo $r}}", "-c", "catch {reset run}", "-c", "shutdown"])
    return parse_words(out).get(addr)


def swd(serial, mem):
    o, s = _openocd()
    if not o:
        return {"tried": "swd", "error": "OpenOCD not found"}
    addrs = sorted(mem)
    reads = [f"if {{[catch {{mdw 0x{a:08X}}} r] == 0}} {{echo $r}}" for a in addrs]   # catch keeps mdw's text in r; an unmapped address does not stop the run
    base = _base(o, s, serial, "swd", 1000) + ["-c", "swd newdap chip cpu -enable", "-c", "dap create chip.dap -chain-position chip.cpu",
                                               "-c", "target create chip.cpu cortex_m -dap chip.dap", "-c", "init"]
    out = _run(base + sum((["-c", r] for r in reads), []) + ["-c", "shutdown"])
    words, woke = readable(out), None
    for a in [a for a in addrs if words.get(a) == 0 and a in FAMILY_CFG]:      # answered but dark: the family's own config
        w = _read_with_cfg(serial, *FAMILY_CFG[a], a)
        if w:
            words[a], woke = w, FAMILY_CFG[a][1]
            break
    hits, aps = match_mem(words, mem), []
    if parse_dpidr(out) and not any(h["families"] for h in hits):
        # The radio parts keep DEVICEID in a configuration access port, register 0 (CC35xx AP0, CC23xx/CC27xx AP1):
        # a read only, never a memory-style CSW or TAR write (registry, CC35xx entry).
        dap_only = _base(o, s, serial, "swd", 1000) + ["-c", "swd newdap chip cpu -enable", "-c", "dap create chip.dap -chain-position chip.cpu"]
        out2 = _run(dap_only + ["-c", "init"] + sum((["-c", f"if {{[catch {{chip.dap apreg {n} 0x0}} r] == 0}} {{echo \"AP{n} $r\"}}"] for n in (0, 1)), [])
                    + ["-c", "shutdown"])
        aps = [int(v, 0) for v in re.findall(r"^AP\d (0x[0-9a-fA-F]+|\d+)$", out2, re.M)]
    return {"tried": "swd", "dpidr": parse_dpidr(out), "family_cfg": woke, "hits": hits,
            "ap_ids": [t for t in match_jtag([a for a in aps if (a & 0xFFF) == 0x02F], tables()[1])]}


def config_port_regs(serial, offsets, ap=0):
    """Read registers of a radio part's configuration access port, read-only (AP register reads, no memory access).
    CC35xx AP0: 0x00 DEVICE_ID, 0x14 LIFE_CYCLE_STATE, 0x28 and 0x2C UNIQUE_DEVICE_ID0 and 1, the layout in TI's
    Wi-Fi Toolbox radio-tool guide ("general device_status"); on our LP-EM-CC35X1 two reads gave the same words
    (2026-10-25). Returns {offset: word}; an offset that did not answer is left out."""
    o, s = _openocd()
    if not o:
        return {}
    dap_only = _base(o, s, serial, "swd", 1000) + ["-c", "swd newdap chip cpu -enable", "-c", "dap create chip.dap -chain-position chip.cpu"]
    reads = sum((["-c", f"if {{[catch {{chip.dap apreg {ap} 0x{a:X}}} r] == 0}} {{echo \"R{a:02X} $r\"}}"] for a in offsets), [])
    out = _run(dap_only + ["-c", "init"] + reads + ["-c", "shutdown"])
    return {int(a, 16): int(v, 0) for a, v in re.findall(r"^R([0-9A-F]{2}) (0x[0-9a-fA-F]+|\d+)$", out, re.M)}


def jtag_scan(serial, jtag):
    """A blind autoprobe reads all zeros through the XDS110 (2026-10-24), so declare a placeholder tap: TI's ICEPick
    (IR length 6), then a plain Arm JTAG-DP (IR length 4). OpenOCD reports the real IDCODE as "UNEXPECTED"."""
    o, s = _openocd()
    if not o:
        return {"tried": "jtag", "error": "OpenOCD not found"}
    codes = []
    # TI's ICEPick parts start in two-wire cJTAG: ti-cjtag.cfg switches them to 4-pin JTAG on reset (as ti_cc26x0.cfg does)
    tries = [["-f", "target/ti-cjtag.cfg", "-c", "jtag newtap probe tap -irlen 6 -ircapture 0x1 -irmask 0x3f -expected-id 0x00000001",
              "-c", "jtag configure probe.tap -event post-reset {ti_cjtag_to_4pin_jtag probe.tap}"],
             ["-c", "jtag newtap probe tap -irlen 6 -ircapture 0x1 -irmask 0x3f -expected-id 0x00000001"],
             ["-c", "jtag newtap probe tap -irlen 4 -ircapture 0x1 -irmask 0xf -expected-id 0x00000001"]]
    for tap in tries:
        out = _run(_base(o, s, serial, "jtag", 1000) + tap + ["-c", "init", "-c", "shutdown"])
        codes = [c for c in parse_taps(out) if c not in (0, 0xFFFFFFFF)]
        if any((c & 0xFFF) == 0x02F for c in codes):   # a TI IDCODE (manufacturer 0x17); the cJTAG switch on a 4-pin K3 part reads garbage
            break
    return {"tried": "jtag", "taps": match_jtag(codes, jtag)}


PROBE_LINE = re.compile(r"Device: (\S+), Flash: (\d+)kb, RAM: (\d+)kb")


def lpf3(serial):
    """TI's CC23xx/CC27xx driver: `flash probe 0` names the part, then the program runs again."""
    o, s = _openocd("ti_openocd")
    if not o:
        return {"tried": "lpf3", "error": "TI's OpenOCD (ti_openocd) not found"}
    for cfg in ("target/ti_cc23xx.cfg", "target/ti_cc27xx.cfg"):
        out = _run(_base(o, s, serial, "swd", 4000) + ["-f", cfg, "-c", "init", "-c", "catch {flash probe 0}", "-c", "catch {reset run}", "-c", "shutdown"])
        m = PROBE_LINE.search(out)
        if m:
            return {"tried": "lpf3", "cfg": cfg, "part": m.group(1), "flash_kb": int(m.group(2))}
    return {"tried": "lpf3", "part": None}


def c28_ids(registry=REGISTRY):
    """{PARTIDH: [(family, part)]} for the C2000 members whose registry PARTID is one exact value."""
    out = {}
    for f in sorted(glob.glob(str(Path(registry) / "*.json"))):
        for fam in json.loads(Path(f).read_text(encoding="utf-8"))["families"]:
            for m in fam.get("members", []):
                v = m.get("partid") or ""
                if re.fullmatch(r"0x[0-9A-Fa-f]{8}", v.strip()):
                    out.setdefault(int(v, 16), []).append((fam["family"], m["part"]))
    return out


def dslite_c28(serial):
    """C2000: OpenOCD has no C28x support, so TI's DSLite reads PARTIDL and PARTIDH (0x5D008, four 16-bit words)
    through an XDS110 and ICEPick description of a single-core C28x (tools/frl/ccxml/c28x_xds110.ccxml, from the
    LAUNCHXL-F2800137 file). Read-only. The registry gives PARTIDH for F2800x, F28003x, F28004x, F28P55x and F28P65x."""
    import tempfile
    d = toolfind.find("dslite", required=False)
    if not d:
        return {"tried": "dslite_c28", "error": "TI's DSLite not found"}
    tmp = Path(tempfile.mkdtemp(prefix="frl-c28-"))
    ccxml = tmp / "c28.ccxml"
    ccxml.write_text((Path(__file__).parent / "ccxml" / "c28x_xds110.ccxml").read_text(encoding="utf-8").replace("{SERIAL}", serial))
    out = _run([d["exe"], "memory", "-c", str(ccxml), "-r", "0x5D008,4", "-s", "16", "-o", str(tmp / "id.bin"), "-t", "60"], timeout=90)
    raw = (tmp / "id.bin").read_bytes() if (tmp / "id.bin").is_file() else b""
    if len(raw) < 8:
        return {"tried": "dslite_c28", "partidh": None, "log": out[-300:]}
    w = [int.from_bytes(raw[i:i + 2], "little") for i in range(0, 8, 2)]
    partidh = (w[3] << 16) | w[2]
    hits = c28_ids().get(partidh, [])
    return {"tried": "dslite_c28", "partidh": f"0x{partidh:08X}", "partidl": f"0x{(w[1] << 16) | w[0]:08X}",
            "families": sorted({f for f, _ in hits}), "parts": sorted({p for _, p in hits})}


def identify(serial):
    """Every step that ran, and the answer: {family, part, how, steps}."""
    mem, jtag = tables()
    steps = [swd(serial, mem)]
    hit = next((h for h in steps[0].get("hits", []) if h["families"]), None)
    ap = next((a for a in steps[0].get("ap_ids", []) if a["families"]), None)
    if not hit and ap:
        return {"family": ap["families"], "part": ap["parts"], "exact": len(ap["parts"]) == 1, "how": f"SWD, configuration port DEVICEID {ap['idcode']}", "steps": steps}
    if hit:
        return {"family": hit["families"], "part": hit["parts"], "exact": hit["exact"], "how": f"SWD, ID register {hit['addr']}", "steps": steps}
    steps.append(jtag_scan(serial, jtag))
    tap = next((t for t in steps[-1].get("taps", []) if t["families"]), None)
    if tap:
        return {"family": tap["families"], "part": tap["parts"], "exact": len(tap["parts"]) == 1, "how": f"JTAG IDCODE {tap['idcode']}", "steps": steps}
    steps.append(lpf3(serial))
    if steps[-1].get("part"):
        fam = "CC23xx" if "cc23" in steps[-1]["cfg"] else "CC27xx"
        return {"family": [fam], "part": [steps[-1]["part"]], "exact": True, "how": "TI's CC23xx/CC27xx driver probe", "steps": steps}
    steps.append(dslite_c28(serial))
    c = steps[-1]
    if c.get("families"):
        return {"family": c["families"], "part": c["parts"], "exact": len(c["parts"]) == 1, "how": f"TI's DSLite, C2000 PARTIDH {c['partidh']}", "steps": steps}
    ti = [t["idcode"] for t in (steps[1].get("taps") or []) if t["ti"]]
    return {"family": [], "part": [], "exact": False, "steps": steps,
            "how": (f"a TI JTAG device {', '.join(ti)} that the registry does not list yet" if ti else "no answer on SWD or JTAG")}
