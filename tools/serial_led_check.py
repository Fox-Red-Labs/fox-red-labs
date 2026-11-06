#!/usr/bin/env python3
"""Prove a board running Zephyr's blinky is alive by reading its serial port.

  serial_led_check.py <probe-serial> [seconds]

Reads /dev/cu.usbmodem<probe-serial>1 at 115200 and prints each line with the time it arrived.
"LED state: ON" and "LED state: OFF" a second apart means blinky is running.
Silence means some other program (or none) is on the chip. Exit code 0 if any LED line was seen.
"""
import os, sys, termios, time, select
if len(sys.argv) < 2: sys.exit(__doc__)
port = f"/dev/cu.usbmodem{sys.argv[1]}1"; secs = float(sys.argv[2]) if len(sys.argv) > 2 else 5
fd = os.open(port, os.O_RDONLY | os.O_NONBLOCK | os.O_NOCTTY)
a = termios.tcgetattr(fd); a[4] = a[5] = termios.B115200; a[2] |= termios.CLOCAL | termios.CREAD; a[3] = a[0] = a[1] = 0
termios.tcsetattr(fd, termios.TCSANOW, a); termios.tcflush(fd, termios.TCIFLUSH)
t0 = time.time(); buf = b""; seen = 0
while time.time() - t0 < secs:
    if select.select([fd], [], [], 0.2)[0]:
        buf += os.read(fd, 512)
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1); text = line.strip().decode(errors="replace")
            seen += "LED state" in text; print(f"{time.time() - t0:5.2f}s {text}")
print(f"{port}: {seen} LED lines in {secs:.0f} s" if seen else f"{port}: silent for {secs:.0f} s")
sys.exit(0 if seen else 1)
