"""Synthetic training scenes for the alarm listener: 40 s streams of room sound with a smoke alarm
(temporal-3), a CO alarm (temporal-4) or neither, mixed with decoys, plus per-window labels.

Timing of the real patterns (NFPA 72 18.4.2 and 18.4.3.2, each +-10 %):
  temporal-3: three 0.5 s beeps 0.5 s apart, then 1.5 s quiet, repeat (4 s cycle)
  temporal-4: four 0.1 s pulses 0.1 s apart, then 5 s quiet, repeat (5.8 s cycle)
Positives here vary each time by up to +-12 %; look-alike negatives are off by 35 % or more.

Window labels (128 frames, 8.26 s), from where the true alarm's beeps fall:
  smoke: one complete group of three beeps plus at least one beep of the next or previous group
  co:    one complete group of four pulses plus 3 s of its quiet gap inside the window
  ignore (weight 0): two or more alarm beeps in the window but neither rule met yet (onset, tail)
  other: everything else
The model judges patterns; loudness is not a label (a TV playing an alarm is still the pattern).
"""
import json
import os

import numpy as np

import features as F

FS = F.FS
STREAM_S = 40.0
CLASSES = ["other", "smoke", "co"]


def db2a(db):
    return 10 ** (db / 20)


class Bank:
    def __init__(self, d, split):
        man = json.load(open(os.path.join(d, "manifest.json")))
        self.items = [(m["kind"], np.load(os.path.join(d, m["name"] + ".npy")).astype(np.float32) / 32768)
                      for m in man if m["split"] == split]

    def clip(self, rng, dur):
        kind, x = self.items[rng.integers(len(self.items))]
        n = int(dur * FS)
        if len(x) <= n:
            return kind, np.tile(x, n // max(len(x), 1) + 1)[:n]
        s = rng.integers(len(x) - n)
        return kind, x[s:s + n]


# ---------------- sound pieces ----------------
def env(n, att, rel):
    e = np.ones(n, np.float32)
    a, r = min(int(att * FS), n // 2), min(int(rel * FS), n // 2)
    if a:
        e[:a] = np.linspace(0, 1, a)
    if r:
        e[-r:] = np.linspace(1, 0, r)
    return e


def tone(rng, f0, dur, harm=None, square=False, drift=0.0, am=0.0):
    n = max(int(dur * FS), 8)
    t = np.arange(n) / FS
    f = f0 * (1 + drift * np.sin(2 * np.pi * rng.uniform(0.5, 3) * t + rng.uniform(0, 6)))
    ph = 2 * np.pi * np.cumsum(f) / FS + rng.uniform(0, 6)
    if square:
        y = np.zeros(n)
        for h in (1, 3, 5, 7, 9, 11, 13):
            if f0 * h < FS / 2 - 300:
                y += np.sin(h * ph) / h
    else:
        y = np.sin(ph)
        for h, g in (harm or {}).items():
            if f0 * h < FS / 2 - 300:
                y += db2a(g) * np.sin(h * ph + rng.uniform(0, 6))
    if am:
        y *= 1 + am * np.sin(2 * np.pi * rng.uniform(40, 200) * t)
    y = y / (np.abs(y).max() + 1e-9)
    return (y * env(n, rng.uniform(0.001, 0.01), rng.uniform(0.002, 0.02))).astype(np.float32)


def place(buf, x, t, gain):
    i = int(t * FS)
    if i >= len(buf) or i + len(x) <= 0:
        return
    j0 = max(0, -i)
    i = max(i, 0)
    m = min(len(x) - j0, len(buf) - i)
    buf[i:i + m] += gain * x[j0:j0 + m]


def pink(rng, n):
    w = np.fft.rfft(rng.standard_normal(n))
    f = np.arange(len(w)) + 1.0
    w /= f ** rng.uniform(0.3, 0.6)
    y = np.fft.irfft(w, n)
    return (y / (y.std() + 1e-9)).astype(np.float32)


def reverb(rng, x):
    rt = rng.uniform(0.1, 0.7)
    n = int(rt * FS)
    h = rng.standard_normal(n) * np.exp(-6.9 * np.arange(n) / n)
    h *= db2a(rng.uniform(-25, -8)) / (np.sqrt((h ** 2).sum()) + 1e-9)
    h[0] = 1.0
    L = len(x) + n
    nf = 1 << int(np.ceil(np.log2(L)))
    y = np.fft.irfft(np.fft.rfft(x, nf) * np.fft.rfft(h, nf), nf)[:len(x)]
    return y.astype(np.float32)


def eq(rng, x):
    """Random coloration: microphone, speaker and room (tilt plus two gentle peaks or dips)."""
    X = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1 / FS)
    g = rng.uniform(-6, 6) * np.log2(np.maximum(f, 100) / 1000) / 3
    for _ in range(2):
        fc, bw, gd = rng.uniform(300, 6000), rng.uniform(0.3, 1.5), rng.uniform(-8, 8)
        g += gd * np.exp(-0.5 * (np.log2(np.maximum(f, 1) / fc) / bw) ** 2)
    g[f < 80] -= 20
    return np.fft.irfft(X * db2a(g), len(x)).astype(np.float32)


# ---------------- alarms ----------------
def alarm_sound(rng):
    if rng.random() < 0.15:
        f0, sq = rng.uniform(470, 580), True            # low-frequency (520 Hz) alarm, square wave
    else:
        f0, sq = rng.uniform(2600, 4200), False
    harm = {2: rng.uniform(-40, -12), 3: rng.uniform(-35, -10)}
    drift = rng.uniform(0, 0.01)
    am = rng.uniform(0, 0.3) if rng.random() < 0.3 else 0.0
    return lambda dur: tone(rng, f0, dur, harm, sq, drift, am)


def pattern_times(rng, kind, t0, t1, scale=None):
    """Beep on-times as (start, end, group) for a pattern from t0 to t1. kind: t3, t4 or a look-alike."""
    if kind == "t3":
        on, off, gap, k = 0.5, 0.5, 1.5, 3
    elif kind == "t4":
        on, off, gap, k = 0.1, 0.1, 5.0, 4
    else:
        on, off, gap, k = kind
    sc = scale if scale is not None else [rng.uniform(0.88, 1.12) for _ in range(3)]
    on, off, gap = on * sc[0], off * sc[1], gap * sc[2]
    beeps, t, g = [], t0, 0
    while t < t1:
        for i in range(k):
            jit = rng.uniform(-0.01, 0.01)
            s = t + i * (on + off) + jit
            if s + on > t1:
                break
            beeps.append((s, s + on, g))
        t += k * on + (k - 1) * off + gap
        g += 1
    return beeps


def render_beeps(buf, beeps, snd, gain):
    for s, e, _ in beeps:
        place(buf, snd(e - s), s, gain)


# ---------------- decoys ----------------
def off_scale(rng):
    """A timing factor clearly outside the +-10 % tolerance."""
    return rng.uniform(0.35, 0.65) if rng.random() < 0.5 else rng.uniform(1.4, 2.6)


HARD = ["bank", "oven", "oven", "beeper", "microwave", "microwave", "clock", "clock", "t3_off", "t3_off",
        "t4_off", "t4_off", "chirp", "whistle", "phone", "birds"]


def decoy(rng, buf, bank, t0, dur, level, hard=False, kind=None):
    """Add one decoy event to buf starting at t0 lasting about dur seconds. Returns its name."""
    kind = kind or (rng.choice(HARD) if hard else rng.choice(["bank", "bank", "bank", "oven", "microwave", "clock", "t3_off", "t4_off", "chirp",
                       "whistle", "kettle", "doorbell", "phone", "birds", "clink", "claps", "beeper"]))
    g = db2a(level)
    f = lambda: rng.uniform(1000, 4500)
    if kind == "bank":
        _, x = bank.clip(rng, dur)
        place(buf, reverb(rng, x / (np.sqrt(np.mean(x ** 2)) + 1e-6) * 0.3), t0, g)
    elif kind in ("oven", "beeper"):                                   # continuous even beeping, no long gap
        on = rng.uniform(0.08, 0.7)
        off = on * rng.uniform(0.6, 1.6) if rng.random() < 0.7 else rng.uniform(0.1, 0.8)
        fq = f() if kind == "oven" else rng.uniform(900, 1300)
        bs = pattern_times(rng, (on, off, off, 1), t0, t0 + dur, scale=[1, 1, 1])
        render_beeps(buf, bs, lambda d: tone(rng, fq, d, {2: -20, 3: -18}), g)
    elif kind == "microwave":
        n, on, off, fq = rng.integers(1, 6), rng.uniform(0.1, 1.0), rng.uniform(0.1, 0.6), f()
        bs = pattern_times(rng, (on, off, 30.0, int(n)), t0, t0 + min(dur, n * (on + off) + 0.1), scale=[1, 1, 1])
        render_beeps(buf, bs, lambda d: tone(rng, fq, d, {2: -15}), g)
    elif kind == "clock":                                              # groups of quick beeps, short gaps
        k, on, gap, fq = int(rng.integers(2, 6)), rng.uniform(0.05, 0.14), rng.uniform(0.3, 2.5), f()
        bs = pattern_times(rng, (on, on, gap, k), t0, t0 + dur, scale=[1, 1, 1])
        render_beeps(buf, bs, lambda d: tone(rng, fq, d, {3: -10}), g)
    elif kind == "t3_off":                                             # T3 look-alike, timing out of spec
        sc = [rng.uniform(0.9, 1.1) for _ in range(3)]
        sc[rng.integers(3)] = off_scale(rng)
        k = 3 if rng.random() < 0.6 else int(rng.choice([2, 4, 5]))
        if k != 3:
            sc = [rng.uniform(0.9, 1.1) for _ in range(3)]
        bs = pattern_times(rng, (0.5, 0.5, 1.5, k), t0, t0 + dur, scale=sc)
        render_beeps(buf, bs, alarm_sound(rng), g)
    elif kind == "t4_off":
        sc = [rng.uniform(0.9, 1.1) for _ in range(3)]
        k = 4
        r = rng.random()
        if r < 0.4:
            k = int(rng.choice([2, 3, 5, 6]))
        elif r < 0.7:
            sc[0] = sc[1] = rng.uniform(2.0, 4.0)
        else:
            sc[2] = rng.uniform(0.1, 0.5)
        bs = pattern_times(rng, (0.1, 0.1, 5.0, k), t0, t0 + dur, scale=sc)
        render_beeps(buf, bs, alarm_sound(rng), g)
    elif kind == "chirp":                                              # low-battery chirp
        fq, per = rng.uniform(2600, 4200), rng.uniform(3, 45)
        t = t0
        while t < t0 + dur:
            place(buf, tone(rng, fq, rng.uniform(0.02, 0.12)), t, g)
            t += per
    elif kind == "whistle":
        t = t0
        while t < t0 + dur:
            d = rng.uniform(0.12, 0.9)
            n = int(d * FS)
            f1 = rng.uniform(700, 3500)
            fr = f1 * (1 + np.linspace(0, rng.uniform(-0.3, 0.3), n)) * (1 + 0.02 * np.sin(2 * np.pi * rng.uniform(4, 7) * np.arange(n) / FS))
            y = np.sin(2 * np.pi * np.cumsum(fr) / FS) * env(n, 0.03, 0.05)
            y += 0.05 * rng.standard_normal(n) * env(n, 0.03, 0.05)
            place(buf, y.astype(np.float32), t, g)
            t += d + rng.uniform(0.02, 0.5)
    elif kind == "kettle":
        fq, d = rng.uniform(1500, 4000), dur
        x = tone(rng, fq, d, {2: -25}, drift=0.02) * np.linspace(0.2, 1, int(d * FS)).astype(np.float32)[:max(int(d * FS), 8)]
        place(buf, x + 0.1 * pink(rng, len(x)), t0, g)
    elif kind == "doorbell":
        t = t0
        while t < t0 + dur:
            for fq in [[659, 523], [784, 659, 523], [880, 698, 587, 440]][rng.integers(3)]:
                n = int(1.2 * FS)
                y = tone(rng, fq, 1.2, {2: -6, 3: -12, 4: -20}) * np.exp(-np.arange(n) / FS * 3).astype(np.float32)
                place(buf, y, t, g)
                t += rng.uniform(0.35, 0.7)
            t += rng.uniform(3, 15)
    elif kind == "phone":
        t = t0
        a, b = [[440, 480], [400, 450], [1000, 1200], [2000, 2400]][rng.integers(4)]
        while t < t0 + dur:
            d = rng.uniform(0.8, 2.0)
            y = tone(rng, a, d) + tone(rng, b, d)
            if rng.random() < 0.5:
                y *= (np.sin(2 * np.pi * 20 * np.arange(len(y)) / FS) > 0)
            place(buf, y.astype(np.float32), t, g)
            t += d + rng.uniform(0.2, 4)
    elif kind == "birds":
        t = t0
        while t < t0 + dur:
            n = int(rng.uniform(0.04, 0.25) * FS)
            fr = np.linspace(rng.uniform(2000, 6000), rng.uniform(2000, 6000), n)
            place(buf, (np.sin(2 * np.pi * np.cumsum(fr) / FS) * env(n, 0.005, 0.02)).astype(np.float32), t, g)
            t += rng.uniform(0.05, 1.5)
    elif kind == "clink":
        t = t0
        while t < t0 + dur:
            n = int(0.5 * FS)
            y = sum(np.sin(2 * np.pi * rng.uniform(2000, 7500) * np.arange(n) / FS) * rng.uniform(0.3, 1) for _ in range(3))
            place(buf, (y * np.exp(-np.arange(n) / FS * rng.uniform(8, 30))).astype(np.float32), t, g)
            t += rng.uniform(0.1, 3)
    elif kind == "claps":
        t = t0
        while t < t0 + dur:
            n = int(0.05 * FS)
            place(buf, (rng.standard_normal(n) * np.exp(-np.arange(n) / FS * 80)).astype(np.float32), t, g)
            t += rng.uniform(0.1, 1.0)
    return kind


# ---------------- scenes ----------------
def scene(rng, bank, alarm=None, hard=False):
    """One 40 s stream. alarm: None, 't3' or 't4'. Returns audio, beeps of the true alarm, info."""
    n = int(STREAM_S * FS)
    bed = -rng.uniform(55, 88)                                   # room noise level, dBFS
    x = pink(rng, n) * db2a(bed)
    if rng.random() < 0.3:                                       # mains hum and fan
        t = np.arange(n) / FS
        x += (db2a(bed + rng.uniform(-5, 10)) * sum(np.sin(2 * np.pi * 60 * h * t) / h for h in (1, 2, 3))).astype(np.float32)
    ev = np.zeros(n, np.float32)
    names, levels = [], []
    for _ in range(int(rng.integers(2, 7) if hard else rng.integers(0, 5))):   # decoys
        t0 = rng.uniform(-5, STREAM_S - 1)
        du = rng.uniform(1, 30)
        dl = bed + rng.uniform(5, 50)                            # same draw order as before levels were kept
        names.append(decoy(rng, ev, bank, t0, du, dl, hard))
        levels.append(dl)
    beeps = []
    if alarm:
        t0 = rng.uniform(-10, STREAM_S - 8)
        t1 = min(STREAM_S, t0 + rng.uniform(10, 60))
        beeps = pattern_times(rng, alarm, t0, t1)
        al = np.zeros(n, np.float32)
        render_beeps(al, beeps, alarm_sound(rng), 1.0)
        lev = min(bed + rng.uniform(0, 60), -6)
        ev += reverb(rng, al) * db2a(lev)
    x = eq(rng, x + ev)
    pk = np.abs(x).max()
    if pk > 0.9:                                                 # keep under full scale like the real chain
        x *= 0.9 / pk
    return x.astype(np.float32), beeps, dict(bed=bed, decoys=names, decoy_levels=levels, alarm=alarm,
                                             alarm_level=lev if alarm else None)


def window_labels(beeps, alarm, nframes, hop):
    """Label and weight for each window start (in frames, step hop). Returns arrays (starts, y, w)."""
    starts = np.arange(0, nframes - F.T + 1, hop)
    fdur = F.FRAME / FS
    y = np.zeros(len(starts), np.int64)
    w = np.ones(len(starts), np.float32)
    if not beeps:
        return starts, y, w
    b = np.array([(s, e, g) for s, e, g in beeps])
    for i, st in enumerate(starts):
        ws, we = st * fdur, (st + F.T) * fdur
        inside = (b[:, 0] >= ws) & (b[:, 1] <= we)
        n_in = int(inside.sum())
        groups = {}
        for s, e, g in b:
            groups.setdefault(int(g), []).append((s, e))
        pos = False
        if alarm == "t3":
            for g, bb in groups.items():
                full = len(bb) == 3 and all(s >= ws and e <= we for s, e in bb)
                if full:
                    nb = [x for gg in (g - 1, g + 1) for x in groups.get(gg, []) if x[0] >= ws and x[1] <= we]
                    if nb:
                        pos = True
                        break
        else:
            for g, bb in groups.items():
                if len(bb) == 4 and all(s >= ws and e <= we for s, e in bb):
                    gs, ge = bb[0][0], bb[-1][1]
                    after = min(we, groups[g + 1][0][0] if g + 1 in groups else we) - ge
                    before = gs - max(ws, groups[g - 1][-1][1] if g - 1 in groups else ws)
                    if after >= 3.0 or before >= 3.0:
                        pos = True
                        break
        if pos:
            y[i] = 1 if alarm == "t3" else 2
        elif n_in >= 2:
            w[i] = 0.0
    return starts, y, w


def make_set(bank_dir, split, nstreams, seed, hop=8, hard=False):
    """Frame features and window labels for nstreams scenes. Returns dict of arrays."""
    rng = np.random.default_rng(seed)
    bank = Bank(bank_dir, split)
    feats, Y, W, S, sid = [], [], [], [], []
    for k in range(nstreams):
        r = rng.random()
        alarm = "t3" if r < 0.4 else ("t4" if r < 0.7 else None)
        if hard:
            alarm = "t3" if r < 0.25 else ("t4" if r < 0.5 else None)
        x, beeps, _ = scene(rng, bank, alarm, hard)
        fr = F.frame_features(x)
        st, y, w = window_labels(beeps, alarm, len(fr), hop)
        feats.append(fr)
        Y.append(y)
        W.append(w)
        S.append(st)
        sid.append(np.full(len(st), k))
    return dict(frames=np.stack(feats), y=np.concatenate(Y), w=np.concatenate(W), start=np.concatenate(S),
                stream=np.concatenate(sid))
