#!/usr/bin/env python3
"""Converter check: pixel order and header. Needs ffmpeg. Run: python3 tests/test_converter.py"""
import os, subprocess, sys, tempfile
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "player"))
import video_to_frames as v

d = tempfile.mkdtemp()
clip = os.path.join(d, "dots.mp4")
# white pixel at (0,0), at (9,3) and at (127,127) on black, lossless so thresholds stay exact
subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "lavfi", "-i", "color=c=black:s=128x128:r=5:d=1",
                "-vf", "drawbox=x=0:y=0:w=1:h=1:color=white:t=fill,drawbox=x=9:y=3:w=1:h=1:color=white:t=fill,"
                       "drawbox=x=127:y=127:w=1:h=1:color=white:t=fill,format=rgb24",
                "-c:v", "libx264rgb", "-crf", "0", os.path.join(d, "dots.mkv")], check=True)
out = os.path.join(d, "V.BIN")
n, data = v.convert(os.path.join(d, "dots.mkv"), out, 5, "stretch", "none", False, 128, 1)
fr = data[:v.FRAME]
fails = []
def px(x, y): return (fr[y * 16 + (x >> 3)] >> (x & 7)) & 1
for (x, y, want) in [(0, 0, 1), (9, 3, 1), (127, 127, 1), (1, 0, 0), (8, 3, 0), (126, 127, 0), (64, 64, 0)]:
    if px(x, y) != want: fails.append((x, y, px(x, y), want))
raw = open(out, "rb").read()
if raw[:8] != b"SH128V1\0" or raw[8] != 5 or raw[12] != n or raw[16] != 128 or len(raw) != 512 + n * v.FRAME:
    fails.append("header")
print("PASS" if not fails else f"FAIL {fails}")
sys.exit(1 if fails else 0)
