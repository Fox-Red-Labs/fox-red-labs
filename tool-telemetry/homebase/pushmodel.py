#!/usr/bin/env python3
"""Hand the newest signed model to a hub, which then installs it on every tool tag it hears. Standard library only.
    python3 tool-telemetry/homebase/pushmodel.py --hub http://<hub address> [--from http://127.0.0.1:8090 | --file model-v2.json]
        [--wait 60]
This is the path a site uplink takes; the phone app does the same when it has the model and joins the hub's Wi-Fi.
With --wait it polls the hub's /api/model until every tool tag it hears reports the new version."""
import argparse
import json
import os
import time
import urllib.request

HUB_ENV = os.path.expanduser(os.environ.get("TTGW_SECRET_FILE", "~/.config/frl/tool-telemetry-hub.env"))


def hub_token():
    """The hub's control token (gateway/build.sh made it); posts that change tools need it."""
    try:
        return [l.split("=", 1)[1].strip() for l in open(HUB_ENV) if l.startswith("TTGW_CTRL_TOKEN=")][0]
    except (OSError, IndexError):
        return ""


def get(url):
    return json.load(urllib.request.urlopen(url, timeout=10))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--hub", required=True)
    ap.add_argument("--from", dest="src", default="http://127.0.0.1:8090", help="home base to fetch /api/model from")
    ap.add_argument("--file", help="a model-v<N>.json to send instead")
    ap.add_argument("--wait", type=int, default=0, help="seconds to wait for every tag to report the new version")
    a = ap.parse_args()
    m = json.load(open(a.file)) if a.file else get(a.src.rstrip("/") + "/api/model")
    for pkg in m["packages"]:
        req = urllib.request.Request(a.hub.rstrip("/") + "/api/model", data=pkg.encode(), method="POST", headers={"X-Token": hub_token(), "Content-Type": "text/plain"})
        r = json.load(urllib.request.urlopen(req, timeout=10))
        if not r.get("ok"):
            raise SystemExit("hub refused the package: %s" % r)
    print("hub has model v%d (%s)" % (m["version"], m.get("sha256", "")[:12]))
    end = time.time() + a.wait
    while a.wait and time.time() < end:
        s = get(a.hub.rstrip("/") + "/api/model")
        if s["tags"] and s["on_version"] == s["tags"] and s["state"] == 0:
            print("all %d tool tag(s) on v%d; installs ok %d, failed %d" % (s["tags"], m["version"], s["installs_ok"], s["installs_failed"]))
            return
        time.sleep(2)
    if a.wait:
        raise SystemExit("timed out: %s" % get(a.hub.rstrip("/") + "/api/model"))


if __name__ == "__main__":
    main()
