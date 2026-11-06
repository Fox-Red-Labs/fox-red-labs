#!/usr/bin/env python3
"""Byte relay between stdin/stdout and the board's serial port, opened the way link.py opens it (termios plus the
IOSSIOSPEED ioctl for the 9.2 Mbaud rate). node serialport did not apply that rate on this Mac on 2026-10-18 (zero bytes
back), so phone_console.mjs spawns this instead. Usage: serial_relay.py /dev/cu.usbmodemAL1300011 9200000"""
import fcntl, os, select, struct, sys, termios, time


def main():
    path, baud = sys.argv[1], int(sys.argv[2])
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    fcntl.ioctl(fd, getattr(termios, 'TIOCEXCL', 0x2000740d))   # exclusive: a browser or second tool gets 'busy' at once instead of sharing the byte stream
    a = termios.tcgetattr(fd)
    a[0] = a[1] = a[3] = 0; a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL; a[4] = a[5] = termios.B115200
    termios.tcsetattr(fd, termios.TCSANOW, a)
    fcntl.ioctl(fd, 0x80085402, struct.pack("L", baud))
    time.sleep(0.2); termios.tcflush(fd, termios.TCIOFLUSH)
    sys.stderr.write("relay open\n"); sys.stderr.flush()
    inp, out = 0, 1
    os.set_blocking(inp, False)
    pending = b""
    while True:
        r, w, _ = select.select([fd, inp], [fd] if pending else [], [], 0.05)
        if fd in r:
            try:
                b = os.read(fd, 1 << 16)
                if b:
                    os.write(out, b)
            except BlockingIOError:
                pass
        if inp in r:
            try:
                b = os.read(inp, 1 << 16)
            except BlockingIOError:
                b = None
            if b == b"":          # the bridge closed our stdin: done
                break
            if b:
                pending += b
        if pending and fd in w:
            try:
                n = os.write(fd, pending); pending = pending[n:]   # never drop a byte: the board counts 16-byte packets
            except BlockingIOError:
                pass
    os.close(fd)


if __name__ == "__main__":
    main()
