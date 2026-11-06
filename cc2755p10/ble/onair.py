#!/usr/bin/env python3
"""Time from the end of a DSLite flash to the first advertisement the Mac sees.
   onair.py <name> <hex> [seconds]   (run with the bleak venv: ~/.venvs/ble/bin/python)
Starts the scanner first, flashes the image with DSLite (-e -f -u) for the LP-EM-CC2755P10, prints the host
time the flash command returned and the first time an advertisement named <name> arrived (both seconds from the scan start)."""
import asyncio, subprocess, sys, time
from bleak import BleakScanner
name, hexfile = sys.argv[1], sys.argv[2]; limit = float(sys.argv[3]) if len(sys.argv) > 3 else 30
t0 = time.time(); seen = []
def cb(d, adv):
    n = adv.local_name or d.name
    if n == name: seen.append((time.time() - t0, adv.rssi, d.address))
async def main():
    async with BleakScanner(cb):
        await asyncio.sleep(3)
        pre = len(seen)
        p = await asyncio.create_subprocess_exec("/Applications/ti/uniflash_9.6.0/dslite.sh", "--mode", "flash", "-c",
            "cc2755p10/CC2755P10.ccxml", "-e", "-f", "-u", hexfile, stdout=asyncio.subprocess.DEVNULL, stderr=asyncio.subprocess.DEVNULL)
        await p.wait(); tf = time.time() - t0
        end = time.time() + limit
        while time.time() < end and len(seen) == pre: await asyncio.sleep(0.02)
        print(f"before flash: {pre} advertisements named {name!r}")
        print(f"flash returned at {tf:.2f} s; first advertisement after it at {seen[pre][0]:.2f} s => on air {seen[pre][0]-tf:.2f} s after the flash returned (rssi {seen[pre][1]}, {seen[pre][2]})" if len(seen) > pre else "no advertisement seen")
asyncio.run(main())
