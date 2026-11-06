#!/usr/bin/env python3
"""A wire made of USB: copy the tool MCU's telemetry lines from its console port to the tag's port, byte for byte.
    python bridge.py <tool console port> <tag port> [--minutes 5] [--learn] [--pulls N --on 2 --off 4 --duty 2]
Lines that start with "$T" go to the tag; everything else the tool prints is shown here. The other way, "$TC" lines
from the tag (signed commands it checked, stage two) go to the tool unchanged, as on a real two-wire link. On the bench this stands in
for one jumper (tool TX to tag RX, plus ground); the tag cannot tell the difference. --pulls drives the motor firmware's
own commands as a trigger: 'g' (run) for --on seconds, 's' (stop) for --off seconds, N times, at --duty x 10 percent.
--learn sends 'l' first (the BLDC firmware refuses to run before its Hall table is learned; the shaft must be free).
Needs pyserial."""
import argparse
import sys
import time

import serial


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("tool")
    ap.add_argument("tag")
    ap.add_argument("--minutes", type=float, default=5)
    ap.add_argument("--learn", action="store_true")
    ap.add_argument("--pulls", type=int, default=0)
    ap.add_argument("--on", type=float, default=2)
    ap.add_argument("--off", type=float, default=4)
    ap.add_argument("--duty", type=int, default=2, help="duty in tens of percent (the firmware caps at 40 percent)")
    ap.add_argument("--log", help="append everything the tool printed, time-stamped")
    a = ap.parse_args()
    tool = serial.Serial(a.tool, 115200, timeout=0.05)
    tag = serial.Serial(a.tag, 115200, timeout=0)
    logf = open(a.log, "a") if a.log else None
    buf = b""
    t0 = time.time()
    plan = []                                   # (time, bytes to send to the tool)
    t = 1.0
    if a.learn:
        plan.append((t, b"l")); t += 12
    if a.pulls:
        plan.append((t, str(a.duty).encode())); t += 0.5
        for _ in range(a.pulls):
            plan.append((t, b"g")); t += a.on
            plan.append((t, b"s")); t += a.off
    forwarded = 0
    tbuf = b""
    while time.time() - t0 < a.minutes * 60 or plan:
        now = time.time() - t0
        while plan and plan[0][0] <= now:
            tool.write(plan.pop(0)[1])
        tbuf += tag.read(4096)
        while b"\n" in tbuf:
            raw, tbuf = tbuf.split(b"\n", 1)
            k = raw.find(b"$TC,")                   # anywhere in the line: the tag's own status text may share it
            if k >= 0:
                tool.write(raw[k:].strip() + b"\n")
                print("%7.2f <- tag %s" % (now, raw[k:].strip().decode("utf-8", "replace")), flush=True)
        buf += tool.read(4096)
        while b"\n" in buf:
            raw, buf = buf.split(b"\n", 1)
            line = raw + b"\n"
            text = raw.decode("utf-8", "replace").strip()
            if logf and text:
                logf.write("%.3f %s\n" % (time.time(), text))
                logf.flush()
            if raw.lstrip().startswith(b"$T"):
                tag.write(raw.lstrip().rstrip(b"\r") + b"\n")
                forwarded += 1
                print("%7.2f -> tag %s" % (now, text), flush=True)
            elif text:
                print("%7.2f    tool %s" % (now, text), flush=True)
        if not plan and a.pulls and time.time() - t0 > t + 3:
            break
    print("forwarded %d telemetry lines" % forwarded, file=sys.stderr)


if __name__ == "__main__":
    main()
