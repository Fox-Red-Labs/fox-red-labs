#!/usr/bin/env python3
"""List, permit-join and remove ZHA devices through Home Assistant (same URL and token source as tools/ha.py; the token is never printed).

  ~/.venvs/esp/bin/python esp32c6/zigbee/zha_tool.py list
  ~/.venvs/esp/bin/python esp32c6/zigbee/zha_tool.py permit [seconds]
  ~/.venvs/esp/bin/python esp32c6/zigbee/zha_tool.py remove <ieee>      (zha.remove service, then lists again)
  ~/.venvs/esp/bin/python esp32c6/zigbee/zha_tool.py entities <ieee>    (entity ids and states of one device)
"""
import asyncio, json, pathlib, re, sys
import websockets

def ha():
    cfg = json.load(open(pathlib.Path.home() / ".claude.json"))
    def find(o):
        if isinstance(o, dict):
            if "home-assistant" in o and isinstance(o["home-assistant"], dict) and "url" in o["home-assistant"]: return o["home-assistant"]
            for v in o.values():
                r = find(v)
                if r: return r
    m = find(cfg); base = re.sub(r"/mcp_server/sse$", "", m["url"])
    return base.replace("http", "ws", 1) + "/api/websocket", m["headers"]["Authorization"].split()[-1]

async def run(argv):
    url, tok = ha()
    async with websockets.connect(url, max_size=2**24) as ws:
        await ws.recv(); await ws.send(json.dumps({"type": "auth", "access_token": tok}))
        assert json.loads(await ws.recv())["type"] == "auth_ok"
        n = 0
        async def call(msg):
            nonlocal n
            n += 1; await ws.send(json.dumps(dict(msg, id=n)))
            while True:
                r = json.loads(await ws.recv())
                if r.get("id") == n: return r
        if argv[0] == "list":
            r = await call({"type": "zha/devices"})
            for d in r.get("result", []):
                print(d["ieee"], "|", d.get("user_given_name") or d.get("name"), "|", d.get("manufacturer"), d.get("model"), "| lqi", d.get("lqi"), "rssi", d.get("rssi"), "| available", d.get("available"))
        elif argv[0] == "permit":
            r = await call({"type": "call_service", "domain": "zha", "service": "permit", "service_data": {"duration": int(argv[1]) if len(argv) > 1 else 180}})
            print("permit", r.get("success"), json.dumps(r.get("error")) if not r.get("success") else "")
        elif argv[0] == "remove":
            r = await call({"type": "call_service", "domain": "zha", "service": "remove", "service_data": {"ieee": argv[1]}})
            print("remove", r.get("success"), json.dumps(r.get("error")) if not r.get("success") else "")
        elif argv[0] == "entities":
            r = await call({"type": "zha/device"}) if False else None
            r = await call({"type": "config/entity_registry/list"})
            dv = await call({"type": "config/device_registry/list"})
            ids = [d["id"] for d in dv["result"] if any(i[0] == "zha" and i[1] == argv[1] for i in d.get("identifiers", []))]
            for e in r["result"]:
                if e.get("device_id") in ids: print(e["entity_id"], e.get("original_name"))
asyncio.run(run(sys.argv[1:]))
