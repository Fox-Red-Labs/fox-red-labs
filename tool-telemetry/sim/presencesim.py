#!/usr/bin/env python3
"""A pretend presence sensor (think mmWave radar) for the field-data framework: twice a second it writes a $FD line
for app 2 (portability/fielddata/apps/presence.json) to a tag's serial port. A person comes and goes; while they are in
the room they sit still or move. The sensor's own `state` is right most of the time but misses still people now and
then, which is the kind of mistake a model trained on the features can fix. The truth goes to --truth as CSV.
    python presencesim.py <port|-> [--minutes 5] [--speed 1] [--seed 1] [--truth truth.csv]"""
import argparse
import os
import random
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "portability", "fielddata"))
import fd  # noqa: E402

STATES = ("empty", "still", "moving")


def iso(t):
    return time.strftime("%Y-%m-%dT%H:%M:%S", time.gmtime(t)) + ".%03dZ" % int((t % 1) * 1000)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("port")
    ap.add_argument("--minutes", type=float, default=5)
    ap.add_argument("--speed", type=float, default=1)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--truth")
    ap.add_argument("--device", default="40:F3:B0:xx:xx:xx", help="the tag's address, for the label file")
    a = ap.parse_args()
    r = random.Random(a.seed)
    app = fd.load_apps()[2]
    out = None
    if a.port != "-":
        import serial
        out = serial.Serial(a.port, 115200, timeout=0)
    truth = open(a.truth, "w") if a.truth else None
    if truth:
        truth.write("device,start_utc,end_utc,label\n")
    seg_state, seg_start = None, time.time()
    state, left, rng_cm, n, t0, sim_t = 0, r.uniform(5, 20), 0.0, 0, time.time(), 0.0
    quiet = False
    temp = 31.0
    while time.time() - t0 < a.minutes * 60:
        sim_t += 0.5 * a.speed
        left -= 0.5 * a.speed
        if left <= 0:                                   # the person comes, settles, moves, leaves
            state = {0: 2, 2: r.choice([1, 1, 0]), 1: r.choice([2, 2, 0])}[state] if state else 2
            left = {0: r.uniform(10, 40), 1: r.uniform(15, 60), 2: r.uniform(4, 15)}[state]
            quiet = state == 1 and r.random() < 0.4      # reading at a desk: barely any motion for the whole stretch
            if state == 2 and rng_cm == 0:
                rng_cm = r.uniform(300, 500)
        if state == 0:
            rng_cm, motion, still, conf = 0.0, r.uniform(0, 40), r.uniform(0, 30), r.uniform(80, 99)
        elif state == 1:
            rng_cm = max(80, rng_cm + r.gauss(0, 3)) if rng_cm else r.uniform(150, 400)
            motion, still, conf = r.uniform(10, 120), r.uniform(150, 600), r.uniform(45, 90)
            if quiet:
                motion, still, conf = r.uniform(0, 45), r.uniform(60, 200), r.uniform(30, 70)
        else:
            rng_cm = min(600, max(60, (rng_cm or 400) + r.gauss(0, 25)))
            motion, still, conf = r.uniform(300, 1600), r.uniform(80, 400), r.uniform(70, 99)
        sensor = state
        if state == 1 and (quiet or (still < 260 and r.random() < 0.6)):
            sensor = 0                                   # the sensor's own logic (energy thresholds) misses a quiet sitter
        if state == 0 and motion > 30 and r.random() < 0.05:
            sensor = 2                                   # a fan or a curtain
        temp += r.gauss(0, 0.05)
        line = fd.line_for(app, n, state=STATES[sensor], confidence_pct=int(conf), range_cm=int(rng_cm),
                           motion_energy=int(motion), still_energy=int(still), people=1 if state else 0,
                           temp_c=int(round(temp)))
        n = (n + 1) & 0xFFFF
        if out:
            out.write(line.encode())
            out.read(4096)
        else:
            sys.stdout.write(line)
        now = time.time()
        if truth and seg_state != state:
            if seg_state is not None:
                truth.write("%s,%s,%s,%s\n" % (a.device, iso(seg_start), iso(now), STATES[seg_state]))
                truth.flush()
            seg_state, seg_start = state, now
        time.sleep(0.5)
    if truth and seg_state is not None:
        truth.write("%s,%s,%s,%s\n" % (a.device, iso(seg_start), iso(time.time()), STATES[seg_state]))
    print("sent %d lines" % n, file=sys.stderr)


if __name__ == "__main__":
    main()
