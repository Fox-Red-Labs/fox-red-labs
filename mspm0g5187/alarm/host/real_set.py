#!/usr/bin/env python3
"""Turn recordings from the board's microphone (rec.py WAVs) into a labeled set for train_alarm.py --real.

  real_set.py OUT.npz alarm1.wav:t3 alarm2.wav:t4[:3150] room.wav:neg reel.wav:neg [--mix 20]
  (an optional third field gives the alarm's tone in Hz when another sound is louder)

t3 / t4: an alarm recording. Its beeps are found from the dominant tone's envelope (the loudest tonal
peak, band-passed +-120 Hz, 5 ms smoothing, on when within 12 dB of the loudest beep), grouped by the
quiet between them, and labeled with synth.window_labels, the rule the synthetic scenes use.
neg: anything without an alarm; every window is 'other'.
--mix N also writes N mixtures of a random alarm recording with a random neg recording at random gains
(the alarm 0 to 30 dB above the background), so the real horn is heard over real room sound.
"""
import argparse
import os
import sys
import wave

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import features as F  # noqa: E402
import synth  # noqa: E402


def load(path):
    with wave.open(path) as w:
        b = np.frombuffer(w.readframes(w.getnframes()), np.uint8).reshape(-1, 3).astype(np.int32)
    v = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
    v = np.where(v & 0x800000, v - 0x1000000, v)
    return (v >> 6).astype(np.float64) / 2 ** 17


def find_beeps(x, kind, f0=None):
    if not f0:
        fr = F.frame_features(x)
        tonal = fr[:, 11] >= F.TONAL
        if not tonal.any():
            return [], 0.0
        i = np.argmax(np.where(tonal, fr[:, 12], -1e9))
        f0 = fr[i, 10] * 4000
    X = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1 / F.FS)
    X[np.abs(f - f0) > 120] = 0
    env = np.abs(np.fft.irfft(X, len(x)))
    k = int(0.005 * F.FS)
    env = np.convolve(env, np.ones(k) / k, "same")
    on = env > env.max() * 10 ** (-12 / 20)
    edges = np.flatnonzero(np.diff(on.astype(int)))
    if on[0]:
        edges = np.r_[0, edges]
    if on[-1]:
        edges = np.r_[edges, len(on) - 1]
    beeps = [(s / F.FS, e / F.FS) for s, e in zip(edges[::2], edges[1::2]) if (e - s) / F.FS > 0.03]
    gap = 1.0 if kind == "t3" else 2.0
    out, g = [], 0
    for j, (s, e) in enumerate(beeps):
        if j and s - beeps[j - 1][1] > gap:
            g += 1
        out.append((s, e, g))
    return out, f0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("items", nargs="+")
    ap.add_argument("--mix", type=int, default=0)
    ap.add_argument("--seed", type=int, default=0)
    a = ap.parse_args()
    rng = np.random.default_rng(a.seed)
    streams = []
    alarms, negs = [], []
    for it in a.items:
        parts = it.split(":")
        p, kind = parts[0], parts[1]
        hint = float(parts[2]) if len(parts) > 2 else None
        x = load(p)
        if kind == "neg":
            negs.append(x)
            streams.append((x, [], None))
        else:
            b, f0 = find_beeps(x, kind, hint)
            print(f"{os.path.basename(p)}: {kind}, tone {f0:.0f} Hz, {len(b)} beeps in {b[-1][2] + 1 if b else 0} groups; "
                  f"first on-times {[round(e - s, 2) for s, e, _ in b[:4]]}")
            alarms.append((x, b, kind))
            streams.append((x, b, kind))
    for _ in range(a.mix):
        x, b, kind = alarms[rng.integers(len(alarms))]
        n = negs[rng.integers(len(negs))]
        m = min(len(x), len(n))
        o = rng.integers(0, len(n) - m + 1)
        g = 10 ** (rng.uniform(0, 30) / 20) * np.sqrt(np.mean(n ** 2)) / (np.sqrt(np.mean(x ** 2)) + 1e-12)
        y = n[o:o + m] + x[:m] * min(g, 0.9 / (np.abs(x).max() + 1e-12))
        streams.append((y, [bb for bb in b if bb[1] <= m / F.FS], kind))
    frames, Y, W, S, sid = [], [], [], [], []
    for k, (x, b, kind) in enumerate(streams):
        fr = F.frame_features(x)
        st, y, w = synth.window_labels(b, "t3" if kind == "t3" else "t4", len(fr), 8)
        frames.append(fr)
        Y.append(y)
        W.append(w)
        S.append(st)
        sid.append(np.full(len(st), k))
    L = max(len(f) for f in frames)
    pad = np.full((len(frames), L, 13), -130.0, np.float32)
    pad[:, :, 10:12] = 0
    for k, f in enumerate(frames):
        pad[k, :len(f)] = f
    y = np.concatenate(Y)
    np.savez_compressed(a.out, frames=pad, y=y, w=np.concatenate(W), start=np.concatenate(S), stream=np.concatenate(sid))
    print(f"{a.out}: {len(streams)} streams, windows per class {np.bincount(y, minlength=3).tolist()}")


if __name__ == "__main__":
    main()
