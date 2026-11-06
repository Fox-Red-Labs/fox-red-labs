#!/usr/bin/env python3
"""The Nordic-client checks that cannot survive a debugger access, run with the probe left alone while the link is up.
A debugger attach or memory read halts or resets the CC2340R5 and drops the Bluetooth link (2026-11-02), so this
script never touches the probe during a link: results come back over Bluetooth itself, the red LED pin is read through the probe
(host/gpio_probe.py) only after the link is closed.

  ~/.venvs/ble/bin/python host/nordic_nolink.py <outdir> [lines] [writes] [presses] [final LED 0|1]

Needs the test build (FRL_TEST_BTN=1 ./build.sh): its UART lines "btn1" and "btn0" act like BTN-1 pressed and released,
so the Button notification can be timed with no debugger and no hand. Times are the Mac's clock."""
import asyncio, statistics, subprocess, sys, time
from pathlib import Path
from bleak import BleakClient, BleakScanner

LBS = "00001523-1212-efde-1523-785feabcd123"
LBS_BUTTON = "00001524-1212-efde-1523-785feabcd123"
LBS_LED = "00001525-1212-efde-1523-785feabcd123"
NUS_RX = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
NUS_TX = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
NAME = "FRL Nordic"
OUT = Path(sys.argv[1]); OUT.mkdir(parents=True, exist_ok=True)
LINES = int(sys.argv[2]) if len(sys.argv) > 2 else 100
WRITES = int(sys.argv[3]) if len(sys.argv) > 3 else 100
PRESSES = int(sys.argv[4]) if len(sys.argv) > 4 else 20
FINAL = int(sys.argv[5]) if len(sys.argv) > 5 else 1
REPO = Path(__file__).resolve().parents[3]


def stats(xs):
    xs = sorted(x * 1000 for x in xs)
    p95 = xs[min(len(xs) - 1, int(round(0.95 * (len(xs) - 1))))]
    return f"n {len(xs)}, median {statistics.median(xs):.1f} ms, min {xs[0]:.1f}, p95 {p95:.1f}, worst {xs[-1]:.1f}"


async def find(timeout=20):
    fut = asyncio.get_running_loop().create_future()

    def cb(dev, adv):
        if (adv.local_name == NAME or dev.name == NAME) and not fut.done():
            fut.set_result(dev)

    async with BleakScanner(cb):
        return await asyncio.wait_for(fut, timeout)


async def main():
    dev = await find()
    t = time.perf_counter()
    async with BleakClient(dev, timeout=15) as c:
        print(f"connected in {(time.perf_counter() - t) * 1000:.0f} ms, MTU {c.mtu_size}", flush=True)

        print(f"== Nordic UART echo: {LINES} lines of 19 bytes, write without response, wait for the full echo", flush=True)
        rx, evt = [], asyncio.Event()

        def on_tx(_, d):
            rx.append(bytes(d)); evt.set()

        await c.start_notify(NUS_TX, on_tx)
        await asyncio.sleep(0.3)
        rtt, bad, lost = [], 0, 0
        for i in range(LINES):
            line = f"nus line {i:03d} okay!\n".encode()
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
        print(f"  ECHO {len(line)}-byte lines: {stats(rtt)}; wrong {bad}, lost {lost}", flush=True)

        print(f"== LED: {WRITES} writes with response, each read back over the link", flush=True)
        wr, bad = [], 0
        for i in range(WRITES):
            v = (i + 1) % 2          # 1,0,1,0 ... ends on 0
            t = time.perf_counter()
            await c.write_gatt_char(LBS_LED, bytes([v]), response=True)
            wr.append(time.perf_counter() - t)
            back = await c.read_gatt_char(LBS_LED)
            bad += back != bytes([v])
        print(f"  LED write RTT: {stats(wr)}; readback wrong {bad}", flush=True)
        print(f"  LED left at {FINAL:02x} for the pin read after the link closes", flush=True)

        print(f"== Button notify: {PRESSES} presses by the btn1 line, time to the Button notification", flush=True)
        await c.stop_notify(NUS_TX)
        ev, got = asyncio.Event(), []

        def on_btn(_, d):
            got.append((time.perf_counter(), bytes(d))); ev.set()

        await c.start_notify(LBS_BUTTON, on_btn)
        await asyncio.sleep(0.3)
        lat, miss, wrong = [], 0, 0
        for i in range(PRESSES):
            for cmd, want in ((b"btn1\n", b"\x01"), (b"btn0\n", b"\x00")):
                got.clear(); ev.clear()
                t = time.perf_counter()
                await c.write_gatt_char(NUS_RX, cmd, response=False)
                try:
                    await asyncio.wait_for(ev.wait(), 3)
                except asyncio.TimeoutError:
                    miss += 1
                    continue
                dt = got[0][0] - t
                wrong += got[0][1] != want
                if cmd == b"btn1\n":
                    lat.append(dt)
                await asyncio.sleep(0.05)
        print(f"  BUTTON notify (press edge): {stats(lat)}; missed {miss}, wrong value {wrong}", flush=True)
        await c.stop_notify(LBS_BUTTON)
    print("link closed", flush=True)


asyncio.run(main())
