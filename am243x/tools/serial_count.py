#!/usr/bin/env python3
"""Count what the LP-AM243 console sends in a time window, for checks against a known rate.
serial_count.py <secs> <log> [--port /dev/cu.usbmodemS24L00961] [--baud 115200]
Writes every line with its arrival time (s since the start) to <log> and prints BYTES <n> LINES <n> SECS <t>. Needs pyserial (use /usr/bin/python3)."""
import argparse, time
import serial

a = argparse.ArgumentParser()
a.add_argument("secs", type=float)
a.add_argument("log")
a.add_argument("--port", default="/dev/cu.usbmodemS24L00961")
a.add_argument("--baud", type=int, default=115200)
args = a.parse_args()
n = lines = bad = 0
buf = b""
with serial.Serial(args.port, args.baud, timeout=0.2) as port, open(args.log, "w") as out:
    time.sleep(0.3)
    port.reset_input_buffer()
    t0 = time.time()
    while time.time() - t0 < args.secs:
        d = port.read(4096)
        if not d:
            continue
        t = time.time() - t0
        n += len(d)
        buf += d
        while b"\n" in buf:
            ln, buf = buf.split(b"\n", 1)
            ok = ln.startswith(b"BUSY up ") and ln.endswith(b"\r")
            lines += 1
            bad += 0 if ok else 1
            out.write(f"{t:.3f} {'ok ' if ok else 'BAD'} {ln.decode('ascii', 'replace').strip()}\n")
    el = time.time() - t0
print(f"BYTES {n} LINES {lines} NOT_WELL_FORMED {bad} SECS {el:.2f}")
