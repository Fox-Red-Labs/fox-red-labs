#!/usr/bin/env python3
"""inject.py LINE [LINE ...]: write lines (for example 'SMOKE 1') to the CC35X1 console UART, 115200 8N1, one per 1 s."""
import sys, time, serial
s = serial.Serial('/dev/cu.usbmodemE10000FQ1', 115200, timeout=0.1)
for l in sys.argv[1:]:
    s.write((l + '\n').encode()); s.flush(); print(time.time(), l); time.sleep(1)
