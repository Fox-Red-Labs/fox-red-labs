#!/usr/bin/env python3
"""Send keystrokes to the AM13E230 backchannel and print what comes back.
   uart_cmd.py "<keys>" [seconds] [port] [--stop] [--gap seconds]
   Keys are sent 0.3 s apart unless --gap is given. --stop sends coast when the timer ends.
"""
import sys, os, time, termios, tty, select
keys = sys.argv[1]; secs = float(sys.argv[2]) if len(sys.argv) > 2 else 3.0
port = sys.argv[3] if len(sys.argv) > 3 else "/dev/cu.usbmodemAL1300011"
stop_at_end = "--stop" in sys.argv[3:]
if port == "--stop": port = "/dev/cu.usbmodemAL1300011"
if port == "--gap": port = "/dev/cu.usbmodemAL1300011"
gap = float(sys.argv[sys.argv.index("--gap") + 1]) if "--gap" in sys.argv else 0.3
fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
tty.setraw(fd); a = termios.tcgetattr(fd); a[4] = a[5] = termios.B115200; termios.tcsetattr(fd, termios.TCSANOW, a)
buf = b""; t0 = time.time(); i = 0; nxt = t0
try:
    while time.time() - t0 < secs:
        if i < len(keys) and time.time() >= nxt: os.write(fd, keys[i].encode()); i += 1; nxt = time.time() + gap
        if select.select([fd], [], [], 0.05)[0]:
            try: buf += os.read(fd, 1024)
            except BlockingIOError: pass
finally:
    if stop_at_end:
        os.write(fd, b"s")
        time.sleep(0.15)
        if select.select([fd], [], [], 0.05)[0]:
            try: buf += os.read(fd, 1024)
            except BlockingIOError: pass
    os.close(fd)
sys.stdout.write(buf.decode("latin1").replace("\r", "")); sys.stdout.flush()
