#!/usr/bin/env python3
"""Run a Python file on the board through MicroPython's raw REPL (no file system needed).

  ~/.venvs/labgrid/bin/python ports/micropython/tools/paste_raw.py FILE [FILE ...] [--port /dev/cu.usbmodemMG0000011]
Each file runs in order in one session, so a later file can use names a former one defined.
Needs pyserial. Prints what the board printed. Use --leave to skip the final soft reset.
"""
import argparse, sys, time
import serial

ap = argparse.ArgumentParser()
ap.add_argument("files", nargs="+")
ap.add_argument("--port", default="/dev/cu.usbmodemMG0000011")
ap.add_argument("--timeout", type=float, default=20)
a = ap.parse_args()

s = serial.Serial(a.port, 115200, timeout=0.2)

def read_until(tok, secs):
    buf = b""
    end = time.time() + secs
    while time.time() < end and tok not in buf:
        buf += s.read(4096)
    return buf

s.write(b"\x03\x03")
time.sleep(0.3)
s.reset_input_buffer()
s.write(b"\x01")
r = read_until(b"raw REPL; CTRL-B to exit\r\n>", 3)
if b"raw REPL" not in r:
    sys.exit("no raw REPL: " + repr(r))
for f in a.files:
    src = open(f, "rb").read()
    for i in range(0, len(src), 128):    # small chunks: the port's UART ring is short
        s.write(src[i:i + 128])
        time.sleep(0.012)
    s.write(b"\x04")
    r = read_until(b"\x04>", a.timeout)
    ok = r.startswith(b"OK")
    body = r[2:] if ok else r
    out, _, err = body.partition(b"\x04")
    sys.stdout.write(out.decode("utf-8", "replace"))
    if err.strip(b"\x04>").strip():
        sys.stdout.write("ERROR: " + err.strip(b"\x04>").decode("utf-8", "replace") + "\n")
        break
s.write(b"\x02")   # back to the normal REPL
