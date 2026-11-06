#!/usr/bin/env python3
"""Hold the console port open, send lines to the MicroPython REPL, print and save what comes back.
  python3 repl.py PORT LOGFILE [--reset] -- 'line1' 'line2' ...
Paste mode (Ctrl-E) is used so multi-line text is not auto-indented. Needs pyserial (python3 -m pip install pyserial)."""
import sys, time
try:
    import serial
except ImportError:
    sys.exit("error: pyserial missing (python3 -m pip install pyserial)")
port, log = sys.argv[1], sys.argv[2]
args = sys.argv[3:]
reset = '--reset' in args
lines = args[args.index('--') + 1:] if '--' in args else []
s = serial.Serial(port, 115200, timeout=0.2)
out = bytearray()
def drain(t):
    end = time.time() + t
    while time.time() < end:
        b = s.read(512)
        if b:
            out.extend(b); end = max(end, time.time() + 0.3)
if reset:
    s.dtr = False; s.rts = False
drain(1.0)
s.write(b'\x03\x03'); drain(0.8)
if reset:
    s.write(b'\x04'); drain(2.0)   # Ctrl-D soft reset prints the banner
for ln in lines:
    s.write(ln.encode() + b'\r'); drain(1.2)
open(log, 'wb').write(out)
sys.stdout.write(out.decode('utf-8', 'replace'))
