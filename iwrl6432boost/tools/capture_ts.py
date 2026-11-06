#!/usr/bin/env python3
"""Read a serial port for N seconds (termios, no pyserial needed) and save the bytes plus a per-chunk arrival log.
   capture_ts.py <port> <seconds> <out.bin> [baud]   Read only: nothing is written to the port.
   Writes <out.bin> and <out.bin>.ts (one line per chunk: seconds since start, bytes)."""
import os, sys, termios, time
port, secs, out = sys.argv[1], float(sys.argv[2]), sys.argv[3]
baud = {115200: termios.B115200, 230400: termios.B230400}[int(sys.argv[4]) if len(sys.argv) > 4 else 115200]
fd = os.open(port, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
a = termios.tcgetattr(fd)
a[0] = 0; a[1] = 0; a[3] = 0
a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
a[4] = a[5] = baud
a[6][termios.VMIN] = 0; a[6][termios.VTIME] = 1
termios.tcsetattr(fd, termios.TCSANOW, a)
t0 = time.time(); buf = b""; log = []
while time.time() - t0 < secs:
    try: d = os.read(fd, 4096)
    except BlockingIOError: d = b""
    if d:
        buf += d; log.append(f"{time.time()-t0:.4f} {len(d)}")
    else: time.sleep(0.005)
open(out, "wb").write(buf); open(out + ".ts", "w").write("\n".join(log) + "\n")
print(len(buf), "bytes")
