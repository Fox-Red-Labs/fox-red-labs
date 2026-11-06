#!/usr/bin/env python3
"""Convert binary PPM (P6) files to PNG with the standard library only.
Usage: python3 -I ppm2png.py <in.ppm> [<in2.ppm> ...]   (writes beside each input; --scale N enlarges by an integer factor)"""
import sys, zlib, struct

def read_ppm(path):
    d = open(path, 'rb').read()
    parts = d.split(None, 4)
    assert parts[0] == b'P6'
    w, h = int(parts[1]), int(parts[2])
    return w, h, d[len(d) - w * h * 3:]

def write_png(path, w, h, rgb, scale=1):
    rows = []
    for y in range(h):
        row = rgb[y * w * 3:(y + 1) * w * 3]
        if scale > 1:
            row = b''.join(row[i:i + 3] * scale for i in range(0, len(row), 3))
        for _ in range(scale):
            rows.append(b'\0' + row)
    raw = b''.join(rows)
    def chunk(t, b):
        c = struct.pack('>I', len(b)) + t + b
        return c + struct.pack('>I', zlib.crc32(t + b) & 0xffffffff)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w * scale, h * scale, 8, 2, 0, 0, 0)) \
        + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')
    open(path, 'wb').write(png)

if __name__ == '__main__':
    args = sys.argv[1:]
    scale = 1
    if args and args[0] == '--scale':
        scale = int(args[1]); args = args[2:]
    for p in args:
        w, h, rgb = read_ppm(p)
        write_png(p.rsplit('.', 1)[0] + '.png', w, h, rgb, scale)
