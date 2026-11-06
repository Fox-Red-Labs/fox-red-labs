#!/usr/bin/env python3
"""Talk to an OpenThread CLI image over the board's backchannel UART: send commands, print replies.
Usage: ot_cli.py <port> "<cmd>" ["<cmd>" ...]     e.g. ot_cli.py /dev/cu.usbmodemLS45013A1 state "dataset init new"
A command of the form "wait:N" sleeps N seconds (for example while a network forms).
TI's ot-ti CLI runs at 921600 baud (ot-ti src/uart.c), not 115200; set BAUD=... to override. macOS termios stops at
230400, so the speed is set with the IOSSIOSPEED ioctl."""
import fcntl, os, select, struct, sys, termios, time
BAUD = int(os.environ.get("BAUD", "921600"))
IOSSIOSPEED = 0x80045402   # _IOW('T', 2, speed_t), macOS
fd = os.open(sys.argv[1], os.O_RDWR | os.O_NONBLOCK | os.O_NOCTTY)
a = termios.tcgetattr(fd); a[4] = a[5] = termios.B115200; a[2] |= termios.CREAD | termios.CLOCAL; a[3] = 0; a[1] = 0; a[0] = 0
termios.tcsetattr(fd, termios.TCSANOW, a)
fcntl.ioctl(fd, IOSSIOSPEED, struct.pack("L", BAUD))
termios.tcflush(fd, termios.TCIOFLUSH)
def read(t):
    b, end = b"", time.time() + t
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.1)
        if r: b += os.read(fd, 4096)
    return b.decode("latin-1").replace("\r", "")
os.write(fd, b"\r\n"); read(0.5)
for c in sys.argv[2:]:
    if c.startswith("wait:"):
        time.sleep(float(c[5:])); continue
    os.write(fd, c.encode() + b"\r\n")
    out = read(1.5).strip().replace(c, "", 1).strip()
    print(f"> {c}\n{out}")
