#!/usr/bin/env python3
"""Three ways: record the board's backchannel UART and check the app's output. Touches no probe.
  serial_check.py <port> <seconds> <out.txt>
  e.g. serial_check.py /dev/cu.usbmodemMG3500011 12 logs/run-zephyr-g3507.txt
Checks: lines match 't=<n> adc=<raw> avg=<avg>', t counts up by 1, raw is 0 to 4095, avg is the
4-sample moving average of the raw values (checked once 4 lines are seen, or from t=0), and lines
arrive about once a second. Writes every byte received, then the arrival time of each line."""
import os, re, select, sys, termios, time

port, secs, outp = sys.argv[1], float(sys.argv[2]), sys.argv[3]
fd = os.open(port, os.O_RDONLY | os.O_NONBLOCK | os.O_NOCTTY)
a = termios.tcgetattr(fd); a[4] = a[5] = termios.B115200; a[2] |= termios.CLOCAL | termios.CREAD; a[3] = a[0] = a[1] = 0
termios.tcsetattr(fd, termios.TCSANOW, a); termios.tcflush(fd, termios.TCIFLUSH)
t0 = time.time(); buf = b""; stamps = []; seen = 0
while time.time() - t0 < secs:
    if select.select([fd], [], [], 0.2)[0]:
        buf += os.read(fd, 4096)
        while buf.count(b"\n") > seen:
            seen += 1; stamps.append(round(time.time() - t0, 3))
os.close(fd)
lines = buf.decode(errors="replace").splitlines()
pat = re.compile(r"^t=(\d+) adc=(\d+) avg=(\d+)$")
rows = [tuple(map(int, m.groups())) for m in (pat.match(l.strip()) for l in lines) if m]
fails = []
if len(rows) < 5: fails.append(f"only {len(rows)} app lines")
for (t1, _, _), (t2, _, _) in zip(rows, rows[1:]):
    if t2 != t1 + 1: fails.append(f"t jumps {t1} to {t2}")
for t, raw, avg in rows:
    if raw > 4095: fails.append(f"raw {raw} above 12 bits at t={t}")
for i, (t, raw, avg) in enumerate(rows):
    if rows[0][0] == 0 or i >= 3:
        win = [r for _, r, _ in rows[max(0, i - 3):i + 1]]
        if avg != sum(win) // len(win): fails.append(f"avg {avg} at t={t}, expected {sum(win) // len(win)}")
app_stamps = [s for s, l in zip(stamps, lines) if pat.match(l.strip())]
gaps = [b - a for a, b in zip(app_stamps, app_stamps[1:])]
if gaps and not all(0.9 <= g <= 1.2 for g in gaps): fails.append(f"line gaps {min(gaps):.3f} to {max(gaps):.3f} s")
with open(outp, "w") as f:
    f.write(buf.decode(errors="replace"))
    f.write("\n# arrival times, s: " + " ".join(map(str, stamps)) + "\n")
print(f"{len(rows)} app lines, other lines: {[l for l in lines if l.strip() and not pat.match(l.strip())]}")
if gaps: print(f"line gaps {min(gaps):.3f} to {max(gaps):.3f} s")
print("PASS" if not fails else "FAIL: " + "; ".join(fails[:6]))
sys.exit(1 if fails else 0)
