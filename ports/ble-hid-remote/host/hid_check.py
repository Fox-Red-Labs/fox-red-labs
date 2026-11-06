#!/usr/bin/env python3
"""See the remote from the Mac with bleak (Bluetooth only; no pairing needed for the scan).

  ~/.venvs/ble/bin/python host/hid_check.py scan [SECONDS]
      Prints the first advertisement from "FRL Remote" with its wall-clock time, RSSI,
      service UUIDs and appearance, then every packet count. Exit 1 if none is seen.
  ~/.venvs/ble/bin/python host/hid_check.py connect
      Connects, lists every service and characteristic the Mac lets an app see, and reads
      Battery Level, PnP ID and (if visible) the HID Report Map. macOS may ask to pair.
"""
import asyncio
import sys
import time

from bleak import BleakClient, BleakScanner

NAME = "FRL Remote"
HID = "00001812-0000-1000-8000-00805f9b34fb"


def u16(x):
    return "0000%s-0000-1000-8000-00805f9b34fb" % x


async def scan(secs):
    first, count = [None], [0]

    def cb(dev, adv):
        if adv.local_name == NAME or HID in (adv.service_uuids or []):
            count[0] += 1
            if first[0] is None:
                first[0] = time.time()
                print("first seen %.3f  rssi %s  name %r  uuids %s  manufacturer %s" % (
                    first[0], adv.rssi, adv.local_name, adv.service_uuids, adv.manufacturer_data))
    async with BleakScanner(cb):
        t0 = time.time()
        while time.time() - t0 < secs:
            await asyncio.sleep(0.05)
    print("packets %d in %.0f s" % (count[0], secs))
    return 0 if first[0] else 1


async def connect():
    dev = await BleakScanner.find_device_by_filter(lambda d, a: a.local_name == NAME or HID in (a.service_uuids or []), timeout=20)
    if dev is None:
        print("not found")
        return 1
    print("found", dev)
    async with BleakClient(dev, timeout=30) as c:
        print("connected; MTU", c.mtu_size)
        for s in c.services:
            print("service", s.uuid, s.description)
            for ch in s.characteristics:
                print("   char", ch.uuid, ch.properties, ch.description)
        for label, uuid in (("battery level", u16("2a19")), ("PnP ID", u16("2a50")),
                            ("HID information", u16("2a4a")), ("report map", u16("2a4b"))):
            try:
                v = await c.read_gatt_char(uuid)
                print("%s: %s" % (label, bytes(v).hex()))
            except Exception as e:  # CoreBluetooth hides the HID service from apps
                print("%s: not readable from an app (%s)" % (label, type(e).__name__ + ": " + str(e)[:120]))
    return 0


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else "scan"
    if mode == "scan":
        sys.exit(asyncio.run(scan(float(sys.argv[2]) if len(sys.argv) > 2 else 30)))
    sys.exit(asyncio.run(connect()))
