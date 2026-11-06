#!/usr/bin/env python3
"""Drive the CC1407P radio lab (radio_lab.c on the LP-MSPM0G5187) and its CC1354P10 partner (partner.c)
over their serial ports, and log every line with the Mac's time.

    lab.py <log file> <step> [<step> ...]

A step is "g:<command>" for the G5187, "p:<command>" for the CC1354P10, "p&:<command>" to send to the
CC1354P10 without waiting (so both radios can run at once), "g&:<command>" and "gdone:<seconds>" to
start a G5187 command and collect its output later, or "wait:<seconds>". Each g: step waits for
the G5187's "DONE" line. Ports default to this bench's probes; set LAB_G and LAB_P to use your own.
Uses only the standard library.
"""
import os, sys, termios, time, select

G = os.environ.get("LAB_G", "/dev/cu.usbmodemMG0200011")
P = os.environ.get("LAB_P", "/dev/cu.usbmodemLS45013A1")


def opn(path):
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    a = termios.tcgetattr(fd)
    a[0] = 0; a[1] = 0; a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL; a[3] = 0
    a[4] = a[5] = termios.B115200
    termios.tcsetattr(fd, termios.TCSANOW, a)
    return fd


class Port:
    def __init__(self, name, path, log):
        self.name, self.fd, self.log, self.buf = name, opn(path), log, b""

    def lines(self, timeout):
        out, end = [], time.time() + timeout
        while time.time() < end:
            r, _, _ = select.select([self.fd], [], [], 0.05)
            if r:
                try:
                    self.buf += os.read(self.fd, 4096)
                except BlockingIOError:
                    pass
            while b"\n" in self.buf:
                ln, self.buf = self.buf.split(b"\n", 1)
                t = ln.decode("ascii", "replace").strip()
                if t:
                    self.log.write(f"{time.time():.3f} {self.name} {t}\n"); self.log.flush()
                    out.append(t)
            if not r and not out and timeout < 0.2:
                break
        return out

    def send(self, cmd):
        self.lines(0.1)
        self.log.write(f"{time.time():.3f} {self.name} >> {cmd}\n")
        os.write(self.fd, (cmd + "\r\n").encode())

    def send_raw(self, cmd):
        """Write a line at once, without first reading what is waiting (for streaming)."""
        os.write(self.fd, (cmd + "\r\n").encode())

    def run(self, cmd, timeout=120):
        self.send(cmd)
        got, end = [], time.time() + timeout
        while time.time() < end:
            batch = self.lines(0.2)
            got += batch                      # keep every line of the batch, including any after DONE
            if any(t.startswith("DONE") for t in batch):
                return got
        got.append("TIMEOUT")
        return got


def main():
    log = open(sys.argv[1], "a")
    g, p = Port("G", G, log), Port("P", P, log)
    for step in sys.argv[2:]:
        if step.startswith("wait:"):
            end = time.time() + float(step[5:])
            while time.time() < end:
                g.lines(0.2); p.lines(0.05)
        elif step.startswith("g&:"):
            g.send(step[3:])
        elif step.startswith("gdone:"):
            got, end = [], time.time() + float(step[6:])
            while time.time() < end:
                ls = g.lines(0.2); got += ls; p.lines(0.05)
                if any(t.startswith("DONE") for t in ls):
                    break
            for t in got:
                print("G", t)
            for t in p.lines(0.3):
                print("P", t)
        elif step.startswith("p&:"):
            p.send(step[3:])
        elif step.startswith("p:"):
            for t in p.run(step[2:]):
                print("P", t)
        elif step.startswith("g:"):
            for t in g.run(step[2:]):
                print("G", t)
            for t in p.lines(0.3):
                print("P", t)


if __name__ == "__main__":
    main()
