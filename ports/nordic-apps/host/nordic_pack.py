#!/usr/bin/env python3
"""The Nordic-client check of the FRL Nordic image, from a Mac with bleak, the way nRF Blinky and nRF Toolbox use it:
find by name and by each service UUID, connect (timed, several times), list the services, write the LED on and off
and read the LED pin's output register through the probe, stand in for the button from the probe and catch the
Button notification, and time 100 Nordic UART lines.

  ~/.venvs/ble/bin/python host/nordic_pack.py <ccxml> [connects] [lines]

The probe steps call host/gpio_probe.py under CCStudio's Python (CCS env var RUN, default the CCS 20.1 path).
Hold the probe lock around the whole run (tools/frl/lock.py exec --serial <probe> -- ...)."""
import asyncio, os, statistics, subprocess, sys, time
from pathlib import Path
from bleak import BleakClient, BleakScanner

LBS = "00001523-1212-efde-1523-785feabcd123"
LBS_BUTTON = "00001524-1212-efde-1523-785feabcd123"
LBS_LED = "00001525-1212-efde-1523-785feabcd123"
NUS = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
NUS_RX = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
NUS_TX = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
NAME = "FRL Nordic"
HERE = Path(__file__).resolve().parent
RUN = os.environ.get("RUN", "/Applications/ti/ccs2101/ccs/scripting/python/run.sh")
CCXML = sys.argv[1]
CONNECTS = int(sys.argv[2]) if len(sys.argv) > 2 else 5
LINES = int(sys.argv[3]) if len(sys.argv) > 3 else 100


def probe(cmd):
    t = time.perf_counter()
    r = subprocess.run([RUN, str(HERE / "gpio_probe.py"), CCXML, cmd], capture_output=True, text=True, timeout=120)
    line = [l for l in r.stdout.splitlines() if l.startswith(cmd + ":")]
    out = line[-1] if line else "probe failed: " + (r.stdout + r.stderr)[-400:]
    print(f"  probe {out}  (call {time.perf_counter() - t:.1f} s)", flush=True)
    return out


async def find(uuid=None, timeout=20):
    t0 = time.perf_counter()
    found = asyncio.get_running_loop().create_future()

    def cb(dev, adv):
        ok = (uuid in [u.lower() for u in adv.service_uuids]) if uuid else (adv.local_name == NAME or dev.name == NAME)
        if ok and not found.done():
            found.set_result((dev, adv, time.perf_counter() - t0))

    async with BleakScanner(cb):
        return await asyncio.wait_for(found, timeout)


def stats(xs):
    xs = sorted(x * 1000 for x in xs)
    p95 = xs[min(len(xs) - 1, int(round(0.95 * (len(xs) - 1))))]
    return f"n {len(xs)}, median {statistics.median(xs):.1f} ms, min {xs[0]:.1f}, p95 {p95:.1f}, worst {xs[-1]:.1f}"


async def main():
    print("== scan", flush=True)
    for label, uuid in (("name", None), ("LBS UUID", LBS), ("NUS UUID", NUS)):
        dev, adv, dt = await find(uuid)
        print(f"  seen by {label} after {dt:.2f} s: name {adv.local_name!r}, rssi {adv.rssi}, uuids {sorted(adv.service_uuids)}", flush=True)

    print(f"== connect x{CONNECTS} (scan by LBS UUID, then connect; bleak's connect includes service discovery on macOS)", flush=True)
    scan_t, conn_t, fails = [], [], 0
    for i in range(CONNECTS):
        for attempt in range(3):
            dev, adv, dt = await find(LBS)
            c = BleakClient(dev, timeout=15)
            t = time.perf_counter()
            try:
                await c.connect()
                break
            except (TimeoutError, asyncio.TimeoutError, Exception) as e:
                fails += 1
                print(f"  {i + 1}: connect attempt {attempt + 1} failed after {time.perf_counter() - t:.1f} s: {type(e).__name__} {e}", flush=True)
                await asyncio.sleep(2)
        else:
            raise SystemExit("three connect attempts failed")
        conn_t.append(time.perf_counter() - t); scan_t.append(dt)
        n = sum(1 for _ in c.services)
        await c.disconnect()
        print(f"  {i + 1}: seen {dt:.2f} s, connect+discover {conn_t[-1] * 1000:.0f} ms, services {n}", flush=True)
        await asyncio.sleep(1.5)
    print(f"  scan to first sighting: {stats(scan_t)}", flush=True)
    print(f"  connect+discover: {stats(conn_t)}; failed attempts {fails}", flush=True)

    dev, adv, dt = await find(LBS)
    async with BleakClient(dev, timeout=15) as c:
        print("== services", flush=True)
        for s in c.services:
            print("  service", s.uuid, flush=True)
            for ch in s.characteristics:
                print("     ", ch.uuid, ",".join(ch.properties), flush=True)
        print("== LED (write, read back, then the pin's output register through the probe)", flush=True)
        for v in (1, 0, 1, 0):
            await c.write_gatt_char(LBS_LED, bytes([v]), response=True)
            back = (await c.read_gatt_char(LBS_LED)).hex()
            print(f"  wrote {v:02x}, characteristic reads {back}", flush=True)
            probe("read")
        alive = (await c.read_gatt_char(LBS_LED)).hex()
        print(f"  link still up after the probe reads: LED reads {alive}", flush=True)

        print("== Button (probe drives DIO10 low for about 2 s, then lets go)", flush=True)
        t0 = time.perf_counter()
        events = []
        await c.start_notify(LBS_BUTTON, lambda _, d: events.append((time.perf_counter() - t0, "Button", bytes(d).hex())))
        await c.start_notify(NUS_TX, lambda _, d: events.append((time.perf_counter() - t0, "UART TX", bytes(d))))
        await asyncio.sleep(1.0)
        before = len(events)
        tp = time.perf_counter() - t0
        await asyncio.to_thread(probe, "press")
        tpe = time.perf_counter() - t0
        await asyncio.sleep(2.0)
        tr = time.perf_counter() - t0
        await asyncio.to_thread(probe, "release")
        tre = time.perf_counter() - t0
        await asyncio.sleep(2.0)
        print(f"  stray events before press: {before}; press call {tp:.2f}-{tpe:.2f} s, release call {tr:.2f}-{tre:.2f} s", flush=True)
        for e in events:
            print(f"  {e[0]:6.2f} s  {e[1]} -> {e[2]!r}", flush=True)
        print("  Button reads now", (await c.read_gatt_char(LBS_BUTTON)).hex(), flush=True)
        await c.stop_notify(LBS_BUTTON)

        print(f"== Nordic UART: {LINES} lines, write without response to RX, time to the full echo on TX", flush=True)
        rx, evt = [], asyncio.Event()
        await c.stop_notify(NUS_TX)

        def on_tx(_, d):
            rx.append(bytes(d)); evt.set()

        await c.start_notify(NUS_TX, on_tx)
        await asyncio.sleep(0.3)
        rtt, bad, lost = [], 0, 0
        for i in range(LINES):
            line = f"nus line {i:03d} ok\n".encode()
            rx.clear(); evt.clear()
            t = time.perf_counter()
            await c.write_gatt_char(NUS_RX, line, response=False)
            got = b""
            try:
                while len(got) < len(line):
                    await asyncio.wait_for(evt.wait(), 3)
                    evt.clear(); got = b"".join(rx)
            except asyncio.TimeoutError:
                lost += 1
                continue
            rtt.append(time.perf_counter() - t)
            bad += got != line
            await asyncio.sleep(0.02)
        print(f"  {len(line)}-byte lines: {stats(rtt)}; wrong {bad}, lost {lost}", flush=True)
        await c.stop_notify(NUS_TX)
        print("  MTU seen by the Mac:", c.mtu_size, flush=True)


asyncio.run(main())
