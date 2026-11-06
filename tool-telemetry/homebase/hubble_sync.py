#!/usr/bin/env python3
"""Bring Hubble's packets into the home base: where each tool was heard, and what its 13-byte message says.
Standard library only.
    python3 tool-telemetry/homebase/hubble_sync.py [--home http://127.0.0.1:8090] [--once] [--file packets.json]
Reads Hubble's packet stream (GET /api/org/<org>/packets, read-packets scope) with the API token kept outside the
repository, remembers its place with the Continuation-Token, decodes each payload with proto/hubble_payload.py and posts
the result to the home base (POST /api/hubble), which shows it on the fleet page. For ground packets the position is
the phone that heard the tag; satellites do not report one, so the message's own "last site" is what places it.
--file reads a saved API reply instead (the test uses one)."""
import argparse
import base64
import json
import os
import re
import sys
import time
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "proto"))
import hubble_payload  # noqa: E402

SECRETS = os.path.expanduser("~/.config/frl/hubble")
STATE = os.path.abspath("out/tool-telemetry/hubble-sync-state.json")


def records(reply):
    """Hubble's reply to our records: one per packet, payload decoded when it is ours."""
    out = []
    for p in reply.get("packets", []):
        dev, loc = p.get("device", {}), p.get("location") or {}
        try:
            raw = base64.b64decode(dev.get("payload") or "")
            msg = hubble_payload.decode(raw) if raw else None
        except (ValueError, TypeError):
            msg = None
        net = dev.get("network_type") or p.get("network") or ""
        out.append(dict(device=str(dev.get("id", ""))[:40], name=str(dev.get("name", ""))[:40], network=str(net)[:12],
                        unix=float(dev.get("timestamp") or 0), seq=dev.get("sequence_number"), rssi=dev.get("rssi"),
                        heard_lat=loc.get("latitude") if net != "SATELLITE" else None,
                        heard_lon=loc.get("longitude") if net != "SATELLITE" else None,
                        heard_acc=loc.get("horizontal_accuracy") if net != "SATELLITE" else None,
                        payload=raw.hex() if raw else "", msg=msg))
    return out


def fetch(org, token, cont):
    req = urllib.request.Request("https://api.hubble.com/api/org/%s/packets" % org,
                                 headers={"Authorization": "Bearer " + token, **({"Continuation-Token": cont} if cont else {})})
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.load(r), r.headers.get("Continuation-Token", ""), int(r.headers.get("Retry-After") or 60)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--home", default="http://127.0.0.1:8090")
    ap.add_argument("--once", action="store_true")
    ap.add_argument("--file", help="a saved packets reply, instead of the API")
    a = ap.parse_args()
    if a.file:
        recs = records(json.load(open(a.file)))
    else:
        token = open(os.path.join(SECRETS, "api-token.txt")).read().strip()
        org = re.search(r"Org ID: ([0-9a-f-]{36})", open(os.path.join(SECRETS, "README.txt")).read()).group(1)
        st = json.load(open(STATE)) if os.path.exists(STATE) else {}
        recs = []
        while True:
            reply, cont, wait = fetch(org, token, st.get("cont", ""))
            recs += records(reply)
            if cont:
                st["cont"] = cont
            json.dump(st, open(STATE, "w"))
            if not reply.get("packets") or a.once:
                break
    if recs:
        req = urllib.request.Request(a.home.rstrip("/") + "/api/hubble", data=json.dumps(recs).encode(), method="POST",
                                     headers={"Content-Type": "application/json", "X-Token": os.environ.get("FRL_HOMEBASE_TOKEN", "")})
        print(json.load(urllib.request.urlopen(req, timeout=10)))
    print("%d Hubble packet(s)" % len(recs))


if __name__ == "__main__":
    main()
