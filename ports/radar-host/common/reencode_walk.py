#!/usr/bin/env python3
"""Turn a recorded IWRL6432BOOST walk (iwrl6432boost/live/runs/*-walk-*.json) into the byte stream the
firmware sends with `guiMonitor 1 ...`: the classic frame (magic word, 40-byte header) with TLV 1
(x, y, z, velocity as float32) and TLV 7 (SNR, noise as int16 in 0.1 dB), padded to 32 bytes.

  reencode_walk.py <walk.json> <out.bin>
  reencode_walk.py --selftest

The header fields match a capture from the board on 2026-11-03 (version 0x05040001, platform 0xA6432,
subframe 0xFFFFFFFF; frame layout from motion_detect.c, MMWAVE-L-SDK 05.05.04.02). The points are the
recorded ones, rounded to 3 decimals by the recorder. Noise is not recorded and is written as 0.
"""
import json, struct, sys

MAGIC = bytes([2, 1, 4, 3, 6, 5, 8, 7])
VERSION, PLATFORM, SUBFRAME = 0x05040001, 0xA6432, 0xFFFFFFFF

def frame(n, pts):
    tlvs = b""
    if pts:
        tlvs += struct.pack("<II", 1, 16 * len(pts)) + b"".join(struct.pack("<4f", *p[:4]) for p in pts)
        tlvs += struct.pack("<II", 7, 4 * len(pts)) + b"".join(
            struct.pack("<hh", int(round((p[4] if len(p) > 4 else 0) * 10)), 0) for p in pts)
    total = 40 + len(tlvs)
    pad = (-total) % 32
    hdr = MAGIC + struct.pack("<8I", VERSION, total + pad, PLATFORM, n, 0, len(pts), 2 if pts else 0, SUBFRAME)
    return hdr + tlvs + b"\0" * pad

def encode(walk):
    return b"".join(frame(f.get("frame", i), f.get("points") or []) for i, f in enumerate(walk["frames"]))

def selftest():
    b = frame(5, [[1.0, 2.0, 0.5, -0.25, 12.3]])
    assert b[:8] == MAGIC and len(b) % 32 == 0
    v, tot, plat, n, _, nobj, ntlv, sub = struct.unpack("<8I", b[8:40])
    assert (tot, plat, n, nobj, ntlv) == (len(b), PLATFORM, 5, 1, 2)
    assert struct.unpack("<II4f", b[40:64]) == (1, 16, 1.0, 2.0, 0.5, -0.25)
    assert struct.unpack("<IIhh", b[64:76]) == (7, 4, 123, 0)
    print("selftest OK")

if __name__ == "__main__":
    if sys.argv[1:] == ["--selftest"]:
        selftest(); sys.exit(0)
    if len(sys.argv) != 3:
        print(__doc__.split("\n\n")[1]); sys.exit(2)
    data = encode(json.load(open(sys.argv[1])))
    open(sys.argv[2], "wb").write(data)
    print(f"{sys.argv[2]}: {len(data)} bytes")
