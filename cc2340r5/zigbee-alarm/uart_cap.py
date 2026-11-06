#!/usr/bin/env python3
"""Capture the radio board's UART with receive times.  uart_cap.py PORT SECONDS [send-lines...]"""
import os, sys, termios, time, select
port, secs = sys.argv[1], float(sys.argv[2])
fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
a = termios.tcgetattr(fd); a[0] = 0; a[1] = 0; a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL; a[3] = 0
a[4] = a[5] = termios.B115200; termios.tcsetattr(fd, termios.TCSANOW, a); termios.tcflush(fd, termios.TCIOFLUSH)
t0 = time.time(); buf = b""
while time.time() - t0 < secs:
    r, _, _ = select.select([fd], [], [], 0.05)
    if r:
        buf += os.read(fd, 256)
        while b"\n" in buf:
            l, buf = buf.split(b"\n", 1); print(f"{time.time()-t0:7.2f} {l.decode(errors='replace').strip()}", flush=True)
