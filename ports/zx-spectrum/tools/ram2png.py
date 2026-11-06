#!/usr/bin/env python3
"""Render a 6,912-byte Spectrum screen dump (bitmap then attributes) to a PNG: ram2png.py ram.bin out.png [scale]."""
import sys
from PIL import Image
d = open(sys.argv[1], "rb").read(); s = int(sys.argv[3]) if len(sys.argv) > 3 else 3
pal = lambda i: tuple((255 if i & 8 else 205) * ((i >> k) & 1) for k in (1, 2, 0))
im = Image.new("RGB", (256, 192))
for y in range(192):
    off = ((y & 0xC0) << 5) | ((y & 7) << 8) | (((y >> 3) & 7) << 5)
    for cx in range(32):
        a = d[0x1800 + (y // 8) * 32 + cx]; br = 8 if a & 0x40 else 0
        ink, paper = pal((a & 7) | br), pal(((a >> 3) & 7) | br)
        for k in range(8): im.putpixel((cx * 8 + k, y), ink if d[off + cx] & (0x80 >> k) else paper)
im.resize((256 * s, 192 * s), Image.NEAREST).save(sys.argv[2])
