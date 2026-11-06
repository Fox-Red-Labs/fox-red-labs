#!/usr/bin/env python3
"""Listen on the AM13E230 backchannel and stamp each line with the Mac's clock.
   uart_listen.py <seconds> [baud] [port]   (default port: /dev/cu.usbmodemAL1300011, this board's probe only)
Prints "<seconds since start> <line>". Start it before flashing so the program's first line is not missed."""
import sys, time, os, termios, tty, select
secs = float(sys.argv[1]); baud = int(sys.argv[2]) if len(sys.argv) > 2 else 115200
port = sys.argv[3] if len(sys.argv) > 3 else "/dev/cu.usbmodemAL1300011"
fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
a = termios.tcgetattr(fd); sp = getattr(termios, "B%d" % baud)
tty.setraw(fd); a = termios.tcgetattr(fd); a[4] = a[5] = sp
# PARITY=odd or PARITY=even in the environment: listen 8O1 or 8E1 instead of 8N1 (the STM32 UART_Printf example sends odd parity)
par = os.environ.get("PARITY")
if par: a[2] |= termios.PARENB | (termios.PARODD if par == "odd" else 0)
termios.tcsetattr(fd, termios.TCSANOW, a)
t0 = time.time(); buf = b""
print("start", time.strftime("%H:%M:%S"), flush=True)
while time.time() - t0 < secs:
    if select.select([fd], [], [], 0.05)[0]: buf += os.read(fd, 256)
    while b"\n" in buf:
        line, buf = buf.split(b"\n", 1)
        print("%.2f %s" % (time.time() - t0, line.decode("latin1").rstrip()), flush=True)
# RAW=1 in the environment: also print what is left in the buffer at the end (output with no newline)
if os.environ.get("RAW") and buf: print("%.2f raw %r" % (time.time() - t0, buf), flush=True)
