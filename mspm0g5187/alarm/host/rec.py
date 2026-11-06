#!/usr/bin/env python3
"""Record the LP-MSPM0G5187's onboard microphone (alarm_capture image) to a WAV file.

  rec.py OUT.wav SECONDS [--port /dev/cu.usbmodemMG0200011] [--play FILE.wav] [--say "text"]

The board sends 1 ms packets: A5 5A seq n, n samples as 3-byte little-endian signed 18-bit values,
then the low byte of the payload sum; text lines starting with '#' carry status. The WAV is
15,873 Hz mono 24-bit (the 18-bit value shifted up 6 bits). Dropped packets (sequence gaps) are
filled with zeros and counted. --play plays a file on the Mac's speaker while recording (decoys),
--say speaks text with macOS say. Prints level stats at the end.
"""
import argparse
import os
import subprocess
import sys
import time
import wave

import numpy as np
import serial

FS = 32e6 / 7 / 96 / 3


def read_stream(port, seconds, on_start=None):
    s = serial.Serial(port, 921600, timeout=0.2)
    s.reset_input_buffer()
    buf = bytearray()
    out, drops, bad, texts = [], 0, 0, []
    last = None
    t0 = time.time()
    t_end = None
    started = False
    while True:
        chunk = s.read(8192)
        if chunk:
            buf += chunk
        i = 0
        while True:
            if len(buf) - i < 4:
                break
            if buf[i] == 0xA5 and buf[i + 1] == 0x5A:
                n = buf[i + 3]
                L = 4 + 3 * n + 1
                if n > 32:
                    i += 1
                    continue
                if len(buf) - i < L:
                    break
                pay = bytes(buf[i + 4:i + 4 + 3 * n])
                if (sum(pay) & 0xFF) != buf[i + L - 1]:
                    bad += 1
                    i += 1
                    continue
                seq = buf[i + 2]
                if not started:
                    started = True
                    t_end = time.time() + seconds
                    if on_start:
                        on_start()
                if last is not None and seq != ((last + 1) & 0xFF):
                    gap = (seq - last - 1) & 0xFF
                    drops += gap
                    out.append(np.zeros(gap * n, np.int32))
                last = seq
                a = np.frombuffer(pay, np.uint8).reshape(n, 3).astype(np.int32)
                v = a[:, 0] | (a[:, 1] << 8) | (a[:, 2] << 16)
                v = np.where(v & 0x800000, v - 0x1000000, v)
                out.append(v)
                i += L
            elif buf[i] == ord("#"):
                j = buf.find(b"\n", i)
                if j < 0:
                    break
                texts.append(buf[i:j].decode(errors="replace").strip())
                i = j + 1
            else:
                i += 1
        del buf[:i]
        if started and time.time() >= t_end:
            break
        if not started and time.time() - t0 > 5:
            raise SystemExit(f"no audio packets in 5 s on {port} (is alarm_capture flashed?) text seen: {texts[-3:]}")
    s.close()
    x = np.concatenate(out) if out else np.zeros(0, np.int32)
    return x, drops, bad, texts


def write_wav(path, x18):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(3)
        w.setframerate(int(round(FS)))
        v = (x18.astype(np.int32) << 6)
        b = np.stack([v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF], 1).astype(np.uint8)
        w.writeframes(b.tobytes())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("seconds", type=float)
    ap.add_argument("--port", default="/dev/cu.usbmodemMG0200011")
    ap.add_argument("--play")
    ap.add_argument("--say")
    a = ap.parse_args()
    procs = []

    def start():
        if a.play:
            procs.append(subprocess.Popen(["afplay", a.play]))
        if a.say:
            procs.append(subprocess.Popen(["say", a.say]))

    x, drops, bad, texts = read_stream(a.port, a.seconds, start)
    for p in procs:
        p.terminate()
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    write_wav(a.out, x)
    fs = x.astype(np.float64) / 2 ** 17
    rms = np.sqrt(np.mean(fs ** 2)) if len(fs) else 0
    pk = np.max(np.abs(fs)) if len(fs) else 0
    print(f"{a.out}: {len(x)/FS:.2f} s, dropped packets {drops}, bad checksums {bad}, "
          f"rms {20*np.log10(rms+1e-12):.1f} dBFS, peak {20*np.log10(pk+1e-12):.1f} dBFS; board: {texts[-1] if texts else 'no status line'}")


if __name__ == "__main__":
    main()
