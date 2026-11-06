"""Feature extraction for the alarm listener. This file is the specification: features.c on the board
computes the same numbers (checked on recorded audio with check_features.py).

Audio: 15,873 samples a second, values scaled so the board's 18-bit full scale is 1.0.
Frame: two back-to-back 512-sample FFTs (Hann window, no overlap), power averaged: one frame every
1,024 samples (64.5 ms). Per frame, in dB (10 log10 of power):
  0..8  energy in nine bands (edges BAND_HZ)
  9     total energy 150 to 7,000 Hz
  10    peak bin in 400 to 6,000 Hz: frequency in kHz / 4
  11    peak-to-average ratio in 400 to 6,000 Hz (crest), dB / 10
  (12   peak bin level in dB: not a model input; the loudness gate uses it)
A model window is 128 frames (8.26 s). Before the model, the window is normalized (normalize()):
channels 0 to 9 become (dB - loudest frame's total dB) / 10, clipped to [-6, 0], so the model sees the
pattern, not the loudness. Loudness is judged separately against what was learned from your alarm.
Four more channels lock onto the window's dominant tone (the peak frequency of its loudest tonal frame,
a tonal frame being one with crest 12 dB or more), so other sounds cannot blur that tone's cadence:
  12  that tone's level in each frame (dB rel. the loudest frame / 10, -6 when the frame's peak is not it)
  13  each frame's peak level (dB rel. the loudest frame / 10)
  14  1 when the frame is tonal and its peak is within 3 bins of the dominant tone, else 0
  15  the dominant tone in kHz / 4 (the same for every frame; 0 when no frame is tonal)
Frames are first rounded the way the board stores them (dB in 1/64 steps, channels 10 and 11 in 1/4096).
"""
import numpy as np

FS = 32e6 / 7 / 96 / 3          # 15,873.0 Hz
NFFT = 512
FRAME = 2 * NFFT                 # samples per feature frame
T = 128                          # frames per model window
DF = FS / NFFT                   # 31.0 Hz per bin
BAND_HZ = [150, 500, 800, 1300, 2000, 2700, 3100, 3500, 4200, 7000]
PEAK_HZ = (400, 6000)
C = 16                           # model input channels
TONAL = 1.2                      # crest channel value (12 dB) that makes a frame tonal
TONE_BINS = 3
REF_FLOOR_DB = -80.0             # a silent window is not stretched up to full scale
EPS = 1e-13


def bin_of(hz):
    return int(np.ceil(hz / DF))


BAND_BINS = [bin_of(h) for h in BAND_HZ]          # band b covers bins [BAND_BINS[b], BAND_BINS[b+1])
PEAK_BINS = (bin_of(PEAK_HZ[0]), bin_of(PEAK_HZ[1]))
WIN = (0.5 - 0.5 * np.cos(2 * np.pi * np.arange(NFFT) / NFFT)).astype(np.float64)


def frame_power(x):
    """x: float audio (full scale 1.0), length a multiple of FRAME. Returns (frames, 257) mean power."""
    n = len(x) // FRAME
    blocks = x[:n * FRAME].reshape(n * 2, NFFT) * WIN
    P = np.abs(np.fft.rfft(blocks, axis=1)) ** 2
    return P.reshape(n, 2, -1).mean(1)


def frame_features(x):
    """Raw per-frame features, (frames, 13): 9 band dB, total dB, peak kHz/4, crest dB/10, peak dB."""
    P = frame_power(x)
    out = np.empty((len(P), 13), np.float32)
    for b in range(9):
        out[:, b] = 10 * np.log10(P[:, BAND_BINS[b]:BAND_BINS[b + 1]].sum(1) + EPS)
    out[:, 9] = 10 * np.log10(P[:, BAND_BINS[0]:BAND_BINS[9]].sum(1) + EPS)
    seg = P[:, PEAK_BINS[0]:PEAK_BINS[1]]
    k = seg.argmax(1)
    pk = seg[np.arange(len(seg)), k]
    out[:, 10] = (k + PEAK_BINS[0]) * DF / 4000.0
    out[:, 11] = np.clip(10 * np.log10((pk + EPS) / (seg.mean(1) + EPS)) / 10, 0, 4)
    out[:, 12] = 10 * np.log10(pk + EPS)
    return out


def board_round(win):
    """The board keeps frames as int16: dB x 64, channels 10 and 11 x 4096, truncated toward zero."""
    w = np.array(win, np.float32, copy=True)
    for c in list(range(10)) + [12]:
        w[..., c] = np.trunc(w[..., c] * 64) / 64
    for c in (10, 11):
        w[..., c] = np.trunc(w[..., c] * 4096) / 4096
    return w


def normalize(win):
    """win: (..., T, 13) raw frames -> (..., C, T) model input."""
    win = board_round(win)
    ref = np.maximum(win[..., 9].max(-1, keepdims=True), REF_FLOOR_DB)
    z = np.empty(win.shape[:-1] + (C,), np.float32)
    z[..., :10] = np.clip((win[..., :10] - ref[..., None]) / 10, -6, 0)
    z[..., 10] = win[..., 10]
    z[..., 11] = win[..., 11]
    tonal = win[..., 11] >= TONAL
    pk = np.where(tonal, win[..., 12], -1e9)
    i = pk.argmax(-1)
    fdom = np.take_along_axis(win[..., 10], i[..., None], -1)
    has = tonal.any(-1, keepdims=True)
    fdom = np.where(has, fdom, 0.0)
    intone = tonal & has & (np.abs(win[..., 10] - fdom) * 4000 <= TONE_BINS * DF + 1e-3)
    lev = np.clip((win[..., 12] - ref) / 10, -6, 0)
    z[..., 12] = np.where(intone, lev, -6)
    z[..., 13] = lev
    z[..., 14] = intone
    z[..., 15] = np.broadcast_to(fdom, win.shape[:-1])
    return np.swapaxes(z, -1, -2)


# ---- cadence check: the dominant tone's on/off timing against the standard (second stage) ----
# Frame counts at 64.5 ms. Temporal-3 nominal: beep 7.75 frames, short gap 7.75, long gap 23.3.
# Temporal-4 nominal: a 0.7 s group of pulses (10.9 frames) then 5 s quiet (77.5).
T3_BEEP = (6, 11)
T3_SHORT = (4, 10)
T3_LONG = (18, 27)
T4_GROUP = (9, 16)       # one pulse group after merging pieces up to T4_MERGE frames apart
T4_MERGE = 3
T4_EDGE_MIN = 4          # a group cut off by the window's start counts if this long; spacing then by its end
ON_DB = 1.2              # a tone frame counts as on within 12 dB of the window's loudest tone frame (drops reverb tails)
T4_QUIET = 46            # (no longer used: one group then quiet also matches a beep burst that simply stops)
T4_PERIOD = (76, 103)    # start of one pulse group to the next: 5.8 s +-15 %, with only quiet between


def tone_runs(intone):
    """Runs of tone frames, bridging single-frame dropouts: list of (start, length)."""
    s = list(np.asarray(intone) > 0.5)
    for t in range(1, len(s) - 1):
        if not s[t] and s[t - 1] and s[t + 1]:
            s[t] = True
    runs, t = [], 0
    while t < len(s):
        if s[t]:
            u = t
            while u < len(s) and s[u]:
                u += 1
            runs.append((t, u - t))
            t = u
        else:
            t += 1
    return runs


def tone_on(z):
    on = z[14] > 0.5
    if not on.any():
        return on
    return on & (z[12] >= np.float32(z[12][on].max() - np.float32(ON_DB)))


def cadence_ok(z, cls):
    """z: one model input (C, T). cls 1 (smoke) or 2 (co). True when the tone's timing fits the pattern."""
    runs = tone_runs(tone_on(z))
    inside = lambda v, r: r[0] <= v <= r[1]
    if cls == 1:
        # full or partial edge runs are skipped; look for beep, gap, beep, gap, beep, long gap, beep (or mirrored)
        full = [(s, n) for s, n in runs if s > 0 and s + n < len(z[14])]
        for i in range(len(full) - 3):
            seq = full[i:i + 4]
            if not all(inside(n, T3_BEEP) for _, n in seq):
                continue
            gaps = [seq[j + 1][0] - (seq[j][0] + seq[j][1]) for j in range(3)]
            sh = [inside(g, T3_SHORT) for g in gaps]
            lg = [inside(g, T3_LONG) for g in gaps]
            if (sh[0] and sh[1] and lg[2]) or (lg[0] and sh[1] and sh[2]):
                return True
        return False
    T = len(z[14])
    g = []
    for s0, n0 in runs:                                 # merge the pieces of one pulse group
        if g and s0 - (g[-1][0] + g[-1][1]) <= T4_MERGE:
            g[-1] = (g[-1][0], s0 + n0 - g[-1][0])
        else:
            g.append((s0, n0))
    for i in range(len(g) - 1):
        (s0, n0), (s1, n1) = g[i], g[i + 1]
        if s1 + n1 >= T or not inside(n1, T4_GROUP):
            continue
        if s0 == 0:
            if n0 >= T4_EDGE_MIN and inside((s1 + n1) - n0, T4_PERIOD):
                return True
        elif inside(n0, T4_GROUP) and inside(s1 - s0, T4_PERIOD):
            return True
    return False
