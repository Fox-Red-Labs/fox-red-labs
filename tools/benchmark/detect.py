#!/usr/bin/env python3
"""Find boards on USB and say which board and chip each one is.

  python3 tools/benchmark/detect.py [--no-ccs] [--json]
  python3 tools/benchmark/detect.py --probe <serial> [--device <name>]   chip-ID read through one probe

Three layers, cheapest first:
1. USB: every non-hub device, matched to data/arrivals.json (vendor families, probes we already know,
   serials recorded on arrived items). Many TI LaunchPad development kits share one XDS110 identity,
   so USB alone often says only "a TI probe".
2. CCStudio's MCP server, when CCStudio is open: getConnectedDevices names the board and chip behind
   each XDS110 serial in one call. Skipped with --no-ccs. An agent that has the ccs-debug MCP tools
   in its session calls getConnectedDevices itself instead.
3. Chip-ID read through the probe (--probe): connects to that one probe only, with DSLite, and reads
   the Arm CPUID and, where the family is in ID_REGS below, its device-ID register. Read-only. Needs a
   target configuration: a <name>_<serial>.ccxml already in a board folder, or --device <name> (the
   device file name in CCStudio's targetdb, for example MSPM0L1306), from which one is written to
   tools/benchmark/local/.

USB listing: system_profiler on macOS, /sys/bus/usb on Linux, Get-PnpDevice on Windows (written,
not tested on Windows yet).
"""
import argparse
import glob
import json
import re
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import common as c  # noqa: E402

# Device-ID registers we have read on real boards. Add a family when its first board is identified.
ID_REGS = {
    "mspm0": {"addr": 0x41C40004, "name": "DEVICEID (factory region)",
              "decode": lambda v: f"part number 0x{(v >> 12) & 0xFFFF:04X}, version {v >> 28}",
              "known": {0x1BB8202F: "mspm0l1306"}},
}
CPU_PART = {0xC60: "Cortex-M0+", 0xD21: "Cortex-M33", 0xC24: "Cortex-M4", 0xC27: "Cortex-M7", 0xC20: "Cortex-M0", 0xC23: "Cortex-M3"}

CCXML = """<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<configurations XML_version="1.2" id="configurations_0">
    <configuration XML_version="1.2" id="configuration_0">
        <instance XML_version="1.2" desc="Texas Instruments XDS110 USB Debug Probe" href="connections/TIXDS110_Connection.xml" id="Texas Instruments XDS110 USB Debug Probe" xml="TIXDS110_Connection.xml" xmlpath="connections"/>
        <connection XML_version="1.2" id="Texas Instruments XDS110 USB Debug Probe">
@DRIVERS@
            <property Type="choicelist" Value="1" id="SEPK.POD_PORT">
                <choice Name="Only one XDS110 installed" value="0"/>
                <choice Name="Select by serial number" value="1">
                    <property Type="stringfield" Value="@SERIAL@" id="SEPK.POD_SERIAL"/>
                </choice>
            </property>
            <property id="Debug Probe Selection" Type="choicelist" Value="1">
                <choice Name="Select by serial number" value="0">
                    <property id="-- Enter the serial number" Type="stringfield" Value="@SERIAL@"/>
                </choice>
            </property>
            <platform XML_version="1.2" id="platform_0">
                <instance XML_version="1.2" desc="@DEV@" href="devices/@DEV@.xml" id="@DEV@" xml="@DEV@.xml" xmlpath="devices"/>
            </platform>
        </connection>
    </configuration>
</configurations>
"""


# Debug drivers per core, as in the ccxml files CCStudio wrote for our boards (mspm0l1306/, cc1354p10/).
DRIVERS = {
    "Cortex-M0+": ["tixds510cs_dap", "tixds510cortexM0", "tixds510sec_ap"],
    "Cortex-M33": ["tixds510cs_dap", "tixds510cortexM33", "tixds510sec_ap"],
    "Cortex-M4": ["tixds510cs_dap", "tixds510cortexM"],
    "C28x": ["tixds510c28x"],
}


def make_ccxml(serial, device, core=None):
    """Write tools/benchmark/local/<device>_<serial>.ccxml. The core picks the debug drivers; it comes from
    data/boards.csv when the device name is a board id. CC13xx and CC26xx parts also need an ICEPick
    driver: make their file in CCStudio (File, New, Target Configuration) instead."""
    c.need(c.SAFE_SERIAL, serial, "probe serial")
    c.need(c.SAFE_DEVICE, device, "device")
    if not core:
        core = c.load_boards().get(device.lower(), {}).get("core", "")
    key = next((k for k in DRIVERS if core.startswith(k)), None)
    if not key or device.lower().startswith(("cc13", "cc26")):
        raise SystemExit(f"no driver list for core {core!r} ({device}); make the target configuration in CCStudio "
                         f"and save it as <board folder>/{device}_{serial}.ccxml")
    lines = "\n".join(f'            <instance XML_version="1.2" href="drivers/{d}.xml" id="drivers" xml="{d}.xml" xmlpath="drivers"/>'
                      for d in DRIVERS[key])
    if key.startswith("Cortex"):  # what CCStudio writes for an Arm LaunchPad: 1 MHz clock, SWD
        lines += """
            <property Type="choicelist" Value="1" id="The JTAG TCLK Frequency (MHz)">
                <choice Name="Fixed with user specified value" value="SPECIFIC">
                    <property Type="stringfield" Value="1MHz" id="-- Enter a value from 100.0kHz to 2.5MHz"/>
                </choice>
            </property>
            <property Type="choicelist" Value="2" id="SWD Mode Settings">
                <choice Name="SWD Mode - Aux COM port is target TDO pin" value="nothing"/>
            </property>"""
    c.LOCAL.mkdir(parents=True, exist_ok=True)
    p = c.LOCAL / f"{device}_{serial}.ccxml"
    p.write_text(CCXML.replace("@SERIAL@", serial).replace("@DEV@", device).replace("@DRIVERS@", lines))
    return p


def usb_devices():
    """[{vid, pid, serial, name}] for every USB device that is not a hub or a computer's own part."""
    sysname = c.system()
    found = []
    if sysname == "mac":
        out = subprocess.run(["system_profiler", "SPUSBDataType", "-json"], capture_output=True, text=True).stdout
        try:
            tree = json.loads(out)
        except json.JSONDecodeError:
            return []

        def walk(node):
            for x in node.get("_items", []):
                vid = str(x.get("vendor_id", ""))
                if vid and not vid.startswith("apple"):
                    found.append({"vid": vid.split()[0].replace("0x", "").lower(),
                                  "pid": str(x.get("product_id", "")).replace("0x", "").lower(),
                                  "serial": str(x.get("serial_num") or ""), "name": x.get("_name", "")})
                walk(x)
        for top in tree.get("SPUSBDataType", []):
            walk(top)
    elif sysname == "linux":
        for d in glob.glob("/sys/bus/usb/devices/*"):
            p = Path(d)
            if not (p / "idVendor").exists():
                continue
            rd = lambda n: (p / n).read_text().strip() if (p / n).exists() else ""
            if rd("bDeviceClass") == "09":  # hub
                continue
            found.append({"vid": rd("idVendor").lower(), "pid": rd("idProduct").lower(), "serial": rd("serial"),
                          "name": rd("product")})
    else:
        ps = ("Get-PnpDevice -PresentOnly | Where-Object { $_.InstanceId -like 'USB\\VID_*' } | "
              "Select-Object InstanceId, FriendlyName | ConvertTo-Json")
        out = subprocess.run(["powershell", "-NoProfile", "-Command", ps], capture_output=True, text=True).stdout
        try:
            items = json.loads(out or "[]")
        except json.JSONDecodeError:
            items = []
        for it in items if isinstance(items, list) else [items]:
            m = re.match(r"USB\\VID_([0-9A-F]{4})&PID_([0-9A-F]{4})(?:&MI_\d+)?\\(.*)", it.get("InstanceId", ""), re.I)
            if m and "&MI_" not in it["InstanceId"].upper():
                found.append({"vid": m.group(1).lower(), "pid": m.group(2).lower(),
                              "serial": m.group(3) if "&" not in m.group(3) else "", "name": it.get("FriendlyName") or ""})
    return found


def match_usb(dev, reg):
    """Family key, family name and what we already know about this serial."""
    fam = None
    for key, f in reg["usb_families"].items():
        if key.startswith("_"):
            continue
        pids = f["pid"] if isinstance(f["pid"], list) else [f["pid"]]
        if f["vid"] == dev["vid"] and dev["pid"] in pids:
            fam = key
            break
    known = reg["known_probes"].get(dev["serial"])
    item = next((it for it in reg["items"] if dev["serial"] and dev["serial"] in it.get("serials", [])), None)
    return fam, (reg["usb_families"][fam]["name"] if fam else None), known or (item and item["name"]), item


def board_for(device_xml, board_name, boards):
    """Board id in data/boards.csv for a CCStudio device name or board name."""
    cand = (device_xml or "").lower()
    if cand in boards:
        return cand
    for bid, b in boards.items():
        if b["name"].lower() in (board_name or "").lower() or (cand and cand in b["name"].lower()):
            return bid
    return None


def ccs_devices():
    """getConnectedDevices through CCStudio's debug MCP server: [{serialNumber, boardName, deviceXml}]."""
    if not c.ccs_running():
        return None, "CCStudio is not running"
    try:
        m = c.Mcp("debug")
        r = m.tool("getConnectedDevices", timeout=60)
        m.close()
    except Exception as e:
        return None, f"{type(e).__name__}: {e}"
    if not isinstance(r, list):
        return None, f"unexpected answer: {str(r)[:200]}"
    if not r:
        return [], "empty list (seen before CCStudio was restarted; restart it or use --probe)"
    return r, "ok"


def find_ccxml(serial):
    hits = glob.glob(str(c.ROOT / "*" / f"*_{serial}.ccxml")) + glob.glob(str(c.ROOT / "*" / "*" / f"*_{serial}.ccxml"))
    return Path(hits[0]) if hits else None


def dslite_read(dsl, ccxml, addr, words, out, tries=3):
    """Read words through DSLite into out (raw little-endian bytes) and return them as integers. Retries
    a failed read: DSLite has answered "Can't generate board data file" once when called right after
    another DSLite on the same machine (2026-10-08)."""
    err = ""
    for i in range(tries):
        if out.exists():
            out.unlink()
        r = subprocess.run([str(dsl), "memory", "-c", str(ccxml), "-r", f"0x{addr:08X},{words}", "-o", str(out)],
                           capture_output=True, text=True, timeout=300)
        if r.returncode == 0 and out.exists():
            return parse_words(out.read_bytes(), words)
        err = (r.stdout + r.stderr).strip()[-400:]
        time.sleep(2)
    raise RuntimeError(err)


def parse_words(raw, words):
    """DSLite -o writes raw bytes, little-endian; a text dump (one hex word per line) is read too."""
    if len(raw) >= words * 4 and not raw[:words * 4].isascii() or len(raw) == words * 4:
        return [int.from_bytes(raw[i:i + 4], "little") for i in range(0, words * 4, 4)]
    vals = re.findall(r"\b(?:0x)?([0-9A-Fa-f]{8})\b", raw.decode(errors="ignore"))
    return [int(v, 16) for v in vals]


def chip_id(serial, device=None, core=None, fresh=False):
    """Read CPUID and the family ID register through one probe. Returns a dict."""
    c.need(c.SAFE_SERIAL, serial, "probe serial")
    if device:
        c.need(c.SAFE_DEVICE, device, "device")
    ccs = c.find_ccs()
    dsl = c.find_dslite(ccs)
    if not dsl:
        return {"error": "DSLite not found (run doctor.py)"}
    ccxml = None if fresh else find_ccxml(serial)
    if ccxml and device and not ccxml.stem.lower().startswith(device.lower()):
        ccxml = None
    if not ccxml:
        if not device:
            return {"error": f"no *_{serial}.ccxml in a board folder; pass --device <targetdb device name>"}
        tdb = c.find_targetdb(ccs)
        if tdb and not (tdb / "devices" / f"{device}.xml").exists():
            return {"error": f"{device}.xml is not in {tdb / 'devices'}"}
        ccxml = make_ccxml(serial, device, core)
    dev = device or re.sub(r"_[^_]+$", "", ccxml.stem)
    res = {"serial": serial, "ccxml": str(ccxml), "device": dev}
    c.LOCAL.mkdir(parents=True, exist_ok=True)
    try:
        cpuid = dslite_read(dsl, ccxml, 0xE000ED00, 1, c.LOCAL / f"cpuid_{serial}.txt")[0]
        res["cpuid"] = f"0x{cpuid:08X}"
        res["core"] = CPU_PART.get((cpuid >> 4) & 0xFFF, "unknown Arm core")
    except Exception as e:
        res["cpuid_error"] = str(e)
    fam = next((k for k in ID_REGS if dev.lower().startswith(k)), None)
    if fam:
        reg = ID_REGS[fam]
        try:
            v = dslite_read(dsl, ccxml, reg["addr"], 1, c.LOCAL / f"devid_{serial}.txt")[0]
            res["device_id"] = f"{reg['name']} 0x{v:08X}: {reg['decode'](v)}"
            res["board"] = reg["known"].get(v)
        except Exception as e:
            res["device_id_error"] = str(e)
    if not res.get("board"):
        boards = c.load_boards()
        res["board"] = dev.lower() if dev.lower() in boards else None
        if res["board"]:
            res["board_from"] = "the target configuration's device name (ID value not in ID_REGS yet)"
    return res


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--no-ccs", action="store_true", help="skip CCStudio's MCP server")
    ap.add_argument("--probe", help="read the chip ID through this XDS110 serial only")
    ap.add_argument("--device", help="targetdb device name for --probe when no ccxml exists")
    ap.add_argument("--core", help="with --device: the core (Cortex-M0+, Cortex-M33, Cortex-M4, C28x) when the device is not a board id")
    ap.add_argument("--fresh", action="store_true", help="with --probe: write a new target configuration even if a board folder has one")
    ap.add_argument("--json", action="store_true")
    a = ap.parse_args()

    if a.probe:
        r = chip_id(a.probe, a.device, a.core, a.fresh)
        print(json.dumps(r, indent=1) if a.json else "\n".join(f"{k}: {v}" for k, v in r.items()))
        sys.exit(1 if "error" in r else 0)

    reg = c.load_arrivals()
    boards = c.load_boards()
    devs = usb_devices()
    ccs, why = (None, "skipped (--no-ccs)") if a.no_ccs else ccs_devices()
    by_serial = {d.get("serialNumber"): d for d in (ccs or [])}
    rows = []
    for d in devs:
        fam, famname, known, item = match_usb(d, reg)
        if not fam and not known:
            continue  # keyboards, hubs and the rest
        cd = by_serial.get(d["serial"])
        board = board_for(cd.get("deviceXml"), cd.get("boardName"), boards) if cd else None
        if not board and item:
            board = item.get("board_id")
        if not board and known:
            board = board_for(None, known, boards)
        how = "CCStudio getConnectedDevices" if cd else ("serial recorded in data/arrivals.json" if board else "USB family only")
        rows.append({"serial": d["serial"], "usb": f"{d['vid']}:{d['pid']}", "family": famname or d["name"],
                     "known_as": known, "ccs_board": cd.get("boardName") if cd else None,
                     "ccs_device": cd.get("deviceXml") if cd else None, "board": board, "identified_by": how})
    if a.json:
        print(json.dumps({"ccs": why, "boards": rows}, indent=1))
        return
    print(f"CCStudio MCP: {why}")
    if not rows:
        print("No development boards on USB.")
    for r in rows:
        name = r["ccs_board"] or r["known_as"] or r["family"]
        print(f"{r['serial'] or '(no serial)':10} {r['usb']}  {name}  ->  board: {r['board'] or 'unknown'}  ({r['identified_by']})")
    unknown = [r for r in rows if not r["board"]]
    if unknown:
        print("\nNot identified. Read the chip through its probe (read-only):")
        for r in unknown:
            dev = "" if find_ccxml(r["serial"]) else " --device <device name>"
            print(f"  python3 tools/benchmark/detect.py --probe {r['serial']}{dev}")
        print("Only the probe named is touched. Ask before reading a board another session may hold.")


if __name__ == "__main__":
    main()
