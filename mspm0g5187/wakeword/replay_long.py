#!/usr/bin/env python3
"""False triggers through the replay input: 10 minutes of silence (with a faint noise floor) or of the Mac's
voices reading sentences that are not the wake word.

  replay_long.py --mode silence|speech --secs 600 --level -20 --out results/...

The board's WAKE lines are counted; the clip is sent at 8,000 samples a second like replay.py."""
import argparse, re, sys, time, tempfile
import numpy as np, serial
sys.path.insert(0, __file__.rsplit("/", 1)[0])
from replay import render  # noqa
SENT = ["The weather this afternoon is sunny with a light breeze from the west.",
        "Please remember to pick up milk and bread on your way home tonight.",
        "The quick brown fox jumps over the lazy dog near the river bank.",
        "Texas Instruments makes microcontrollers that run neural networks on a chip.",
        "Can you turn on the kitchen light and set the thermostat to seventy one.",
        "Okay, let's get started with the meeting, who has the agenda for today.",
        "Kilby was an engineer who built the first integrated circuit in nineteen fifty eight.",
        "Hey, okay, what time does the game start on Saturday evening.",
        "I would like a large coffee with a little cream and no sugar please."]
ap = argparse.ArgumentParser(); ap.add_argument("--mode", default="speech"); ap.add_argument("--secs", type=float, default=600)
ap.add_argument("--level", type=float, default=-20); ap.add_argument("--voices", nargs="+", default=["Samantha", "Daniel", "Karen"])
ap.add_argument("--set", default="all", help="all, or plain: leave out the sentences that contain okay or Kilby"); ap.add_argument("--out", required=True); ap.add_argument("--port", default="/dev/cu.usbmodemMG0200011"); ap.add_argument("--baud", type=int, default=230400)
a = ap.parse_args()
import os
tmp = tempfile.mkdtemp(); s = serial.Serial(a.port, a.baud, timeout=0); log = open(a.out, "w")
def P(*x): print(*x, flush=True); print(*x, file=log, flush=True)
rng = np.random.default_rng(2)
clips = []
if a.set == "plain":
    SENT = [t for t in SENT if "kay" not in t.lower() and "kilby" not in t.lower()]
if a.mode == "speech":
    for v in a.voices:
        for t in SENT:
            clips.append((v, t, np.concatenate([render(v, t, a.level, tmp), np.zeros(8000)])))
total_speech = sum(len(c[2]) for c in clips) / 8000
P(f"# replay_long mode={a.mode} secs={a.secs} level={a.level} voices={a.voices} sentences={len(SENT)} one pass={total_speech:.0f}s")
buf = b""; wakes = []; base = None; last_inf = None
def pump():
    global buf, last_inf
    buf += s.read(4096)
    while b"\n" in buf:
        l, buf = buf.split(b"\n", 1); l = l.decode("ascii", "replace").strip()
        m = re.search(r"#inf=(\d+)", l)
        if m: last_inf = int(m.group(1))
        if l.startswith("WAKE"): wakes.append((time.time() - t0, l)); P(f"{time.time()-t0:8.1f} {l}")
t0 = time.time(); sent = 0; k = 0
# flush: 4 s of silence first (the board's window holds the last 2 s of whatever came before); WAKE lines in it are not counted
for i in range(0, 8000 * 4, 160):
    due = t0 + sent / 8000.0
    while time.time() < due: time.sleep(0.001)
    s.write(np.zeros(160, dtype="<i2").tobytes()); sent += 160; pump()
wakes.clear(); t0 = time.time(); sent = 0
while time.time() - t0 < a.secs:
    if a.mode == "speech":
        v, t, x = clips[k % len(clips)]; k += 1
    else:
        x = rng.normal(0, 10 ** (-60 / 20) * 32767, 8000 * 5)
    x = np.clip(x, -32768, 32767).astype("<i2"); n0 = sent
    for i in range(0, len(x), 160):
        due = t0 + sent / 8000.0
        while time.time() < due: time.sleep(0.001)
        s.write(x[i:i + 160].tobytes()); sent += 160; pump()
        if time.time() - t0 >= a.secs: break
end = time.time() + 1.5
while time.time() < end: pump(); time.sleep(0.02)
P(f"RESULT mode={a.mode} secs={time.time()-t0:.0f} audio_sent={sent/8000:.0f}s level={a.level} sentences={k} false_wakes={len(wakes)} last_inf={last_inf}")
