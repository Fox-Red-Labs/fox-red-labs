#!/usr/bin/env python3
"""Acoustic test of the alarm listener (image alarm_detect, real microphone): the Mac's speaker plays decoys
and alarm patterns, the Mac presses S2 over the serial port, and the board's decisions are logged and scored.
Never plays past --deadline (local time HH:MM); every item checks the clock first and afplay is cut at it.

  live_test.py --deadline 08:34 [--volume 60] [--log FILE]

Phase 1, generic mode (nothing learned): decoys, then a temporal-3 smoke alarm (3,150 Hz).  expect: no ALARM
         during decoys, ALARM on the smoke alarm (recognized through the air).
Phase 2, learn: S2 pressed from the Mac, the same alarm plays.                              expect: LEARNED smoke
Phase 3, gates: the same alarm 30 dB quieter (TV), the same alarm at full volume, a 3,600 Hz temporal-3 alarm.
         expect: no ALARM (too quiet, or not heard at all), ALARM, IGNORED (tone differs).
"""
import argparse
import datetime as dt
import json
import os
import subprocess
import sys
import threading
import time

import numpy as np
import serial

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import reel  # noqa: E402

SR = reel.SR
D = os.path.abspath("out/alarm/live")


def build():
    os.makedirs(D, exist_ok=True)
    rng = np.random.default_rng(9)
    items = {
        "oven_timer": reel.pattern(3100, 0.5, 0.5, 0.5, 1, 12),
        "backup_beeper": reel.pattern(1100, 0.5, 0.5, 0.5, 1, 12),
        "microwave_3": np.concatenate([reel.pattern(2400, 0.6, 0.4, 30, 3, 3.0), np.zeros(SR * 4)]),
        "alarm_clock": reel.pattern(2800, 0.07, 0.07, 0.5, 4, 12),
        "t3_too_fast": reel.pattern(3200, 0.25, 0.25, 0.75, 3, 12),
        "t3_short_gap": reel.pattern(3200, 0.5, 0.5, 0.5, 3, 12),
        "t4_three_pulses": reel.pattern(3200, 0.1, 0.1, 5.0, 3, 12),
        "t4_short_gap": reel.pattern(3200, 0.1, 0.1, 1.5, 4, 12),
        "chirp": np.concatenate([np.concatenate([reel.tone(3300, 0.05), np.zeros(int(SR * 5))]) for _ in range(2)]),
        "whistled_tune": reel.whistle(12, rng),
        "kettle": reel.tone(2600, 12, ((2, -25),), am=0.1) * np.linspace(0.2, 1, 12 * SR),
        "smoke_t3_3150": reel.pattern(3150, 0.5, 0.5, 1.5, 3, 25),
        "smoke_t3_3600": reel.pattern(3600, 0.5, 0.5, 1.5, 3, 25),
    }
    for k, y in items.items():
        reel.wav(os.path.join(D, k + ".wav"), y)
    tones = "/System/Library/PrivateFrameworks/ToneLibrary.framework/Versions/A/Resources/Ringtones"
    return tones


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--deadline", required=True)
    ap.add_argument("--volume", type=int, default=60)
    ap.add_argument("--port", default="/dev/cu.usbmodemMG0200011")
    ap.add_argument("--log", default=os.path.join(D, "live.log"))
    a = ap.parse_args()
    hh, mm = map(int, a.deadline.split(":"))
    now = dt.datetime.now()
    deadline = now.replace(hour=hh, minute=mm, second=0, microsecond=0).timestamp()
    tones = build()
    s = serial.Serial(a.port, 921600, timeout=0.2)
    s.reset_input_buffer()
    lines, stop = [], threading.Event()

    def reader():
        buf = b""
        while not stop.is_set():
            buf += s.read(4096)
            while b"\n" in buf:
                ln, buf = buf.split(b"\n", 1)
                t = ln.decode(errors="replace").strip()
                if t:
                    lines.append((time.time(), t))
    th = threading.Thread(target=reader, daemon=True)
    th.start()
    subprocess.run(["osascript", "-e", f"set volume output volume {a.volume}"])
    marks = []

    def play(name, path, secs, vol=1.0, gap=6, press=False):
        if time.time() + secs + gap > deadline:
            marks.append((name, None, None))
            print(f"skipped {name}: would run past {a.deadline}", flush=True)
            return
        if press:
            s.write(bytes([0xA5, 0x4E]))             # learn (a plain S2 press clears an alarm that is still on)
        st = time.time()
        p = subprocess.Popen(["afplay", "-v", str(vol), "-t", str(secs), path])
        while p.poll() is None:
            if time.time() > deadline - 5:
                p.kill()
            time.sleep(0.2)
        en = time.time()
        marks.append((name, st, en))
        print(f"{time.strftime('%H:%M:%S')} {name:22s} {en - st:5.1f} s", flush=True)
        time.sleep(gap)

    # phase 1: decoys, then the alarm (generic mode)
    for k in ["oven_timer", "backup_beeper", "microwave_3", "alarm_clock", "t3_too_fast", "t3_short_gap",
              "t4_three_pulses", "t4_short_gap", "chirp", "whistled_tune", "kettle"]:
        play("decoy_" + k, os.path.join(D, k + ".wav"), 12)
    for r in ["Alarm.m4r", "Radar.m4r", "Old Phone.m4r", "Doorbell.m4r"]:
        play("decoy_ring_" + r.split(".")[0].replace(" ", "_"), os.path.join(tones, r), 10)
    play("decoy_music", os.path.expanduser("~/Music/iTunes/iTunes Media/Music/Taylor Swift/This Love - Single/01 This Love.m4a"), 15)
    play("generic_smoke", os.path.join(D, "smoke_t3_3150.wav"), 25, gap=8)
    # phase 2: learn from the Mac's S2 press
    play("learn_smoke", os.path.join(D, "smoke_t3_3150.wav"), 25, press=True, gap=10)
    # phase 3: gates
    play("tv_30dB_quieter", os.path.join(D, "smoke_t3_3150.wav"), 25, vol=0.03, gap=9)
    play("learned_full", os.path.join(D, "smoke_t3_3150.wav"), 25, gap=9)
    play("other_tone_3600", os.path.join(D, "smoke_t3_3600.wav"), 25, gap=9)
    s.write(bytes([0xA5, 0x4C]))                       # clear any alarm left on
    time.sleep(2)
    stop.set()
    th.join()
    with open(a.log, "w") as f:
        for t, ln in lines:
            f.write(f"{t:.2f} {ln}\n")
    ev = [(t, ln) for t, ln in lines if ln.startswith(("ALARM", "IGNORED", "LEARNED", "LEARN", "CLEARED"))]
    for i, (name, st, en) in enumerate(marks):
        if st is None:
            print(f"  {name:22s} skipped")
            continue
        nxt = next((m[1] for m in marks[i + 1:] if m[1]), en + 10)
        hits = [ln for t, ln in ev if st <= t < nxt]
        print(f"  {name:22s} {'; '.join(h[:80] for h in hits) or '-'}")
    print(f"inferences {sum(ln.startswith('I n=') for _, ln in lines)}; done {time.strftime('%H:%M:%S')}; log {a.log}")
    json.dump(dict(marks=marks), open(a.log.replace(".log", ".json"), "w"))


if __name__ == "__main__":
    main()
