#!/usr/bin/env python3
"""Silent end-to-end test of the alarm listener on the board: the Mac streams audio over the backchannel
UART (image alarm_inject: same features, NPU model, timing check and learned gate, the microphone left out),
in real time, and records what the board decides. No sound is played.

  inject.py [--port ...] [--room ROOM.wav] [--log FILE]

Scenes, each on top of real room sound recorded by the board's own microphone (rec.py):
  learn      press S2, then a temporal-3 smoke alarm at 3,150 Hz, loud (as beside the board)   expect LEARNED smoke
  alarm      the same alarm again                                                                expect ALARM smoke
  decoys     look-alike beeping, whistles, a kettle, chirps, speech, music, ringtones (held out)  expect no ALARM
  tv         the same alarm 31 dB quieter (a TV across the room)                                 expect IGNORED (too quiet)
  other_tone a temporal-3 alarm at 3,600 Hz, loud                                                expect IGNORED (tone)
  co         a temporal-4 CO pattern at 3,150 Hz, loud (the same horn)                          expect ALARM co
"""
import argparse
import json
import os
import sys
import threading
import time

import numpy as np
import serial

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import features as F  # noqa: E402
import real_set as R  # noqa: E402
import synth  # noqa: E402

FS = F.FS


def alarm(kind, f0, level_db, dur, rng):
    n = int(dur * FS)
    buf = np.zeros(n, np.float32)
    beeps = synth.pattern_times(rng, kind, 0.3, dur, scale=[1, 1, 1])
    synth.render_beeps(buf, beeps, lambda d: synth.tone(rng, f0, d, {2: -20, 3: -15}), 1.0)
    return synth.reverb(rng, buf) * 10 ** (level_db / 20)


def decoys(rng):
    bank = synth.Bank(os.path.abspath("out/alarm/bank"), "test")
    out = []
    for kind in ["oven", "beeper", "microwave", "clock", "t3_off", "t3_off", "t4_off", "t4_off", "chirp", "whistle",
                 "kettle", "doorbell", "phone", "birds", "bank", "bank", "bank", "bank"]:
        buf = np.zeros(int(20 * FS), np.float32)
        synth.decoy(rng, buf, bank, 0.5, 18, -30, kind=kind)
        out.append((kind, buf))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/cu.usbmodemMG0200011")
    ap.add_argument("--room", default=os.path.abspath("out/alarm/rec/room2.wav"))
    ap.add_argument("--log", default=os.path.abspath("out/alarm/inject.log"))
    ap.add_argument("--quick", action="store_true", help="learn, alarm, clear while the alarm still plays, then stop")
    a = ap.parse_args()
    rng = np.random.default_rng(5)
    room = R.load(a.room)
    room = room - room.mean()

    def bed(dur):
        n = int(dur * FS)
        r = np.tile(room, n // len(room) + 2)
        o = rng.integers(len(room))
        return r[o:o + n].astype(np.float32)

    def co_like(k, gap, dur):
        n = int(dur * FS)
        buf = np.zeros(n, np.float32)
        b = synth.pattern_times(rng, (0.1, 0.1, gap, k), 0.3, dur, scale=[1, 1, 1])
        synth.render_beeps(buf, b, lambda d: synth.tone(rng, 3150, d, {2: -20, 3: -15}), 1.0)
        return synth.reverb(rng, buf) * 10 ** (-14 / 20)

    plan = [("warmup", None, bed(10)),
            ("generic_co_3_pulses", None, bed(30) + co_like(3, 5.0, 30)), ("gap", None, bed(9)),
            ("generic_co_1.5s_gap", None, bed(25) + co_like(4, 1.5, 25)), ("gap", None, bed(9)),
            ("generic_co_real", None, bed(30) + co_like(4, 5.0, 30)), ("clear", "press", bed(10)),
            ("learn", "learn", bed(25) + alarm("t3", 3150, -14, 25, rng)),
            ("quiet", None, bed(12)),
            ("alarm", None, bed(30) + alarm("t3", 3150, -14, 30, rng)),
            ("clear", "press", bed(12))]
    if a.quick:
        plan = plan[:9] + [("smoke", None, bed(25) + alarm("t3", 3150, -14, 25, rng)), ("clear", "press", bed(12)),
                           ("co_same_horn", None, bed(30) + co_like(4, 5.0, 30)), ("clear", "press", bed(12)),
                           ("co_tv_quiet", None, bed(30) + co_like(4, 5.0, 30) * 10 ** (-31 / 20)), ("quiet", None, bed(9))]
    for k, x in ([] if a.quick else decoys(rng)):
        plan.append(("decoy_" + k, None, bed(len(x) / FS) + x))
        plan.append(("gap", None, bed(9)))
    plan += [] if a.quick else [("tv", None, bed(30) + alarm("t3", 3150, -45, 30, rng)), ("gap", None, bed(9)),
             ("other_tone", None, bed(30) + alarm("t3", 3600, -14, 30, rng)), ("gap", None, bed(9)),
             ("co", None, bed(40) + alarm("t4", 3150, -14, 40, rng)), ("gap", None, bed(9))]

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
    marks = []
    CH = 128
    t0 = time.time()
    sent = 0
    for name, cmd, x in plan:
        if cmd == "press":
            s.write(bytes([0xA5, 0x4C]))
        if cmd == "learn":
            s.write(bytes([0xA5, 0x4E]))
        st = time.time()
        q = np.clip(np.round(x * 32767), -32768, 32767).astype("<i2")
        for i in range(0, len(q), CH):
            c = q[i:i + CH]
            s.write(bytes([0xA5, 0x5C, len(c)]) + c.tobytes())
            sent += len(c)
            lag = t0 + sent / FS - time.time()
            if lag > 0:
                time.sleep(lag)
        marks.append((name, st, time.time()))
        print(f"{time.strftime('%H:%M:%S')} {name:16s} {len(x)/FS:5.1f} s", flush=True)
    time.sleep(3)
    stop.set()
    th.join()
    with open(a.log, "w") as f:
        for t, ln in lines:
            f.write(f"{t:.2f} {ln}\n")
    # score
    ev = [(t, ln) for t, ln in lines if ln.startswith(("ALARM", "IGNORED", "LEARNED", "LEARN", "CLEARED", "FORGOT"))]
    rows = []
    for i, (name, st, en) in enumerate(marks):
        nxt = marks[i + 1][1] if i + 1 < len(marks) else en + 3
        hits = [ln for t, ln in ev if st <= t < nxt]
        rows.append(dict(scene=name, events=hits))
        if name != "gap":
            print(f"  {name:16s} {'; '.join(h[:90] for h in hits) or '-'}")
    inf = [ln for _, ln in lines if ln.startswith("I n=")]
    print(f"inferences {len(inf)}; board lines {len(lines)}; log {a.log}")
    json.dump(dict(marks=marks, rows=rows), open(a.log.replace(".log", ".json"), "w"), indent=1)


if __name__ == "__main__":
    main()
