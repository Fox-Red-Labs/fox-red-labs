#!/usr/bin/env python3
"""Open the LP-AM243 console, run one image with ram_run.js, write what it prints to a file.

ram_capture.py <image.out> <log> [--secs 60] [--until TEXT]
The port opens before the load and the first half second is thrown away: the probe can hand over
stale bytes from the previous program. Stops at TEXT or after --secs. Needs pyserial.
"""
import argparse, os, re, subprocess, sys, time
import serial

HERE = os.path.dirname(os.path.abspath(__file__))
RUN = "/Applications/ti/ccs2101/ccs/scripting/run.sh"

a = argparse.ArgumentParser()
a.add_argument("image")
a.add_argument("log")
a.add_argument("--secs", type=float, default=60)
a.add_argument("--until")
a.add_argument("--port", default="/dev/cu.usbmodemS24L00961")
a.add_argument("--baud", type=int, default=115200)
args = a.parse_args()

with serial.Serial(args.port, args.baud, timeout=0.2) as port:
    time.sleep(0.5)
    port.reset_input_buffer()
    load = subprocess.run([RUN, os.path.join(HERE, "ram_run.js"), os.path.abspath(args.image)],
                          capture_output=True, text=True, timeout=240)
    if "FRL_RUN" not in load.stdout:
        sys.exit("load failed:\n" + load.stdout[-800:] + load.stderr[-800:])
    started = time.time()
    data = b""
    first = None
    while time.time() - started < args.secs:
        chunk = port.read(4096)
        if chunk and first is None:
            first = time.time() - started
        data += chunk
        if args.until and args.until.encode() in data:
            time.sleep(1.0)
            data += port.read(65536)
            break

text = data.decode("latin-1").replace("\r", "")
text = re.sub(r"\(ssh://[^)]*\)", "(compiler build string removed)", text)  # the compiler's version string names its build host
with open(args.log, "w") as f:
    f.write(text)
print(f"CAPTURED {len(data)} bytes, first byte at {first} s after start, {time.time() - started:.1f} s total -> {args.log}")
