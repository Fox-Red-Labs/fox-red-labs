#!/usr/bin/env python3
"""A pretend equipment MCU: trigger pulls, motor runs, the odd overcurrent trip, a draining battery. It writes the
tool-telemetry UART lines (PROTOCOL.md section 1) to a serial port, the way the real tool MCU does to the tag.
    python toolsim.py <port|-> [--id UNIT-07] [--minutes 5] [--speed 1] [--seed 1] [--jam-every 6]
'-' prints the lines instead. --speed 10 runs ten times faster than real time (the totals grow faster, the line rate
stays readable). Every line is also echoed to stdout with a timestamp, so a run can be compared with what the tag and
the gateway reported."""
import argparse
import os
import random
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "proto"))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "ai"))
import tt_proto as P  # noqa: E402
import physics  # noqa: E402


class Tool:
    def __init__(self, ident, seed, jam_every, profile="mixed"):
        self.r = random.Random(seed)
        self.id = ident
        self.trig = self.oc = self.flt = 0
        self.run = 0.0
        self.rpm = self.ipk = 0
        self.vb = 20500
        self.tc = 24
        self.st = 0
        self.soc, self.cyc = 96, 211
        self.jam_every = jam_every            # kept for the command line; the jam rate now comes from the profile
        self.profile = profile
        self.temp = 24.0
        self.truth = []                        # (trigger number, true class, features) for scoring the tag's model
        self.pull_f, self.pull_cls = None, 0
        self.pull_left = 0.0
        self.pull_len = 0.0
        self.idle_left = 2.0
        self.n = 0

    def line(self, typ, **kv):
        s = P.make_line(typ, self.n, **kv)
        self.n = (self.n + 1) & 0xFFFF
        return s

    def status(self):
        return self.line("TS", id=self.id, trig=self.trig, oc=self.oc, flt=self.flt, run=int(self.run), rpm=self.rpm,
                         ipk=self.ipk, vb=self.vb, tc=int(round(self.tc)), st=self.st)

    def battery(self):
        return self.line("TB", soc=self.soc, cyc=self.cyc, vb=self.vb, tc=int(round(self.tc - 2)))

    def pull_line(self):
        f = self.pull_f
        return self.line("TP", dur=f["dur_ms"], ravg=f["rpm_avg"], rmax=f["rpm_max"], iavg=f["i_avg_ma"],
                         ipk=f["i_peak_ma"], vmin=f["v_min_mv"], tc=f["temp_c"])

    def command(self, cmd, duty=0):
        """A TC line from the tag (stage two): stop ends the pull now, lock stops it and refuses the trigger until
        unlock, run starts a pull at once. The tool reports the result as st in its next TS line."""
        if cmd == "lock":
            self.st, self.rpm = 3, 0
        elif cmd == "unlock" and self.st == 3:
            self.st, self.idle_left = 0, 3.0
        elif cmd == "stop" and self.st == 1:
            self.pull_left = 0
        elif cmd == "run" and self.st == 0:
            self.idle_left = 0

    def step(self, dt):
        """Advance dt seconds of tool time; returns the event lines that happened. Each pull is one of the four load
        classes in ai/physics.py; when it ends the tool sends TE trig, TE oc (a jam) and the TP summary."""
        out = []
        if self.st == 1:
            self.pull_left -= dt
            self.run += dt
            f = self.pull_f
            self.rpm = int(f["rpm_avg"] + self.r.uniform(-80, 80))
            self.ipk = max(self.ipk, f["i_peak_ma"] if self.pull_left <= 0 else f["i_avg_ma"])
            self.tc = f["temp_c"]
            self.vb = max(16000, int(self.vb - 3 * dt))
            if self.pull_left <= 0:
                if self.pull_cls == 3:
                    self.oc += 1
                    out.append(self.line("TE", ev="oc", v=f["i_peak_ma"]))
                self.st, self.rpm = 0, 0
                out.append(self.line("TE", ev="trig", v=min(65535, f["dur_ms"])))
                out.append(self.pull_line())
                self.truth.append((self.trig, self.pull_cls, dict(f)))
                self.idle_left = self.r.uniform(1.5, 6)
        elif self.st == 3:                               # locked: the trigger does nothing
            self.temp = physics.cool(self.temp, dt)
            self.tc = int(round(self.temp))
        else:
            self.temp = physics.cool(self.temp, dt)
            self.tc = int(round(self.temp))
            self.idle_left -= dt
            if self.idle_left <= 0:
                self.trig += 1
                self.st = 1
                self.pull_cls = physics.task_mix(self.r, self.profile)
                self.pull_f, self.temp = physics.pull(self.r, self.pull_cls, int(self.vb), self.temp)
                self.pull_len = self.pull_left = self.pull_f["dur_ms"] / 1000
                self.ipk = 0
        if self.r.random() < dt / 120:
            self.soc = max(5, self.soc - 1)
        return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("port")
    ap.add_argument("--id", default="UNIT-07")
    ap.add_argument("--minutes", type=float, default=5)
    ap.add_argument("--speed", type=float, default=1)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--jam-every", type=float, default=6, help="mean trigger pulls between overcurrent trips, 0 = never")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--profile", default="mixed", choices=("mixed", "finish", "framing"))
    a = ap.parse_args()
    out = None
    if a.port != "-":
        import serial
        out = serial.Serial(a.port, a.baud, timeout=0)
    tool = Tool(a.id, a.seed, a.jam_every, a.profile)
    t0 = time.time()
    last_ts = last_tb = -99.0
    tick = 0.1
    sent = 0

    rx = ""

    def listen():
        """Lines from the tag: its status, and TC commands it passed on after checking their signature."""
        nonlocal rx
        if not out or not out.in_waiting:
            return
        rx += out.read(out.in_waiting).decode("utf-8", "replace")
        while "\n" in rx:
            ln, rx = rx.split("\n", 1)
            ln = ln.strip()
            if ln:
                sys.stdout.write("  <tag> " + ln + "\n")
            if "$TC," in ln and "*" in ln:
                body, cs = ln[ln.index("$TC,") + 1:].rsplit("*", 1)
                x = 0
                for ch in body:
                    x ^= ord(ch)
                if "%02X" % x != cs[:2].upper():
                    continue
                kv = dict(p.split("=", 1) for p in body.split(",")[1:] if "=" in p)
                tool.command(kv.get("cmd", ""), int(kv.get("duty", 0)))
                sys.stdout.write("%.2f command %s: state now %s\n" % (time.time() - t0, kv.get("cmd"), ("idle", "running", "fault", "locked")[tool.st]))
                emit(tool.status())

    def emit(s):
        nonlocal sent
        sent += 1
        sys.stdout.write("%.2f %s" % (time.time() - t0, s))
        sys.stdout.flush()
        if out:
            out.write(s.encode())
            out.flush()

    emit(tool.line("TE", ev="boot", v=1))
    while time.time() - t0 < a.minutes * 60:
        was = tool.st
        for ev in tool.step(tick * a.speed):
            emit(ev)
        now = time.time() - t0
        period = 1.0 if tool.st == 1 else 10.0
        if tool.st != was or now - last_ts >= period:
            emit(tool.status())
            last_ts = now
            if tool.st == 0:
                tool.ipk = 0
        if now - last_tb >= 10:
            emit(tool.battery())
            last_tb = now
        listen()
        time.sleep(tick)
    print("sent %d lines; totals trig=%d oc=%d run=%d" % (sent, tool.trig, tool.oc, int(tool.run)), file=sys.stderr)


if __name__ == "__main__":
    main()
