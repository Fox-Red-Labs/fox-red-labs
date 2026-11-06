#!/usr/bin/env python3
"""Test the FRL Nordic image from a Mac with bleak: find the board by each Nordic service UUID,
list the services, drive the LED Button Service and the Nordic UART Service, and time them.

  ~/.venvs/ble/bin/python host/nordic_test.py scan [seconds]      time to first sighting, by each UUID
  ~/.venvs/ble/bin/python host/nordic_test.py full                everything below, with timings
  ~/.venvs/ble/bin/python host/nordic_test.py led on|off [secs]   set the LED, hold the link for secs
  ~/.venvs/ble/bin/python host/nordic_test.py button [secs]       wait for button notifications

The first run on a Mac asks whether Terminal may use Bluetooth. Allow it.
"""
import asyncio, statistics, sys, time
from bleak import BleakClient, BleakScanner

LBS = "00001523-1212-efde-1523-785feabcd123"
LBS_BUTTON = "00001524-1212-efde-1523-785feabcd123"
LBS_LED = "00001525-1212-efde-1523-785feabcd123"
NUS = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
NUS_RX = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
NUS_TX = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
NAME = "FRL Nordic"


async def find(uuid=None, timeout=15):
    """Return (device, adv, seconds until first sighting). Matches by service UUID when given, else by name."""
    t0 = time.perf_counter()
    found = asyncio.get_event_loop().create_future()

    def cb(dev, adv):
        ok = (uuid in [u.lower() for u in adv.service_uuids]) if uuid else (adv.local_name == NAME or dev.name == NAME)
        if ok and not found.done():
            found.set_result((dev, adv, time.perf_counter() - t0))

    async with BleakScanner(cb):
        return await asyncio.wait_for(found, timeout)


async def scan(secs):
    for label, uuid in (("name " + NAME, None), ("LED Button Service UUID", LBS), ("Nordic UART Service UUID", NUS)):
        try:
            dev, adv, dt = await find(uuid, secs)
            print(f"seen by {label}: {dt:.2f} s, name {adv.local_name!r}, rssi {adv.rssi}, service uuids {sorted(adv.service_uuids)}")
        except asyncio.TimeoutError:
            print(f"NOT seen by {label} in {secs} s")


def med(xs):
    return statistics.median(xs) * 1000


async def full():
    dev, adv, dt = await find(LBS, 20)
    print(f"seen after {dt:.2f} s: {adv.local_name!r} rssi {adv.rssi}")
    async with BleakClient(dev) as c:
        for s in c.services:
            print("service", s.uuid)
            for ch in s.characteristics:
                print("   ", ch.uuid, ch.properties)
        # ---- LED Button Service
        print("LED before:", (await c.read_gatt_char(LBS_LED)).hex())
        lat = []
        for i in range(20):
            v = bytes([(i + 1) % 2])
            t = time.perf_counter()
            await c.write_gatt_char(LBS_LED, v, response=True)
            lat.append(time.perf_counter() - t)
            await asyncio.sleep(0.05)
        back = (await c.read_gatt_char(LBS_LED)).hex()
        print(f"LED write round trips (with response), 20 writes: median {med(lat):.1f} ms, min {min(lat)*1000:.1f}, max {max(lat)*1000:.1f}; last write 00, read back {back}")
        await c.write_gatt_char(LBS_LED, b"\x01", response=True)
        print("LED after write 01 reads", (await c.read_gatt_char(LBS_LED)).hex())
        await c.write_gatt_char(LBS_LED, b"\x00", response=True)
        print("LED after write 00 reads", (await c.read_gatt_char(LBS_LED)).hex())
        print("Button reads", (await c.read_gatt_char(LBS_BUTTON)).hex())
        btn = []
        await c.start_notify(LBS_BUTTON, lambda _, d: btn.append(bytes(d)))
        await asyncio.sleep(1.0)
        await c.stop_notify(LBS_BUTTON)
        print("Button notifications in 1 s with nobody pressing:", btn)
        # ---- Nordic UART Service
        rx = []
        evt = asyncio.Event()

        def on_tx(_, d):
            rx.append(bytes(d))
            evt.set()

        await c.start_notify(NUS_TX, on_tx)
        await asyncio.sleep(0.3)
        rtt = []
        bad = 0
        for i in range(20):
            line = f"line {i:02d} hello\n".encode()
            rx.clear(); evt.clear()
            t = time.perf_counter()
            await c.write_gatt_char(NUS_RX, line, response=False)
            got = b""
            while got != line:
                await asyncio.wait_for(evt.wait(), 3)
                evt.clear()
                got = b"".join(rx)
                if len(got) >= len(line):
                    break
            rtt.append(time.perf_counter() - t)
            if got != line:
                bad += 1
            await asyncio.sleep(0.05)
        print(f"UART echo round trips, 20 lines of {len(line)} bytes (write without response to last TX notification): median {med(rtt):.1f} ms, min {min(rtt)*1000:.1f}, max {max(rtt)*1000:.1f}; wrong echoes {bad}")
        # one long line, to see the notification chunking
        long = (bytes(range(65, 91)) * 4)[:100] + b"\n"
        rx.clear(); evt.clear()
        await c.write_gatt_char(NUS_RX, long[:20], response=True)
        await c.write_gatt_char(NUS_RX, long[20:40], response=True)
        await c.write_gatt_char(NUS_RX, long[40:60], response=True)
        await c.write_gatt_char(NUS_RX, long[60:80], response=True)
        await c.write_gatt_char(NUS_RX, long[80:], response=True)
        await asyncio.sleep(1.5)
        print(f"101-byte line sent in 5 writes: echoed {len(b''.join(rx))} bytes in {len(rx)} notifications, identical: {b''.join(rx) == long}, sizes {[len(x) for x in rx]}")
        # two lines in one write
        rx.clear()
        await c.write_gatt_char(NUS_RX, b"a\nb\n", response=True)
        await asyncio.sleep(0.5)
        print("two lines in one write ->", b"".join(rx))
        await c.stop_notify(NUS_TX)
        print("MTU as seen by the Mac:", c.mtu_size)


async def led(on, hold):
    dev, adv, dt = await find(LBS, 20)
    async with BleakClient(dev) as c:
        await c.write_gatt_char(LBS_LED, bytes([1 if on else 0]), response=True)
        print("LED written", int(on), "reads back", (await c.read_gatt_char(LBS_LED)).hex(), flush=True)
        await asyncio.sleep(hold)


async def button(secs):
    dev, adv, dt = await find(LBS, 20)
    async with BleakClient(dev) as c:
        t0 = time.perf_counter()

        def on(_, d):
            print(f"{time.perf_counter()-t0:6.2f} s  Button -> {bytes(d).hex()}", flush=True)

        def on_tx(_, d):
            print(f"{time.perf_counter()-t0:6.2f} s  UART TX -> {bytes(d)!r}", flush=True)

        await c.start_notify(LBS_BUTTON, on)
        await c.start_notify(NUS_TX, on_tx)
        print(f"press the left button (BTN-1) for the next {secs} s", flush=True)
        await asyncio.sleep(secs)


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "full"
    if cmd == "scan":
        asyncio.run(scan(float(sys.argv[2]) if len(sys.argv) > 2 else 15))
    elif cmd == "full":
        asyncio.run(full())
    elif cmd == "led":
        asyncio.run(led(sys.argv[2] == "on", float(sys.argv[3]) if len(sys.argv) > 3 else 1))
    elif cmd == "button":
        asyncio.run(button(float(sys.argv[2]) if len(sys.argv) > 2 else 30))
    else:
        sys.exit(__doc__)
