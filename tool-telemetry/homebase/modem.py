#!/usr/bin/env python3
"""The gateway's modem line, on the bench: read "@L {json}" and "@I {json}" lines from the gateway's UART and forward
them to the home base, the way a cellular modem on that UART would.
    python modem.py <gateway serial port> <home base URL> [--batch-s 5]
Items with no time from the gateway get one from this computer's clock (a modem has network time). Items wait in
memory while the home base is unreachable and go with the next batch. Needs pyserial."""
import argparse
import json
import os
import sys
import time
import urllib.request

import serial


def post(url, body):
    req = urllib.request.Request(url + "/api/ingest", json.dumps(body).encode(),
                                 {"Content-Type": "application/json", "X-Token": os.environ.get("FRL_HOMEBASE_TOKEN", "")})
    with urllib.request.urlopen(req, timeout=8) as r:
        return json.loads(r.read())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("port")
    ap.add_argument("homebase")
    ap.add_argument("--batch-s", type=float, default=5)
    ap.add_argument("--minutes", type=float, default=0, help="stop after this long, 0 = run until stopped")
    ap.add_argument("--log", help="also append every console line, time-stamped, to this file")
    a = ap.parse_args()
    s = serial.Serial(a.port, 115200, timeout=0.2)
    logf = open(a.log, "a") if a.log else None
    buf, pending, hub, hub_up, hub_up_at = b"", [], "?", None, None
    last_post = t0 = time.time()
    while not a.minutes or time.time() - t0 < a.minutes * 60:
        buf += s.read(4096)
        while b"\n" in buf:
            raw, buf = buf.split(b"\n", 1)
            line = raw.decode("utf-8", "replace").strip()
            if logf and line:
                logf.write("%.3f %s\n" % (time.time(), line))
                logf.flush()
            # the Wi-Fi driver prints some messages without a line end, so a record can follow other text
            for tag in ("@I {", "@L {"):
                if tag in line:
                    line = line[line.index(tag):]
            if line.startswith("@I "):
                try:
                    info = json.loads(line[3:])
                except ValueError:
                    continue
                hub = info.get("hub", hub)
                if "up" in info:
                    hub_up, hub_up_at = info["up"], time.time()
            elif line.startswith("@L "):
                try:
                    it = json.loads(line[3:])
                except ValueError:
                    continue
                if not it.get("unix") and it.get("up") is not None and hub_up is not None and hub_up_at is not None:
                    now_up = hub_up + (time.time() - hub_up_at)
                    it["unix"] = round(time.time() - (now_up - it["up"]))
                pending.append(it)
        if pending and time.time() - last_post >= a.batch_s:
            last_post = time.time()
            try:
                r = post(a.homebase, {"via": "modem", "hub": hub, "items": pending})
                print("%s sent %d, new %d" % (time.strftime("%H:%M:%S"), len(pending), r.get("new", 0)), flush=True)
                pending = []
            except Exception as e:
                print("%s home base unreachable (%s), holding %d" % (time.strftime("%H:%M:%S"), e, len(pending)),
                      file=sys.stderr, flush=True)


if __name__ == "__main__":
    main()
