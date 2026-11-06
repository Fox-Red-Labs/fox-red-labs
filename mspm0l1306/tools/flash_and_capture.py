#!/usr/bin/env python3
"""Flash an image to the LP-MSPM0L1306 (probe ML130001) with OpenOCD, then record the backchannel UART
until a line containing the end marker or the timeout. Writes everything to <out.txt>.
  flash_and_capture.py <image .hex/.out> <out.txt> [end marker, default 'BENCH DONE'] [timeout s, default 180]"""
import os, select, subprocess, sys, termios, time
img, outp = sys.argv[1], sys.argv[2]
marker = (sys.argv[3] if len(sys.argv) > 3 else "BENCH DONE").encode(); limit = float(sys.argv[4]) if len(sys.argv) > 4 else 180
here = os.path.dirname(os.path.abspath(__file__))
fl = subprocess.run([os.path.join(here, "ocd.sh"), "reset halt", f"program {img} verify"], capture_output=True, text=True).stdout
if "Verified OK" not in fl:
    sys.exit("flash failed:\n" + fl[-800:])
fd = os.open("/dev/cu.usbmodemML1300011", os.O_RDONLY | os.O_NONBLOCK | os.O_NOCTTY)
a = termios.tcgetattr(fd); a[4] = a[5] = termios.B115200; a[2] |= termios.CLOCAL | termios.CREAD; a[3] = a[0] = a[1] = 0
termios.tcsetattr(fd, termios.TCSANOW, a); termios.tcflush(fd, termios.TCIFLUSH)
subprocess.run([os.path.join(here, "ocd.sh"), "reset run"], capture_output=True)
t0 = time.time(); buf = b""
while time.time() - t0 < limit and marker not in buf:
    if select.select([fd], [], [], 0.5)[0]:
        buf += os.read(fd, 4096)
os.close(fd)
open(outp, "wb").write(buf)
print(f"{len(buf)} bytes in {time.time() - t0:.1f} s, end marker {'seen' if marker in buf else 'NOT seen'}")
