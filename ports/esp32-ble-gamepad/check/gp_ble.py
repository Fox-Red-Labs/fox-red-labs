#!/usr/bin/env python3
"""FRL-GP1 Bluetooth check from the Mac (bleak). Run with the ble venv python.

  gp_ble.py [name] [scan_secs] [connects]

1. scans scan_secs (default 30) and counts the advertisements macOS reports for the device named `name`
   (default "ESP32 BLE Gamepad"); prints name, service UUIDs, RSSI median, reports per second.
2. connects `connects` times (default 5), prints the connect time of each (ms from connect() call to return),
   and on the first connection reads Device Information (manufacturer, model, software/firmware/hardware revision,
   serial), battery level and PnP ID, and lists the GATT services macOS exposes (HID 0x1812 is hidden by macOS).
Exit 0 when the device was found and connected.
"""
import asyncio, sys, time, statistics
from bleak import BleakScanner, BleakClient
NAME = sys.argv[1] if len(sys.argv) > 1 else "ESP32 BLE Gamepad"
SCAN = float(sys.argv[2]) if len(sys.argv) > 2 else 30.0
CONN = int(sys.argv[3]) if len(sys.argv) > 3 else 5
CH = {"2a29": "manufacturer", "2a24": "model", "2a25": "serial", "2a26": "firmware", "2a27": "hardware", "2a28": "software"}
async def main():
    seen = []; dev = None
    def cb(d, adv):
        nonlocal dev
        if (adv.local_name or d.name) == NAME:
            dev = d; seen.append((time.time(), adv.rssi, tuple(adv.service_uuids)))
    t0 = time.time()
    async with BleakScanner(cb): await asyncio.sleep(SCAN)
    if not seen: print(f"BLE CHECK: FAIL (no advertisement named {NAME!r} in {SCAN:.0f} s)"); return 1
    ts = [t for t, _, _ in seen]
    gaps = [b - a for a, b in zip(ts, ts[1:])]
    print(f"advertisements seen: {len(seen)} in {SCAN:.0f} s = {len(seen)/SCAN:.2f}/s, median gap {statistics.median(gaps)*1000:.0f} ms" if gaps else f"advertisements seen: {len(seen)}")
    print("rssi median:", statistics.median([r for _, r, _ in seen]), "service uuids:", sorted(set(u for _, _, us in seen for u in us)))
    times = []; first = True
    for i in range(CONN):
        c = BleakClient(dev, timeout=15)
        t = time.time()
        try:
            await c.connect(); ms = (time.time() - t) * 1000; times.append(ms)
            print(f"connect {i+1}: {ms:.0f} ms")
            if first:
                first = False
                print("services:", sorted(s.uuid[4:8] if s.uuid.endswith("-0000-1000-8000-00805f9b34fb") else s.uuid for s in c.services))
                for s in c.services:
                    for ch in s.characteristics:
                        u = ch.uuid[4:8]
                        if "read" in ch.properties and u in CH or u in ("2a19", "2a50"):
                            try:
                                v = bytes(await c.read_gatt_char(ch))
                                print(f"  {CH.get(u, 'battery' if u == '2a19' else 'pnp_id')}: {v.hex() if u in ('2a19', '2a50') else v.decode('utf-8', 'replace')}")
                            except Exception as e: print(f"  {u}: read failed {e}")
                await asyncio.sleep(3)   # let the board's example loop run on the connection
            await c.disconnect()
        except Exception as e:
            print(f"connect {i+1}: FAILED {e}")
        await asyncio.sleep(2)
    if times: print(f"connect median {statistics.median(times):.0f} ms over {len(times)} (min {min(times):.0f}, max {max(times):.0f})")
    print("BLE CHECK:", "PASS" if times else "FAIL")
    return 0 if times else 1
sys.exit(asyncio.run(main()))
