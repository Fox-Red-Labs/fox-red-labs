#!/usr/bin/env python3
"""Read the alarm listener's serial lines (alarm_detect image), log them, and pass events on:
a Home Assistant entity (sensor.alarm_listener: quiet, smoke, co, learning) plus a notification,
and a macOS notification. Logs every line with a timestamp to --log.

  listen.py [--port /dev/cu.usbmodemMG0200011] [--log FILE] [--ha] [--seconds N]

Home Assistant uses tools/ha.py (token read from ~/.claude.json at run time, never printed).
--notify-service names a notify service (for example notify.mobile_app_<phone>) for a phone push.
"""
import argparse
import os
import subprocess
import sys
import time

import serial

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../.."))
sys.path.insert(0, os.path.join(REPO, "tools"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/cu.usbmodemMG0200011")
    ap.add_argument("--log", default=os.path.abspath("out/alarm/listen.log"))
    ap.add_argument("--ha", action="store_true")
    ap.add_argument("--notify-service", default="")
    ap.add_argument("--seconds", type=float, default=0)
    ap.add_argument("--radio-port", action="append", default=[],
                    help="radio board serial port (PORT or PORT@BAUD, default 115200); repeatable. Each gets one line per change: "
                         "SMOKE 1, SMOKE 0, CO 1, CO 0 (the radio contract in RADIO-PLAN.md)")
    a = ap.parse_args()
    ha = None
    if a.ha:
        import ha as ha_mod
        ha = ha_mod

    def post(state, **attrs):
        if not ha:
            return
        at = {"friendly_name": "Alarm listener (MSPM0G5187)", "icon": "mdi:smoke-detector-variant-alert"}
        at.update(attrs)
        try:
            ha.req("POST", "/api/states/sensor.alarm_listener", {"state": state, "attributes": at})
        except Exception as e:
            print("HA post failed:", e, flush=True)

    def notify(title, msg):
        subprocess.run(["osascript", "-e", f'display notification "{msg}" with title "{title}" sound name "Sosumi"'])
        if ha:
            try:
                ha.req("POST", "/api/services/persistent_notification/create", {"title": title, "message": msg})
                if a.notify_service:
                    dom, svc = a.notify_service.split(".", 1)
                    ha.req("POST", f"/api/services/{dom}/{svc}", {"title": title, "message": msg})
            except Exception as e:
                print("HA notify failed:", e, flush=True)

    radios = []
    for rp in a.radio_port:
        port, _, baud = rp.partition("@")
        radios.append(serial.Serial(port, int(baud or 115200), timeout=0))

    def radio(line):
        for r in radios:
            r.write((line + "\n").encode())
            print(time.strftime("%H:%M:%S"), "radio", r.port, line, flush=True)

    os.makedirs(os.path.dirname(a.log), exist_ok=True)
    log = open(a.log, "a")
    s = serial.Serial(a.port, 921600, timeout=0.5)
    t_end = time.time() + a.seconds if a.seconds else None
    post("quiet")
    buf = b""
    while t_end is None or time.time() < t_end:
        buf += s.read(4096)
        while b"\n" in buf:
            ln, buf = buf.split(b"\n", 1)
            line = ln.decode(errors="replace").strip()
            if not line or line[0] not in "IALSFNC#":
                continue
            ts = time.strftime("%Y-%m-%d %H:%M:%S")
            log.write(f"{ts} {line}\n")
            log.flush()
            if not line.startswith("I "):
                print(ts, line, flush=True)
            if line.startswith("ALARM"):
                kind = line.split()[1]
                post(kind, detail=line)
                radio(("SMOKE" if kind == "smoke" else "CO") + " 1")
                notify("Smoke alarm is sounding" if kind == "smoke" else "CO alarm is sounding", line)
            elif line.startswith("CLEARED"):
                post("quiet")
                radio("SMOKE 0")
                radio("CO 0")
            elif line.startswith("LEARN:"):
                post("learning")
            elif line.startswith("LEARNED") or line.startswith("LEARN FAILED"):
                post("quiet", learned=line)


if __name__ == "__main__":
    main()
