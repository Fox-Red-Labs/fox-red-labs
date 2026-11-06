#!/usr/bin/env python3
"""Send U-Boot commands on the AM62L EVM's UART A and print the replies.
Usage: ub.py CMD [CMD ...]   (env TMO = seconds to wait for each '=> ' prompt, default 30)"""
import os, sys, termios, fcntl, struct, select, time
fd = os.open('/dev/cu.usbserial-072518103640', os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
a = termios.tcgetattr(fd); a[0] = 0; a[1] = 0; a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL; a[3] = 0
termios.tcsetattr(fd, termios.TCSANOW, a); fcntl.ioctl(fd, 0x80045402, struct.pack('i', 115200))
tmo = float(os.environ.get('TMO', '30'))
for cmd in sys.argv[1:]:
    os.write(fd, (cmd + '\r').encode()); b = b''; t = time.time()
    while time.time() - t < tmo:
        if select.select([fd], [], [], 0.2)[0]:
            try: b += os.read(fd, 4096)
            except BlockingIOError: pass
            if b.endswith(b'=> '): break
    sys.stdout.write(b.decode('latin1').replace('\r', '')); sys.stdout.flush()
