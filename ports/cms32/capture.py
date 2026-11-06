#!/usr/bin/env python3
"""Capture a serial port for N seconds without pyserial: termios only (macOS resets a port's settings
when the last process closes it, so the settings and the reads must happen in one open).
   capture.py <tty> <baud> <seconds> <outfile>"""
import os, sys, termios, time, select

tty, baud, secs, out = sys.argv[1], int(sys.argv[2]), float(sys.argv[3]), sys.argv[4]
fd = os.open(tty, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
attr = termios.tcgetattr(fd)
attr[0] = termios.IGNBRK                        # iflag: raw
attr[1] = 0                                     # oflag
attr[2] = termios.CS8 | termios.CREAD | termios.CLOCAL   # cflag
attr[3] = 0                                     # lflag: raw, no echo
speed = getattr(termios, "B%d" % baud, None) or baud
attr[4] = attr[5] = speed
attr[6][termios.VMIN] = 0
attr[6][termios.VTIME] = 0
termios.tcsetattr(fd, termios.TCSANOW, attr)
termios.tcflush(fd, termios.TCIFLUSH)
end = time.time() + secs
with open(out, "wb") as f:
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.2)
        if r:
            try:
                d = os.read(fd, 4096)
            except BlockingIOError:
                d = b""
            if d:
                f.write(d); f.flush()
os.close(fd)
