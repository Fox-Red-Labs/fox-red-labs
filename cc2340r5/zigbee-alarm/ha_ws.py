#!/usr/bin/env python3
"""Home Assistant websocket helper for the Zigbee alarm run (token read from ~/.claude.json at run time, never printed).
Run with the esphome venv (has aiohttp; any Python with aiohttp works): ~/.venvs/findmy/bin/python ha_ws.py <cmd>
  zha                      list ZHA devices (ieee, name, model, type)
  cmd '<json>'             send one websocket command and print the result
  reconf IEEE              run ZHA's reconfigure (binds, writes the CIE address, enrolls)
  watch ENTITY [ENTITY..]  print every state change with the local receive time (ms), until Ctrl-C
"""
import asyncio, json, pathlib, re, sys, time, aiohttp

def cfg():
    c = json.load(open(pathlib.Path.home() / ".claude.json"))
    def find(o):
        if isinstance(o, dict):
            if "home-assistant" in o and isinstance(o["home-assistant"], dict) and "url" in o["home-assistant"]:
                return o["home-assistant"]
            for v in o.values():
                r = find(v)
                if r: return r
    m = find(c)
    base = re.sub(r"/mcp_server/sse$", "", m["url"])
    return base, m["headers"]["Authorization"].split(" ", 1)[1]

class WS:
    def __init__(self): self.i = 0
    async def __aenter__(self):
        base, tok = cfg()
        self.s = aiohttp.ClientSession()
        self.w = await self.s.ws_connect(base.replace("http", "ws", 1) + "/api/websocket", max_msg_size=0)
        await self.w.receive_json()
        await self.w.send_json({"type": "auth", "access_token": tok})
        r = await self.w.receive_json(); assert r["type"] == "auth_ok", r
        return self
    async def __aexit__(self, *a): await self.s.close()
    async def call(self, msg):
        self.i += 1; msg = dict(msg, id=self.i)
        await self.w.send_json(msg)
        while True:
            r = await self.w.receive_json()
            if r.get("id") == self.i and r["type"] == "result": return r

async def main():
    cmd = sys.argv[1]
    async with WS() as ws:
        if cmd == "zha":
            r = await ws.call({"type": "zha/devices"})
            for d in r["result"]:
                print(d["ieee"], d.get("user_given_name") or d.get("name"), d.get("manufacturer"), d.get("model"), d.get("device_type"), d.get("nwk"), d.get("available"))
        elif cmd == "cmd":
            r = await ws.call(json.loads(sys.argv[2])); print(json.dumps(r, indent=1)[:6000])
        elif cmd == "reconf":
            ws.i += 1
            await ws.w.send_json({"id": ws.i, "type": "zha/devices/reconfigure", "ieee": sys.argv[2]})
            t0 = time.time()
            while time.time() - t0 < 40:
                try: m = await asyncio.wait_for(ws.w.receive_json(), 40)
                except asyncio.TimeoutError: break
                print(json.dumps(m)[:300], flush=True)
        elif cmd == "watch":
            ents = sys.argv[2:]
            ws.i += 1
            await ws.w.send_json({"id": ws.i, "type": "subscribe_events", "event_type": "state_changed"})
            while True:
                m = await ws.w.receive_json()
                if m.get("type") == "event":
                    d = m["event"]["data"]
                    if d["entity_id"] in ents:
                        print(round(time.time() * 1000), d["entity_id"], d["new_state"]["state"], flush=True)
asyncio.run(main())
