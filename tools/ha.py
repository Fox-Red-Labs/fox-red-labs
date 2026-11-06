#!/usr/bin/env python3
"""Minimal Home Assistant REST client. URL and token from HA_URL and HA_TOKEN (a long-lived access token, set in
the environment or a systemd EnvironmentFile), else from the home-assistant MCP entry in ~/.claude.json at run time.
The token is never printed or stored in the repo.
Usage: ha.py ping | ha.py states <prefix> | ha.py call <domain> <service> '<json data>' | ha.py get <path>"""
import json, sys, time, urllib.request, re, pathlib

import os

def find(o):
    if isinstance(o, dict):
        if "home-assistant" in o and isinstance(o["home-assistant"], dict) and "url" in o["home-assistant"]:
            return o["home-assistant"]
        for v in o.values():
            r = find(v)
            if r: return r
if os.environ.get("HA_URL") and os.environ.get("HA_TOKEN"):
    BASE, AUTH = os.environ["HA_URL"].rstrip("/"), "Bearer " + os.environ["HA_TOKEN"]
else:
    mcp = find(json.load(open(pathlib.Path.home() / ".claude.json")))
    if not mcp:
        sys.exit("Home Assistant: set HA_URL and HA_TOKEN")
    BASE = re.sub(r"/mcp_server/sse$", "", mcp["url"])
    AUTH = mcp["headers"]["Authorization"]

def req(method, path, data=None):
    r = urllib.request.Request(BASE + path, method=method, headers={"Authorization": AUTH, "Content-Type": "application/json"},
                               data=json.dumps(data).encode() if data is not None else None)
    t0 = time.time()
    with urllib.request.urlopen(r, timeout=30) as resp:
        body = resp.read().decode()
    return round((time.time() - t0) * 1000), json.loads(body) if body else None

if __name__ == "__main__":
    cmd = sys.argv[1]
    if cmd == "ping": print(req("GET", "/api/"))
    elif cmd == "states":
        ms, st = req("GET", "/api/states")
        for s in st:
            if s["entity_id"].startswith(sys.argv[2]): print(s["entity_id"], s["state"], s["attributes"].get("friendly_name"), s["last_changed"])
    elif cmd == "call":
        ms, out = req("POST", f"/api/services/{sys.argv[2]}/{sys.argv[3]}", json.loads(sys.argv[4]) if len(sys.argv) > 4 else {})
        print(ms, "ms", json.dumps(out)[:300])
    elif cmd == "get": print(json.dumps(req("GET", sys.argv[2])[1])[:2000])
