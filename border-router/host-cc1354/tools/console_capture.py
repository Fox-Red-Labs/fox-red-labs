#!/usr/bin/env python3
"""Drive an OpenThread CLI over a debugger UART and keep every byte it prints, with a timestamp per line.
Unlike tools/ot_cli.py (reads 1.5 s after each command) this never stops listening, so log lines and the last words
before a crash are kept.

Usage: console_capture.py <port> <log file> <cmd or wait:N> ...     (921600 baud, macOS IOSSIOSPEED)
Prints the same lines it logs. Exit code 0 even if the board goes quiet; read the log."""
import fcntl, os, select, struct, sys, termios, time

BAUD = int(os.environ.get("BAUD", "921600"))
IOSSIOSPEED = 0x80045402

port, logpath, cmds = sys.argv[1], sys.argv[2], sys.argv[3:]
fd = os.open(port, os.O_RDWR | os.O_NONBLOCK | os.O_NOCTTY)
a = termios.tcgetattr(fd); a[4] = a[5] = termios.B115200; a[2] |= termios.CREAD | termios.CLOCAL; a[3] = 0; a[1] = 0; a[0] = 0
termios.tcsetattr(fd, termios.TCSANOW, a)
fcntl.ioctl(fd, IOSSIOSPEED, struct.pack("L", BAUD))
termios.tcflush(fd, termios.TCIOFLUSH)
log = open(logpath, "a")
t0 = time.time()
partial = b""


def emit(kind, text):
    line = f"{time.time() - t0:8.3f} {kind} {text}"
    print(line, flush=True)
    log.write(line + "\n"); log.flush()


def pump(seconds):
    global partial
    end = time.time() + seconds
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.05)
        if not r:
            continue
        try:
            partial += os.read(fd, 4096)
        except BlockingIOError:
            continue
        while b"\n" in partial:
            line, partial = partial.split(b"\n", 1)
            text = line.replace(b"\r", b"").decode("latin-1").rstrip()
            if text:
                emit("<", text)


os.write(fd, b"\r\n"); pump(0.5)
for c in cmds:
    if c.startswith("wait:"):
        pump(float(c[5:])); continue
    emit(">", c)
    os.write(fd, c.encode() + b"\r\n")
    pump(1.0)
pump(1.0)
if partial.strip():
    emit("<", partial.decode("latin-1").strip())
