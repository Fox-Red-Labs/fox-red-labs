#!/usr/bin/env python3
"""Hands-free live test: the Mac speaks near the board, the board's serial lines are stamped on the Mac clock.

  live.py --voices Samantha Daniel --n 20 --vol 70 --out results/live-....txt
  live.py --mode silence --secs 600 --out results/...        # false triggers in silence
  live.py --mode speech --secs 600 --out results/...         # false triggers while the Mac says other sentences

A hit is a WAKE line stamped inside [say start, say start + 4 s]. Latency is stamped from the start of the
`say` command to the arrival of the WAKE line; the spoken phrase itself lasts DUR seconds (measured with
`say -o`), and `say` needs about 0.2 to 0.4 s to start speaking, so latency after the end of the word is
roughly latency - startup - DUR.
"""
import argparse, os, re, subprocess, sys, tempfile, time, wave
import numpy as np, serial

PORT = "/dev/cu.usbmodemMG0200011"
SENT = ["The weather this afternoon is sunny with a light breeze from the west.",
        "Please remember to pick up milk and bread on your way home tonight.",
        "The quick brown fox jumps over the lazy dog near the river bank.",
        "Texas Instruments makes microcontrollers that run neural networks on a chip.",
        "Can you turn on the kitchen light and set the thermostat to seventy one.",
        "Okay, let's get started with the meeting, who has the agenda for today.",
        "Kilby was an engineer who built the first integrated circuit in nineteen fifty eight.",
        "Hey, okay, what time does the game start on Saturday evening.",
        "I would like a large coffee with a little cream and no sugar please."]

def dur_of(voice, text):
    t = tempfile.mkdtemp(); f = os.path.join(t, "a.aiff")
    subprocess.run(["say", "-v", voice, "-o", f, text], check=True)
    w = subprocess.run(["afinfo", f], capture_output=True, text=True).stdout
    m = re.search(r"estimated duration: ([\d.]+) sec", w)
    return float(m.group(1)) if m else float("nan")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mode", choices=["wake", "silence", "speech"], default="wake")
    ap.add_argument("--voices", nargs="+", default=["Samantha"]); ap.add_argument("--n", type=int, default=20)
    ap.add_argument("--text", default="OK Kilby"); ap.add_argument("--vol", type=int, default=70)
    ap.add_argument("--secs", type=float, default=600); ap.add_argument("--gap", type=float, default=5.0)
    ap.add_argument("--rate", type=int, default=None, help="words per minute for say")
    ap.add_argument("--out", required=True); ap.add_argument("--port", default=PORT); ap.add_argument("--baud", type=int, default=921600)
    a = ap.parse_args()
    s = serial.Serial(a.port, a.baud, timeout=0)
    subprocess.run(["osascript", "-e", f"set volume output volume {a.vol}"], check=True)
    log = open(a.out, "w")
    def P(*x):
        print(*x, flush=True); print(*x, file=log, flush=True)
    buf = b""
    def pump():
        nonlocal buf
        buf += s.read(8192); out = []
        while b"\n" in buf:
            l, buf = buf.split(b"\n", 1); out.append((time.time(), l.decode("ascii", "replace").strip()))
        return out
    P(f"# live mode={a.mode} vol={a.vol} text={a.text!r} port={a.port} start={time.strftime('%Y-%m-%d %H:%M:%S')}")
    def wait(sec):
        end = time.time() + sec; ev = []
        while time.time() < end:
            ev += pump(); time.sleep(0.01)
        return ev
    wait(1.0)
    if a.mode == "wake":
        for voice in a.voices:
            d = dur_of(voice, a.text); hits = 0; lat = []; extra = 0; mm = []
            for i in range(a.n):
                ev = []
                t0 = time.time()
                cmd = ["say", "-v", voice] + (["-r", str(a.rate)] if a.rate else []) + [a.text]
                p = subprocess.Popen(cmd)
                while time.time() - t0 < a.gap:
                    ev += pump(); time.sleep(0.005)
                p.wait()
                wk = [(t, l) for t, l in ev if l.startswith("WAKE")]
                inwin = [(t, l) for t, l in wk if 0 <= t - t0 <= 4.0]
                mxs = [int(m.group(1)) for t, l in ev for m in [re.search(r"maxmargin=(-?\d+)", l)] if m]
                mx = max(mxs) if mxs else None; mm.append(mx)
                hit = bool(inwin); hits += hit
                if hit: lat.append(inwin[0][0] - t0)
                extra += len(wk) - len(inwin)
                P(f"{voice} #{i+1:2d} hit={int(hit)} latency_from_say_start={'%.2f' % lat[-1] if hit else '-'}s wake_lines={len(wk)} window_maxmargin={mx}")
            P(f"RESULT voice={voice} vol={a.vol} hits={hits}/{a.n} extra_wake_lines={extra} phrase_dur={d:.2f}s median_latency_from_say_start={np.median(lat) if lat else float('nan'):.2f}s")
    else:
        t0 = time.time(); ev = []; wakes = 0; k = 0; lines = 0
        while time.time() - t0 < a.secs:
            if a.mode == "speech":
                txt = SENT[k % len(SENT)]; k += 1
                p = subprocess.Popen(["say", "-v", a.voices[k % len(a.voices)], txt])
                while p.poll() is None:
                    for t, l in pump():
                        lines += 1
                        if l.startswith("WAKE"): wakes += 1; P(f"{t - t0:8.2f} {l}")
                    time.sleep(0.01)
                for t, l in wait(1.0):
                    if l.startswith("WAKE"): wakes += 1; P(f"{t - t0:8.2f} {l}")
            else:
                for t, l in wait(1.0):
                    lines += 1
                    if l.startswith("WAKE"): wakes += 1; P(f"{t - t0:8.2f} {l}")
        P(f"RESULT mode={a.mode} secs={time.time() - t0:.0f} vol={a.vol} voices={','.join(a.voices)} false_wakes={wakes} sentences_spoken={k}")

main()
