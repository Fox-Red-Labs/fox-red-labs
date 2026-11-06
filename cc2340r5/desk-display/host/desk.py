#!/usr/bin/env python3
"""Push text to the FRL Desk display (LP-EM-CC2340R5 + SSD1306 OLED) over Bluetooth LE.

  ~/.venvs/ble/bin/python host/desk.py test            connect, send 3 lines, check each echo, print RSSI and round-trip times
  ~/.venvs/ble/bin/python host/desk.py msg "hello"     show a message on line 6
  ~/.venvs/ble/bin/python host/desk.py clock           time, date and (if DESK_LAT and DESK_LON are set) weather, until Ctrl-C
Weather comes from Open-Meteo (https://open-meteo.com, no key). Set DESK_LAT and DESK_LON to your own coordinates;
nothing is sent anywhere without them. The display shows 8 lines of 21 characters; the board takes "N:text" (N 1..8) and "cls".
"""
import asyncio, json, os, statistics, sys, time, urllib.request
from bleak import BleakClient, BleakScanner

NUS = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
RX = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
TX = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
NAME = "FRL Desk"


async def connect():
    found = asyncio.get_event_loop().create_future()
    def cb(dev, adv):
        if (adv.local_name == NAME or dev.name == NAME) and not found.done():
            found.set_result((dev, adv))
    async with BleakScanner(cb):
        dev, adv = await asyncio.wait_for(found, 20)
    print(f"seen {NAME}, rssi {adv.rssi}")
    c = BleakClient(dev)
    await c.connect(timeout=15)
    c.rx = asyncio.Queue()
    await c.start_notify(TX, lambda _, d: c.rx.put_nowait(bytes(d)))
    return c, adv.rssi


async def send(c, line, wait=True):
    t0 = time.perf_counter()
    await c.write_gatt_char(RX, (line + "\n").encode(), response=False)
    if not wait:
        return None
    got = b""
    while not got.endswith(b"\n"):
        got += await asyncio.wait_for(c.rx.get(), 3)
    return got.decode().strip(), (time.perf_counter() - t0) * 1000


async def test():
    c, rssi = await connect()
    times = []
    try:
        for line in ["1:FRL desk display", "3:BLE test line", "cls"]:
            echo, ms = await send(c, line)
            print(f"sent {line!r} echo {echo!r} {ms:.1f} ms {'OK' if echo == line else 'MISMATCH'}")
            times.append(ms)
        print(f"median round trip {statistics.median(times):.1f} ms, advertised rssi {rssi}")
    finally:
        await c.disconnect()


def weather():
    lat, lon = os.environ.get("DESK_LAT"), os.environ.get("DESK_LON")
    if not (lat and lon):
        return None
    url = f"https://api.open-meteo.com/v1/forecast?latitude={lat}&longitude={lon}&current=temperature_2m,wind_speed_10m&temperature_unit=fahrenheit&wind_speed_unit=mph"
    with urllib.request.urlopen(url, timeout=8) as r:
        cur = json.load(r)["current"]
    return f"{cur['temperature_2m']:.0f}F wind {cur['wind_speed_10m']:.0f}mph"


async def clock():
    c, _ = await connect()
    wx, wx_at = None, 0
    try:
        while True:
            now = time.localtime()
            if time.time() - wx_at > 600:
                try:
                    wx = weather() or "set DESK_LAT/LON"
                except Exception as e:
                    wx = "weather: " + type(e).__name__
                wx_at = time.time()
            for line in (time.strftime("2:%H:%M:%S", now), time.strftime("3:%a %d %b %Y", now), "5:" + wx):
                await send(c, line)
            await asyncio.sleep(1)
    finally:
        await c.disconnect()


async def msg(text):
    c, _ = await connect()
    try:
        print(await send(c, "6:" + text))
    finally:
        await c.disconnect()


if __name__ == "__main__":
    a = sys.argv[1:]
    if a[:1] == ["test"]: asyncio.run(test())
    elif a[:1] == ["clock"]: asyncio.run(clock())
    elif a[:1] == ["msg"] and len(a) > 1: asyncio.run(msg(" ".join(a[1:])))
    else: print(__doc__)
