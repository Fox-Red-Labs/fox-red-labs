#!/usr/bin/env python3
"""Local latency of the smoke image: time from the last byte of a `SMOKE n` / `CO n` line written to the XDS110 backchannel
to the `FRL OK` reply (the reply is sent after the cluster attribute is set, before any Thread report leaves the chip).
   python3 latency_uart.py /dev/cu.usbmodemLB07021W1 [events]"""
import serial, sys, time, statistics
port = sys.argv[1]; n = int(sys.argv[2]) if len(sys.argv) > 2 else 20
s = serial.Serial(port, 115200, timeout=0.01)
lines = [b"SMOKE 1\n", b"SMOKE 0\n", b"CO 1\n", b"CO 0\n"]
lat = []
for i in range(n):
    s.reset_input_buffer()
    buf = b""
    t0 = time.perf_counter()
    s.write(lines[i % 4]); s.flush()
    while b"FRL OK" not in buf and time.perf_counter() - t0 < 3:
        buf += s.read(256)
    if b"FRL OK" in buf: lat.append((time.perf_counter() - t0) * 1000)
    time.sleep(0.7)
print(f"{len(lat)} of {n} replies; median {statistics.median(lat):.0f} ms, worst {max(lat):.0f} ms, best {min(lat):.0f} ms")
print(" ".join(f"{x:.0f}" for x in lat))
