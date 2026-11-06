#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Give an LP-EM-CC35X1 its Wi-Fi network over Bluetooth LE, from a Mac, Linux or Windows computer.

The board runs the chained demo (cc35x1/chained-demo), which keeps TI's provisioning GATT service from the
ble_wifi_provisioning example: service 0xCC00, SSID 0xCC01, password 0xCC02, connection 0xCC03 (read starts the
join, notify reports it), security 0xCC04. Notification values: 1 join started, 2 joined, 3 failed.

  pip install bleak
  WIFI_PASSWORD='<password>' python3 provision.py --ssid '<network name>'
  python3 provision.py --ssid '<network name>' --password-file wifi.env    # a line WIFI_PASSWORD=...

The password is never printed. Exit code 0 when the board reports it joined, 3 when it reports a failure,
4 when nothing came back within --timeout seconds.
"""
import argparse, asyncio, os, sys, time
from bleak import BleakClient, BleakScanner

def uuid16(x):
    return f"0000{x:04x}-0000-1000-8000-00805f9b34fb"

SVC, SSID, PASS, CONN, SEC = (uuid16(u) for u in (0xCC00, 0xCC01, 0xCC02, 0xCC03, 0xCC04))
SECURITY = {"open": 0, "wpa2": 2, "wpa3": 12}   # WLAN_SEC_TYPE_* in TI's wlan_if.h
STATUS = {1: "join started", 2: "joined", 3: "join failed"}


def read_password(args):
    if args.password_file:
        for line in open(args.password_file):
            if line.startswith("WIFI_PASSWORD="):
                return line.split("=", 1)[1].strip().strip("'\"")
        sys.exit("no WIFI_PASSWORD= line in the password file")
    pw = os.environ.get("WIFI_PASSWORD")
    if pw is None and args.security != "open":
        sys.exit("set WIFI_PASSWORD or pass --password-file")
    return pw or ""


async def provision(ssid, password, security="wpa2", name="TI CC3xxx", timeout=40.0, log=print):
    """Returns (result, times) where result is 'joined', 'failed' or 'timeout' and times are perf_counter stamps."""
    t = {"start": time.perf_counter()}
    dev = await BleakScanner.find_device_by_name(name, timeout=30.0)
    if dev is None:
        return "not-found", t
    t["found"] = time.perf_counter()
    done = asyncio.Event()
    result = {"r": "timeout"}

    def on_note(_, data):
        v = data[0] if data else 0
        t.setdefault(f"note{v}", time.perf_counter())
        log(f"board says: {STATUS.get(v, v)}")
        if v in (2, 3):
            result["r"] = "joined" if v == 2 else "failed"
            done.set()

    async with BleakClient(dev, timeout=20.0) as c:
        t["connected"] = time.perf_counter()
        await c.start_notify(CONN, on_note)
        await c.write_gatt_char(SSID, ssid.encode(), response=True)
        if password:
            await c.write_gatt_char(PASS, password.encode(), response=True)
        await c.write_gatt_char(SEC, bytes([SECURITY[security]]), response=True)
        t["written"] = time.perf_counter()
        await c.read_gatt_char(CONN)          # the read is what starts the join
        t["requested"] = time.perf_counter()
        try:
            await asyncio.wait_for(done.wait(), timeout)
        except asyncio.TimeoutError:
            pass
        t["end"] = time.perf_counter()
    return result["r"], t


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ssid", required=True)
    ap.add_argument("--password-file")
    ap.add_argument("--security", choices=sorted(SECURITY), default="wpa2")
    ap.add_argument("--name", default="TI CC3xxx", help="advertised name (TI's default)")
    ap.add_argument("--timeout", type=float, default=40.0)
    a = ap.parse_args()
    r, t = asyncio.run(provision(a.ssid, read_password(a), a.security, a.name, a.timeout))
    if "requested" in t:
        print(f"{r}: {t['end'] - t['requested']:.2f} s after the connect request")
    else:
        print(r)
    sys.exit({"joined": 0, "failed": 3}.get(r, 4))


if __name__ == "__main__":
    main()
