#!/usr/bin/env python3
"""Check the board's feature frames against host/features.py on the same audio.

Needs the detect image built with DETECT_FLAGS="-DSTREAM_AUDIO -DDUMP_FEATURES": it sends the audio
(A5 5B packets with the sample index) and one "F <start> <13 values>" line per frame.

  check_features.py SECONDS [--port ...] [--play FILE] [--wav OUT.wav]

Prints, per channel, the median and largest difference board minus Python, over frames whose audio
arrived complete. Band levels within about 0.5 dB, peak frequency equal, is a pass.
"""
import argparse
import os
import subprocess
import sys
import time

import numpy as np
import serial

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import features as F  # noqa: E402
import rec  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("seconds", type=float)
    ap.add_argument("--port", default="/dev/cu.usbmodemMG0200011")
    ap.add_argument("--play")
    ap.add_argument("--wav")
    a = ap.parse_args()
    s = serial.Serial(a.port, 921600, timeout=0.2)
    s.reset_input_buffer()
    p = subprocess.Popen(["afplay", a.play]) if a.play else None
    buf = bytearray()
    audio = {}
    frames = []
    t_end = time.time() + a.seconds
    bad = 0
    while time.time() < t_end:
        buf += s.read(16384)
        i = 0
        while len(buf) - i >= 8:
            if buf[i] == 0xA5 and buf[i + 1] == 0x5B:
                n = buf[i + 6]
                L = 7 + 3 * n + 1
                if n > 32:
                    i += 1
                    continue
                if len(buf) - i < L:
                    break
                pay = bytes(buf[i + 7:i + 7 + 3 * n])
                if (sum(pay) & 0xFF) != buf[i + L - 1]:
                    bad += 1
                    i += 1
                    continue
                idx = int.from_bytes(buf[i + 2:i + 6], "little")
                v = np.frombuffer(pay, np.uint8).reshape(n, 3).astype(np.int32)
                v = v[:, 0] | (v[:, 1] << 8) | (v[:, 2] << 16)
                audio[idx] = np.where(v & 0x800000, v - 0x1000000, v)
                i += L
            elif buf[i] == ord("F") and buf[i + 1] == ord(" "):
                j = buf.find(b"\n", i)
                if j < 0:
                    break
                parts = buf[i:j].decode(errors="replace").split()
                if len(parts) == 15:
                    frames.append((int(parts[1]), np.array([float(x) for x in parts[2:]])))
                i = j + 1
            else:
                i += 1
        del buf[:i]
    if p:
        p.terminate()
    keys = sorted(audio)
    base = keys[0]
    total = keys[-1] + 16 - base
    x = np.zeros(total, np.int32)
    have = np.zeros(total, bool)
    for k in keys:
        x[k - base:k - base + 16] = audio[k]
        have[k - base:k - base + 16] = True
    if a.wav:
        rec.write_wav(a.wav, x)
    diffs = []
    for st, fb in frames:
        o = st - base
        if o < 0 or o + F.FRAME > total or not have[o:o + F.FRAME].all():
            continue
        fp = F.frame_features(x[o:o + F.FRAME].astype(np.float64) / 2 ** 17)[0]
        diffs.append(fb - fp)
    d = np.array(diffs)
    names = [f"band{b}" for b in range(9)] + ["total", "peak_kHz/4", "crest/10", "peak_dB"]
    print(f"{len(d)} frames compared of {len(frames)} sent; audio {total/F.FS:.1f} s, bad packets {bad}")
    for c, nm in enumerate(names):
        print(f"  {nm:11s} median {np.median(d[:, c]):+.3f}  max |diff| {np.abs(d[:, c]).max():.3f}")


if __name__ == "__main__":
    main()
