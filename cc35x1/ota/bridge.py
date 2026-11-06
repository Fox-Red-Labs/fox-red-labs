#!/usr/bin/env python3
"""Serial-to-TCP bridge for the proof run: holds the port open at 115200 and relays bytes to one local TCP client.
   bridge.py <port> <tcp-port> ; run it under tools/frl/lock.py exec. Stands in for the browser's Web Serial port."""
import os, select, socket, sys, termios
port, tp = sys.argv[1], int(sys.argv[2])
fd = os.open(port, os.O_RDWR | os.O_NONBLOCK | os.O_NOCTTY)
a = termios.tcgetattr(fd); a[0] = a[1] = a[3] = 0; a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
a[4] = a[5] = termios.B115200; a[6][termios.VMIN] = 0; a[6][termios.VTIME] = 0
termios.tcsetattr(fd, termios.TCSANOW, a); termios.tcflush(fd, termios.TCIOFLUSH)
s = socket.socket(); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); s.bind(("127.0.0.1", tp)); s.listen(1)
while True:
    c, _ = s.accept()
    while True:
        r, _, _ = select.select([fd, c], [], [], 1)
        if fd in r:
            d = os.read(fd, 4096)
            if d: c.sendall(d)
        if c in r:
            d = c.recv(4096)
            if not d: break
            os.write(fd, d)
    c.close()
