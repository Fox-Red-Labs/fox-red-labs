#!/usr/bin/env python3
"""Log the sender's and receiver's serial ports with timestamps for a fixed time.
usage: log_both.py <seconds> <folder> [sender port] [receiver port]   (writes tx.log and rx.log)"""
import os, sys, termios, time, select, threading
secs, out = float(sys.argv[1]), sys.argv[2]
ports = (sys.argv[3] if len(sys.argv) > 3 else "/dev/cu.usbmodemMG0200011", sys.argv[4] if len(sys.argv) > 4 else "/dev/cu.usbmodemLS45013A1")
os.makedirs(out, exist_ok=True)
def grab(port, path):
    fd = os.open(port, os.O_RDONLY | os.O_NONBLOCK | os.O_NOCTTY)
    a = termios.tcgetattr(fd); a[4] = a[5] = termios.B115200; a[2] |= termios.CREAD | termios.CLOCAL; a[3] = 0; a[0] = 0; a[1] = 0
    termios.tcsetattr(fd, termios.TCSANOW, a); t0 = time.time(); part = b""
    with open(path, "w") as f:
        while time.time() - t0 < secs:
            r, _, _ = select.select([fd], [], [], 0.5)
            if r:
                try: part += os.read(fd, 4096)
                except BlockingIOError: pass
                while b"\n" in part:
                    l, part = part.split(b"\n", 1); f.write(f"{time.time():.2f} {l.decode('utf8', 'replace').strip()}\n"); f.flush()
    os.close(fd)
ts = [threading.Thread(target=grab, args=(ports[0], out + "/tx.log")), threading.Thread(target=grab, args=(ports[1], out + "/rx.log"))]
[t.start() for t in ts]; [t.join() for t in ts]
