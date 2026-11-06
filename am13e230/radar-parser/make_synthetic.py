#!/usr/bin/env python3
"""Build a SYNTHETIC IWRL6432 frame set from the documented format (magic word, 40-byte header, TLVs 1, 7, 315, one unknown
type), plus line noise between frames, a frame with no TLVs and a false header (length 10) that the parser must skip.
This is not radar data. Writes synthetic.bin and synthetic.json (what the generator put in each frame).
   make_synthetic.py [outdir]"""
import json, math, os, random, struct, sys
import numpy as np
MAGIC = bytes([2, 1, 4, 3, 6, 5, 8, 7])
out = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.abspath(__file__))
rng = random.Random(6432)

blob = b""; truth = []
for num in range(1, 41):
    npts = 0 if num % 7 == 0 else rng.randint(1, 12)
    pts = []
    for _ in range(npts):
        r = rng.uniform(0.4, 6.0); az = rng.uniform(-1.0, 1.0); el = rng.uniform(-0.2, 0.2)
        pts.append((r * math.cos(el) * math.sin(az), r * math.cos(el) * math.cos(az), r * math.sin(el), rng.uniform(-2, 2)))
    tl = []
    if npts:
        tl.append((1, b"".join(struct.pack("<4f", *p) for p in pts)))
        tl.append((7, b"".join(struct.pack("<2H", rng.randint(5, 60), rng.randint(1, 9)) for _ in pts)))
    tl.append((315, bytes([1 if npts else 0, rng.randint(0, 3)])))
    if num % 11 == 0: tl.append((999, b"\x01\x02\x03\x04\x05"))     # a type the parser does not know
    if num == 20: tl = []; pts = []                                # a frame with no TLVs: its header still says how many objects
    body = b"".join(struct.pack("<II", t, len(p)) + p for t, p in tl)
    objs = npts
    # header: magic, version, length, platform, frame, cycles, objs, ntlv, subframe
    h = MAGIC + struct.pack("<8I", 0x05030300, 40 + len(body), 0xA6432, num, 1000 * num, objs, len(tl), 0)
    blob += bytes(rng.randrange(256) & 0x7F | 0x10 for _ in range(rng.randint(0, 5)))   # noise: never the magic's first byte (2)
    if num == 25: blob += MAGIC + struct.pack("<8I", 0x05030300, 10, 0, 0, 0, 0, 0, 0)  # false header, skipped
    blob += h + body
    # float32 truth, as the board computes it
    rm = -1
    if pts:
        a = np.array(pts, dtype=np.float32)
        r2 = a[:, 0] * a[:, 0] + a[:, 1] * a[:, 1] + a[:, 2] * a[:, 2]
        rm = int(np.sqrt(r2.min()) * np.float32(1000) + np.float32(0.5))
    truth.append({"frame": num, "objs": objs, "points": len(pts), "nearest_mm": rm})
open(os.path.join(out, "synthetic.bin"), "wb").write(blob)
json.dump(truth, open(os.path.join(out, "synthetic.json"), "w"), indent=1)
print("synthetic.bin", len(blob), "bytes,", len(truth), "frames")
