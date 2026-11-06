#!/usr/bin/env python3
"""Listen for tool tags from this computer's Bluetooth and print every decoded frame.
    python scan.py [seconds] [--raw]
Needs bleak (~/.venvs/ble/bin/python). Whether a computer hands extended advertising to apps depends on its
Bluetooth chip and OS; the count of legacy (0x11) and extended (0x12) frames at the end says what this one did."""
import argparse
import asyncio
import os
import struct
import sys
import time

from bleak import BleakScanner

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "proto"))
import tt_proto as P  # noqa: E402


async def run(seconds, raw):
    counts = {}
    last = {}
    t0 = time.time()

    def cb(dev, adv):
        md = adv.manufacturer_data.get(P.COMPANY)
        if md is None or md[:2] != b"FR":
            return
        f = P.decode_msd(struct.pack("<H", P.COMPANY) + md)
        if f is None:
            return
        key = (dev.address, f["vt"])
        counts[key] = counts.get(key, 0) + 1
        if raw or last.get(key) != f["seq"]:
            last[key] = f["seq"]
            kind = "legacy" if f["vt"] == P.VT_LEGACY else "ext"
            print("%6.1f %s %-6s %s rssi=%d seq=%d trig=%d oc=%d flt=%d run=%ds %s" % (
                time.time() - t0, dev.address, kind, adv.local_name or "-", adv.rssi, f["seq"], f["trig"], f["oc"],
                f["flt"], f["run"], "flags=%d soc=%d tc=%d" % (f["flags"], f["soc"], f["tc"]) if kind == "legacy"
                else "rpm=%s tool=%s ev=%d" % (f.get("rpm"), f.get("tool"), len(f.get("events", [])))), flush=True)

    async with BleakScanner(cb):
        await asyncio.sleep(seconds)
    for (addr, vt), n in sorted(counts.items()):
        print("heard %s %s: %d frames" % (addr, "legacy" if vt == P.VT_LEGACY else "ext", n))
    if not counts:
        print("no tool tags heard")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("seconds", nargs="?", type=float, default=15)
    ap.add_argument("--raw", action="store_true")
    a = ap.parse_args()
    asyncio.run(run(a.seconds, a.raw))
