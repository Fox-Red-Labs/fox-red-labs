#!/usr/bin/env python3
"""Replay speech into the wake-word model through the serial port (wakeword_replay image).

  replay.py --voices Samantha Daniel --n 20 --level -20 [--noise -60] --out results/replay-....txt

For each utterance: render `say -v VOICE "OK Kilby"` to 8 kHz mono, scale the peak to LEVEL dBFS, put it in
4 s of silence (or noise), and send the 16-bit samples to the board at 8,000 a second, 20 ms at a time. The
board's WAKE lines carry the inference number; inference k has seen k*160 samples, so the time of a detection
on the audio clock is k*20 ms. A hit is a WAKE inside [utterance start, utterance end + 1.5 s].
"""
import argparse, os, re, subprocess, sys, tempfile, time, wave
import numpy as np, serial

PORT = "/dev/cu.usbmodemMG0200011"

def render(voice, text, level_db, tmp):
    aiff = os.path.join(tmp, "u.aiff"); wav = os.path.join(tmp, "u.wav")
    subprocess.run(["say", "-v", voice, "-o", aiff, text], check=True)
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", aiff, "-ac", "1", "-ar", "8000", "-sample_fmt", "s16", wav], check=True)
    w = wave.open(wav); x = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(np.float64)
    pk = np.abs(x).max()
    return x * (10 ** (level_db / 20) * 32767 / pk)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--voices", nargs="+", default=["Samantha"]); ap.add_argument("--n", type=int, default=20)
    ap.add_argument("--text", default="OK Kilby"); ap.add_argument("--level", type=float, default=-20)
    ap.add_argument("--noise", type=float, default=None, help="white noise floor, dBFS rms")
    ap.add_argument("--gap", type=float, default=4.0); ap.add_argument("--out", required=True)
    ap.add_argument("--port", default=PORT); ap.add_argument("--baud", type=int, default=230400)
    a = ap.parse_args()
    s = serial.Serial(a.port, a.baud, timeout=0)
    rng = np.random.default_rng(1)
    tmp = tempfile.mkdtemp()
    log = open(a.out, "w"); board = open(a.out + ".board", "w")
    def P(*x):
        print(*x, flush=True); print(*x, file=log, flush=True)
    P(f"# replay text={a.text!r} level={a.level} dBFS peak noise={a.noise} gap={a.gap}s port={a.port}")
    # sync: read until the board reports its inference counter
    rxbuf = b""
    def pump():
        nonlocal rxbuf
        rxbuf += s.read(4096)
        out = []
        while b"\n" in rxbuf:
            l, rxbuf = rxbuf.split(b"\n", 1); l = l.decode("ascii", "replace").strip(); out.append(l)
            if l.startswith("#inf"): board.write(f"{time.time():.2f} {l}\n"); board.flush()
        return out
    base_inf = None
    t_end = time.time() + 3
    while time.time() < t_end:
        for l in pump():
            m = re.search(r"#inf=(\d+)", l)
            if m: base_inf = int(m.group(1))
        time.sleep(0.05)
    for voice in a.voices:
        hits = 0; lat = []; falses = 0; margins = []
        for i in range(a.n):
            u = render(voice, a.text, a.level, tmp)
            lead = int(1.0 * 8000); tail = int(a.gap * 8000)
            x = np.concatenate([np.zeros(lead), u, np.zeros(tail)])
            if a.noise is not None:
                x += rng.normal(0, 10 ** (a.noise / 20) * 32767, len(x))
            x = np.clip(x, -32768, 32767).astype("<i2")
            ustart, uend = lead, lead + len(u)
            # inference counter at start of this clip: samples already sent / 160
            sent = 0; t0 = time.time(); events = []; mx = -999
            chunk = 160
            while sent < len(x):
                due = t0 + sent / 8000.0
                while time.time() < due: time.sleep(0.001)
                s.write(x[sent:sent + chunk].tobytes()); sent += chunk
                for l in pump():
                    if l.startswith("WAKE"): events.append((time.time() - t0, l))
                    m = re.search(r"maxmargin=(-?\d+)", l)
                    if m and ustart / 8000 - 0.2 < time.time() - t0 < uend / 8000 + 1.5: mx = max(mx, int(m.group(1)))
            end = time.time() + 1.0
            while time.time() < end:
                for l in pump():
                    if l.startswith("WAKE"): events.append((time.time() - t0, l))
                time.sleep(0.02)
            inwin = [e for e in events if ustart / 8000 - 0.2 <= e[0] <= uend / 8000 + 1.5]
            out = [e for e in events if e not in inwin]
            hit = bool(inwin); hits += hit; falses += len(out)
            if hit: lat.append(inwin[0][0] - uend / 8000)
            margins.append(mx)
            times = ",".join("%.2f" % (e[0] - uend / 8000) for e in events)
            P(f"{voice} #{i+1:2d} wake_times_vs_word_end=[{times}] dur={len(u)/8000:.2f}s hit={int(hit)} first_wake_after_word_end={'%.2f' % lat[-1] if hit else '-'}s events_in_window={len(inwin)} outside={len(out)} windowmax_margin={mx}")
        P(f"RESULT voice={voice} level={a.level} noise={a.noise} hits={hits}/{a.n} false_outside={falses} median_latency_after_word_end={np.median(lat) if lat else float('nan'):.2f}s")

if __name__ == "__main__": main()
