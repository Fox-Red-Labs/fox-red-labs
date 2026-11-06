#!/usr/bin/env python3
"""Capture one serial port to a log with host timestamps. Holds the port open itself,
because on macOS a baud set by `stty -f` reverts when stty closes the device, so a
later `cat` reads at the default 9600 and prints junk for a 115200 line.
Usage: capture.py PORT BAUD LOGFILE [RAWFILE]   (stop with SIGTERM or Ctrl-C)"""
import os, sys, termios, fcntl, time, struct, select, signal
IOSSIOSPEED = 0x80045402
port, baud, log = sys.argv[1], int(sys.argv[2]), sys.argv[3]
raw = sys.argv[4] if len(sys.argv) > 4 else None
fd = os.open(port, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
a = termios.tcgetattr(fd)
a[0] = 0; a[1] = 0; a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL; a[3] = 0
termios.tcsetattr(fd, termios.TCSANOW, a)
fcntl.ioctl(fd, IOSSIOSPEED, struct.pack('i', baud))
termios.tcflush(fd, termios.TCIFLUSH)
run = True
signal.signal(signal.SIGTERM, lambda *_: globals().__setitem__('run', False))
t0 = time.time(); line = b''
with open(log, 'ab', buffering=0) as lf, (open(raw, 'ab', buffering=0) if raw else open(os.devnull, 'ab')) as rf:
    lf.write(f"# {port} {baud} 8N1, host time, t0={time.strftime('%Y-%m-%dT%H:%M:%S')}\n".encode())
    while run:
        rl, _, _ = select.select([fd], [], [], 0.2)
        if not rl:
            continue
        try:
            d = os.read(fd, 4096)
        except BlockingIOError:
            continue
        rf.write(d)
        for b in d:
            line += bytes([b])
            if b == 10:
                lf.write(f"{time.time()-t0:9.3f} ".encode() + line.replace(b'\r', b''))
                line = b''
    if line:
        lf.write(f"{time.time()-t0:9.3f} ".encode() + line + b'\n')
