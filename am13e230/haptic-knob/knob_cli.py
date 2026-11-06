#!/usr/bin/env python3
"""Run the haptic knob from the terminal and print what it reports.
  python3 haptic-knob/knob_cli.py [--port /dev/cu.usbmodemAL1300011] [--detents 12] [--strength 5] [--seconds 20]
Needs pyserial (pip install pyserial). Sends 'H', then the settings, prints each telemetry line, sends 's' on Ctrl-C or
after --seconds (the board stops itself at 20 s anyway). The log goes to haptic-knob/last-run.log.
Only run this with the motor shaft free, your hands clear and the 12 V plug within reach.
"""
import argparse, sys, time, serial
ap = argparse.ArgumentParser()
ap.add_argument("--port", default="/dev/cu.usbmodemAL1300011")
ap.add_argument("--detents", type=int, default=12, choices=[2, 3, 4, 6, 8, 12])
ap.add_argument("--strength", type=int, default=5)
ap.add_argument("--seconds", type=float, default=20)
a = ap.parse_args()
if not 1 <= a.strength <= 10 or a.seconds > 20:
    sys.exit("strength 1..10, seconds at most 20")
s = serial.Serial(a.port, 115200, timeout=0.2)
log = open(__file__.rsplit("/", 1)[0] + "/last-run.log", "w")
def say(b): s.write(b); s.flush()
s.reset_input_buffer()
say(b"H"); time.sleep(0.4)
say(f"D{a.detents}\n".encode()); time.sleep(0.05); say(f"W{a.strength}\n".encode())
t0 = time.time(); rows = []
try:
    while time.time() - t0 < a.seconds:
        l = s.readline().decode("latin1").strip()
        if l:
            print(l); log.write(l + "\n")
            if l.startswith("H,"): rows.append(l.split(","))
            if l.startswith("haptic done"): break
finally:
    say(b"s"); time.sleep(0.3)
    for l in s.read(2000).decode("latin1").splitlines():
        if l.strip(): print(l.strip()); log.write(l.strip() + "\n")
if rows:
    nets = [int(r[2]) for r in rows]
    print(f"{len(rows)} telemetry lines, net sectors {min(nets)} to {max(nets)}")
