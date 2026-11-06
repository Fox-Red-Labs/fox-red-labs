#!/usr/bin/env python3
"""Capture and read the serial screen of TI's Z-Stack examples (the CUI menu, 115200 baud) from a Mac.

  cui_log.py capture <port> <file> <seconds>    log every read as '<unix time> <bytes repr>'
  cui_log.py decode <file> [<t0 unix time>]     print '<seconds since t0> <status line> <text>', one row per change

The CUI redraws its status lines many times a second; decode keeps a line only when its text changes.
t0 defaults to the first timestamp in the file."""
import ast, os, re, select, sys, termios, time

def capture(port, path, secs):
    fd = os.open(port, os.O_RDWR | os.O_NONBLOCK | os.O_NOCTTY)
    a = termios.tcgetattr(fd); a[4] = a[5] = termios.B115200; a[2] |= termios.CREAD | termios.CLOCAL; a[3] = a[1] = a[0] = 0
    termios.tcsetattr(fd, termios.TCSANOW, a); termios.tcflush(fd, termios.TCIOFLUSH)   # drop bytes left from the previous program
    end = time.time() + secs
    with open(path, "a") as f:
        while time.time() < end:
            if select.select([fd], [], [], 0.5)[0]:
                b = os.read(fd, 4096)
                if b: f.write("%.3f %r\n" % (time.time(), b)); f.flush()

def decode(path, t0=None):
    buf, last = b"", {}
    for line in open(path):
        ts, rep = line.split(" ", 1); ts = float(ts)
        if t0 is None: t0 = ts
        if ts < t0 - 1: continue
        buf += ast.literal_eval(rep)
        while True:   # each status line is sent as STX <label> ESC 7 <n> ESC 8 ": " <text> ETX
            a = buf.find(b"\x02"); b = buf.find(b"\x03", a + 1)
            if a < 0 or b < 0: break
            seg, buf = buf[a + 1:b], buf[b + 1:]
            m = re.match(rb"(.*?)\x1b7(\d+)\x1b8: (.*)", seg, re.S)
            if not m: continue
            key = m.group(1).strip().decode("latin-1") + m.group(2).decode()
            txt = re.sub(r"\s+", " ", re.sub(r"\x1b\[[0-9;?]*[A-Za-z]", "", m.group(3).decode("latin-1"))).strip()
            if last.get(key) != txt:
                last[key] = txt
                print("%8.2f %-16s %s" % (ts - t0, key, txt))

if __name__ == "__main__":
    if sys.argv[1] == "capture": capture(sys.argv[2], sys.argv[3], float(sys.argv[4]))
    elif sys.argv[1] == "decode": decode(sys.argv[2], float(sys.argv[3]) if len(sys.argv) > 3 else None)
    else: sys.exit(__doc__)
