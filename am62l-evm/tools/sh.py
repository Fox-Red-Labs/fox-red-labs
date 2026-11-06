#!/usr/bin/env python3
"""Run shell commands on the AM62L EVM's Linux console (UART A) and print the replies.
Logs in as root if a login prompt shows. Usage: sh.py CMD [CMD ...]   (env TMO, default 30 s per command)"""
import os, sys, termios, fcntl, struct, select, time
fd = os.open('/dev/cu.usbserial-072518103640', os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
a = termios.tcgetattr(fd); a[0] = 0; a[1] = 0; a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL; a[3] = 0
termios.tcsetattr(fd, termios.TCSANOW, a); fcntl.ioctl(fd, 0x80045402, struct.pack('i', 115200))
tmo = float(os.environ.get('TMO', '30'))
MARK = '__FRL_DONE__'
def rd(until, t):
    b = b''; t0 = time.time()
    while time.time() - t0 < t:
        if select.select([fd], [], [], 0.2)[0]:
            try: b += os.read(fd, 4096)
            except BlockingIOError: pass
            if any(u in b for u in until): break
    return b
os.write(fd, b'\r'); b = rd([b'login:', b'# '], 8)
if b'login:' in b:
    os.write(fd, b'root\r'); rd([b'# '], 15)
os.write(fd, b'stty -echo; export PS1="# "\r'); rd([b'# '], 5)
for c in sys.argv[1:]:
    os.write(fd, (c + f'; echo {MARK}\r').encode())
    out = rd([MARK.encode() + b'\r\n', MARK.encode() + b'\n'], tmo).decode('latin1').replace('\r', '')
    sys.stdout.write(out.replace(MARK, '').rstrip('\n# ') + '\n'); sys.stdout.flush()
