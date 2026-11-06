#!/usr/bin/env python3
"""Reset the LP-MSPM0G3519 under the probe lock and print the backchannel UART for N seconds: uart_reset.py [secs]."""
import sys, time
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "touch-games" / "tools")); sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "tools" / "frl"))
import g3519_lock_run as g
from lock import ProbeLock
import serial
secs = float(sys.argv[1]) if len(sys.argv) > 1 else 10
with ProbeLock(g.SERIAL, "chip8", "uart log") as lk:
    s = serial.Serial(f"/dev/cu.usbmodem{g.SERIAL}1", 115200, timeout=0.2)
    g.ocd(["reset run"]); t = time.time(); buf = b""
    while time.time() - t < secs: buf += s.read(512)
    print(buf.decode(errors="replace"))
