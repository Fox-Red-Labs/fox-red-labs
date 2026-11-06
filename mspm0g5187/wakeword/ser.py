#!/usr/bin/env python3
"""Print the LP-MSPM0G5187 serial output with Mac timestamps.   ser.py PORT BAUD SECONDS"""
import serial, sys, time
s = serial.Serial(sys.argv[1], int(sys.argv[2]), timeout=0.2)
t0 = time.time(); buf = b''
while time.time() - t0 < float(sys.argv[3]):
    buf += s.read(512)
    while b'\n' in buf:
        l, buf = buf.split(b'\n', 1)
        print(f"{time.time()-t0:8.2f} {l.decode('ascii','replace').strip()}", flush=True)
