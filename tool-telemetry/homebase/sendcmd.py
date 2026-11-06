#!/usr/bin/env python3
"""Send a signed command to one tool through a hub (stage two). Standard library only.
    python3 tool-telemetry/homebase/sendcmd.py --hub http://<hub address> --tag 40:F3:B0:xx:xx:xx lock|unlock|stop|run [--duty 20]
Signs with the fleet key (outside the repo), for that tag only, with a counter that always grows (seconds
since 1970, never less than the last one sent), so a recorded command never works twice. The hub connects to the tag,
the tag checks the signature and the counter and passes a $TC line to the tool MCU; the tool reports its new state.
Waits for the hub's verdict and prints it."""
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

STATE = os.path.abspath("out/tool-telemetry/cmd-counter.json")
RESULT = {0: "passed to the tool", 1: "refused: bad signature", 2: "refused: old counter (a replay)", 3: "refused: not a command",
          7: "still checking", 20: "hub could not write", 40: "tag did not answer the connection", 50: "timed out"}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--hub", required=True)
    ap.add_argument("--tag", required=True, help="the tag's address as the app shows it")
    ap.add_argument("cmd", choices=sorted(tt_cmd.CMDS))
    ap.add_argument("--duty", type=int, default=0, help="run only: percent, 0 to 40")
    ap.add_argument("--key", default=KEY_FILE)
    a = ap.parse_args()
    st = json.load(open(STATE)) if os.path.exists(STATE) else {}
    n = max(int(time.time()), st.get(a.tag, 0) + 1)          # fits 32 bits until 2106
    st[a.tag] = n
    os.makedirs(os.path.dirname(STATE), exist_ok=True)
    json.dump(st, open(STATE, "w"))
    w = tt_cmd.sign(load_key(a.key), a.tag, a.cmd, a.duty if a.cmd == "run" else 0, n)
    body = ("addr=%s&cmd=%s" % (a.tag, w.hex())).encode()
    r = json.load(urllib.request.urlopen(urllib.request.Request(a.hub.rstrip("/") + "/api/cmd", data=body, method="POST", headers={"X-Token": hub_token(), "Content-Type": "text/plain"}), timeout=10))
    if not r.get("ok"):
        raise SystemExit("hub refused: %s" % r)
    t0 = time.time()
    while time.time() - t0 < 40:
        time.sleep(1)
        s = json.load(urllib.request.urlopen(a.hub.rstrip("/") + "/api/cmd", timeout=10))
        if not s["pending"]:
            last = s["last"]
            print("%s %s: %s (%.1f s)" % (a.cmd, a.tag, RESULT.get(last["err"], "code %d" % last["err"]) if not last["ok"] else RESULT[0],
                                          time.time() - t0))
            return 0 if last["ok"] else 1
    raise SystemExit("no answer from the hub in 40 s")


if __name__ == "__main__":
    sys.exit(main())
