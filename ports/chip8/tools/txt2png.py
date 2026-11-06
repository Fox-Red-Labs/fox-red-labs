#!/usr/bin/env python3
"""Turn the host runner's text screen (64 x 32 of '#' and ' ') into a PNG: txt2png.py in.txt out.png [scale]."""
import sys
from PIL import Image
rows = [l.rstrip("\n").split("|")[0] for l in open(sys.argv[1]) if "|" in l][:32]
s = int(sys.argv[3]) if len(sys.argv) > 3 else 8
im = Image.new("RGB", (64 * s, 32 * s), (0, 0, 0))
for y, r in enumerate(rows):
    for x, ch in enumerate(r[:64]):
        if ch == "#": im.paste((255, 255, 255), (x * s, y * s, (x + 1) * s, (y + 1) * s))
im.save(sys.argv[2])
