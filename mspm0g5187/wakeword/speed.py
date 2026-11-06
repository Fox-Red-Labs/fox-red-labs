#!/usr/bin/env python3
"""How many 20 ms frames a second can the replay build process? Sends noise faster than real time and reads
the board's inference counter against the Mac clock.   speed.py SECONDS [FACTOR]  (default factor 2 = 16,000 samples a second;
the serial link at 230,400 baud carries at most 11,520 samples a second, so use FACTOR 1.4 or less)"""
import re, sys, time, numpy as np, serial
secs = float(sys.argv[1]); f = float(sys.argv[2]) if len(sys.argv) > 2 else 1.4
s = serial.Serial("/dev/cu.usbmodemMG0200011", 230400, timeout=0)
rng = np.random.default_rng(3); buf = b""; marks = []
t0 = time.time(); sent = 0
while time.time() - t0 < secs:
    x = rng.normal(0, 30, 160).astype("<i2"); due = t0 + sent / (8000.0 * f)
    while time.time() < due: time.sleep(0.0005)
    s.write(x.tobytes()); sent += 160
    buf += s.read(4096)
    while b"\n" in buf:
        l, buf = buf.split(b"\n", 1); m = re.search(r"#inf=(\d+).*?backlog=(\d+)", l.decode("ascii", "replace"))
        if m: marks.append((time.time() - t0, int(m.group(1)), int(m.group(2))))
a, b = marks[2], marks[-1]
print(f"factor {f}: {(b[1]-a[1])/(b[0]-a[0]):.1f} frames/s processed (real time is 50.0), backlog at end {b[2]} samples, sent {sent} samples in {time.time()-t0:.1f} s")
