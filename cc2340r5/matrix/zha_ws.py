#!/usr/bin/env python3
"""Home Assistant websocket calls for the Zigbee OTA test (ZHA device list, one device, raw commands).
Reads the URL and token the same way as tools/ha.py; the token is never printed.
Usage: zha_ws.py devices [filter] | zha_ws.py device <ieee> | zha_ws.py raw '<json command without id>'"""
import asyncio, json, sys, pathlib, re
import websockets

cfg = json.load(open(pathlib.Path.home() / ".claude.json"))
def find(o):
    if isinstance(o, dict):
        if "home-assistant" in o and isinstance(o["home-assistant"], dict) and "url" in o["home-assistant"]:
            return o["home-assistant"]
        for v in o.values():
            r = find(v)
            if r: return r
mcp = find(cfg)
BASE = re.sub(r"/mcp_server/sse$", "", mcp["url"])
TOKEN = mcp["headers"]["Authorization"].split(" ", 1)[1]
WS = re.sub(r"^http", "ws", BASE) + "/api/websocket"

async def call(msgs, timeout=60):
    async with websockets.connect(WS, max_size=None) as ws:
        await ws.recv()
        await ws.send(json.dumps({"type": "auth", "access_token": TOKEN}))
        a = json.loads(await ws.recv())
        if a.get("type") != "auth_ok": raise SystemExit("auth failed")
        out = []
        for i, m in enumerate(msgs, 1):
            m = dict(m, id=i)
            await ws.send(json.dumps(m))
            while True:
                r = json.loads(await asyncio.wait_for(ws.recv(), timeout))
                if r.get("id") == i and r.get("type") == "result":
                    out.append(r); break
        return out

def short(d):
    return {k: d.get(k) for k in ("ieee", "nwk", "name", "user_given_name", "manufacturer", "model", "device_type",
                                   "available", "lqi", "rssi", "last_seen", "power_source", "sw_build_id")}

if __name__ == "__main__":
    cmd = sys.argv[1]
    if cmd == "devices":
        r = asyncio.run(call([{"type": "zha/devices"}]))[0]["result"]
        f = sys.argv[2].lower() if len(sys.argv) > 2 else ""
        for d in r:
            s = short(d)
            if f in json.dumps(s).lower(): print(json.dumps(s))
    elif cmd == "device":
        r = asyncio.run(call([{"type": "zha/device", "ieee": sys.argv[2]}]))[0]
        print(json.dumps(r.get("result") or r, indent=1)[:20000])
    elif cmd == "raw":
        r = asyncio.run(call([json.loads(sys.argv[2])], timeout=float(sys.argv[3]) if len(sys.argv) > 3 else 60))[0]
        print(json.dumps(r, indent=1)[:20000])
