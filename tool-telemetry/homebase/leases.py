#!/usr/bin/env python3
"""Give a hub a batch of signed leases for one tool: the tool stays usable while a site hub keeps renewing, and locks
itself when the leases stop coming (it left the site, or was taken). Standard library only.
    python3 tool-telemetry/homebase/leases.py --hub http://<hub address> --tag 40:F3:B0:xx:xx:xx --minutes 2 --every 40 [--count 16]
    python3 tool-telemetry/homebase/leases.py --hub ... --tag ... --stop       (the hub stops renewing: the tag locks in time)
    python3 tool-telemetry/homebase/leases.py --hub ... --tag ... --end        (signed: ends the lease rule on the tag)
Each lease is a signed command (PROTOCOL.md section 7, command 5) with a growing counter, made here, where the key is;
the hub only hands them over, one every --every seconds, so a hub cut off from the home base keeps a site running
for count x every seconds and no longer."""
import argparse
import json
import os
import sys
import time
import urllib.request

HUB_ENV = os.path.expanduser(os.environ.get("TTGW_SECRET_FILE", "~/.config/frl/tool-telemetry-hub.env"))


def hub_token():
    """The hub's control token (gateway/build.sh made it); posts that change tools need it."""
    try:
        return [l.split("=", 1)[1].strip() for l in open(HUB_ENV) if l.startswith("TTGW_CTRL_TOKEN=")][0]
    except (OSError, IndexError):
        return ""

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "proto"))
sys.path.insert(0, os.path.join(HERE, "..", "ai"))
import tt_cmd  # noqa: E402
from model_pkg import load_key, KEY_FILE  # noqa: E402
from sendcmd import STATE  # noqa: E402


def post(hub, body):
    r = urllib.request.Request(hub.rstrip("/") + "/api/lease", data=body.encode(), method="POST", headers={"X-Token": hub_token(), "Content-Type": "text/plain"})
    return json.load(urllib.request.urlopen(r, timeout=10))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--hub", required=True)
    ap.add_argument("--tag", required=True)
    ap.add_argument("--minutes", type=int, default=2, help="how long one lease keeps the tool usable, 1 to 255")
    ap.add_argument("--every", type=int, default=40, help="seconds between renewals; well under the lease")
    ap.add_argument("--count", type=int, default=16, help="leases in the batch, up to 20")
    ap.add_argument("--stop", action="store_true")
    ap.add_argument("--end", action="store_true")
    ap.add_argument("--key", default=KEY_FILE)
    a = ap.parse_args()
    if a.stop:
        print(post(a.hub, "addr=%s&stop=1" % a.tag))
        return
    st = json.load(open(STATE)) if os.path.exists(STATE) else {}
    n0 = max(int(time.time()), st.get(a.tag, 0) + 1)
    count = 1 if a.end else max(1, min(a.count, 20))
    key = load_key(a.key)
    cmds = [tt_cmd.sign(key, a.tag, "lease", 0 if a.end else a.minutes, n0 + i) for i in range(count)]
    st[a.tag] = n0 + count - 1
    os.makedirs(os.path.dirname(STATE), exist_ok=True)
    json.dump(st, open(STATE, "w"))
    print(post(a.hub, "addr=%s&every=%d&cmds=%s" % (a.tag, a.every, "".join(c.hex() for c in cmds))))


if __name__ == "__main__":
    main()
