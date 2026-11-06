#!/usr/bin/env python3
"""The Wi-Fi uplink path: the home base reads a hub that is on the same network (site Wi-Fi, a crew's router, a
cellular router) and stores what is new. Standard library only.
    python3 hubsync.py http://<hub address> http://localhost:8090 [--every 30] [--once]
Keeps its place per hub in a small state file, so each run sends only what is new; the home base drops anything it
already has from another path."""
import argparse
import json
import os
import time
import urllib.request

STATE = os.path.abspath("out/tool-telemetry/hubsync-state.json")


def get(url, timeout=6):
    with urllib.request.urlopen(url, timeout=timeout) as r:
        return json.loads(r.read())


def post(url, body):
    req = urllib.request.Request(url, json.dumps(body).encode(),
                                 {"Content-Type": "application/json", "X-Token": os.environ.get("FRL_HOMEBASE_TOKEN", "")})
    with urllib.request.urlopen(req, timeout=15) as r:
        return json.loads(r.read())


def sync(hub, home, state):
    info = get(hub + "/api/info")
    key = info["hub"]
    cur = state.get(key, {})
    if cur.get("up", 0) > info["up"] or cur.get("next", 0) > info["recs"]:
        cur = {}                                     # the hub restarted: its log starts at 0 again
    nxt, items = cur.get("next", 0), []
    for _ in range(50):
        page = get(hub + "/api/log?since=%d" % nxt)
        now = time.time()
        for it in page["items"]:
            if not it.get("unix") and it.get("up") is not None:
                it["unix"] = round(now - (info["up"] - it["up"]))
            items.append(it)
        nxt = page["next"]
        if not page.get("more"):
            break
    tags = get(hub + "/api/tags")
    r = post(home + "/api/ingest", {"via": "wifi", "hub": key, "items": items, "tags": tags}) if items or tags else {}
    state[key] = {"next": nxt, "up": info["up"]}
    return key, len(items), r.get("new", 0)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("hub")
    ap.add_argument("home")
    ap.add_argument("--every", type=float, default=30)
    ap.add_argument("--once", action="store_true")
    a = ap.parse_args()
    state = json.load(open(STATE)) if os.path.exists(STATE) else {}
    while True:
        try:
            key, n, new = sync(a.hub.rstrip("/"), a.home.rstrip("/"), state)
            json.dump(state, open(STATE, "w"))
            print("%s %s: %d records read, %d new at home base" % (time.strftime("%H:%M:%S"), key, n, new), flush=True)
        except Exception as e:
            print("%s hub or home base unreachable: %s" % (time.strftime("%H:%M:%S"), e), flush=True)
        if a.once:
            break
        time.sleep(a.every)


if __name__ == "__main__":
    main()
