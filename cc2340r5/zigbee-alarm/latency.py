#!/usr/bin/env python3
"""UART line to Home Assistant state latency. Run with a Python that has aiohttp.
  latency.py PORT ZONE N   ZONE = SMOKE or CO; N state changes (1,0,1,0,...), 3 s apart.
t0 = Mac clock just before the line is written to the radio board's UART; t1 = Mac clock when the websocket delivers
the state_changed event for the entity. Both on this Mac, so the Mac to Home Assistant hop (a few ms) is inside the number."""
import asyncio, os, sys, termios, time, statistics, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import importlib.util
spec = importlib.util.spec_from_file_location("hw", os.path.join(os.path.dirname(os.path.abspath(__file__)), "ha_ws.py"))
src = open(spec.origin).read().replace("asyncio.run(main())", "")
hw = {}; exec(compile(src, "ha_ws", "exec"), hw)
ENT = {"SMOKE": "binary_sensor.fox_red_l_frl_smoke_ias_zone", "CO": "binary_sensor.fox_red_l_frl_smoke_ias_zone_2"}

async def main():
    port, zone, n = sys.argv[1], sys.argv[2], int(sys.argv[3])
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY)
    a = termios.tcgetattr(fd); a[0] = 0; a[1] = 0; a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL; a[3] = 0
    a[4] = a[5] = termios.B115200; termios.tcsetattr(fd, termios.TCSANOW, a)
    ent = ENT[zone]; res = []
    async with hw["WS"]() as ws:
        ws.i += 1
        await ws.w.send_json({"id": ws.i, "type": "subscribe_events", "event_type": "state_changed"})
        await ws.w.receive_json()
        for k in range(n):
            want = "on" if k % 2 == 0 else "off"
            line = f"{zone} {1 if want == 'on' else 0}\n".encode()
            t0 = time.time(); os.write(fd, line)
            ms = None
            while time.time() - t0 < 10:
                try: m = await asyncio.wait_for(ws.w.receive_json(), 10 - (time.time() - t0))
                except asyncio.TimeoutError: break
                if m.get("type") == "event":
                    d = m["event"]["data"]
                    if d["entity_id"] == ent and d["new_state"]["state"] == want:
                        ms = (time.time() - t0) * 1000; break
            res.append(ms); print(k + 1, line.decode().strip(), "->", None if ms is None else round(ms), "ms", flush=True)
            await asyncio.sleep(3)
    ok = [r for r in res if r is not None]
    if ok: print(f"{zone}: {len(ok)} of {n} landed, median {statistics.median(ok):.0f} ms, min {min(ok):.0f}, worst {max(ok):.0f}")
asyncio.run(main())
