#!/usr/bin/env python3
"""Parse the IWRL6432 demo UART stream (magic word, 40-byte header, TLVs).
Usage: tlv_parse.py capture.bin   (or: tlv_parse.py --selftest)
Header: magic 8, version, totalPacketLen, platform, frameNumber, timeCpuCycles,
numDetectedObj, numTLVs, subFrameNumber (all uint32 little endian)."""
import struct, sys
MAGIC = bytes([2, 1, 4, 3, 6, 5, 8, 7])
HDR = struct.Struct("<8s8I")

def frames(buf):
    i = 0
    while True:
        i = buf.find(MAGIC, i)
        if i < 0 or i + HDR.size > len(buf):
            return
        h = HDR.unpack_from(buf, i)
        plen = h[2]
        if plen < HDR.size or plen > 65536 or i + plen > len(buf):
            i += 8
            continue
        hd = dict(version=h[1], length=plen, platform=h[3], frame=h[4],
                  cycles=h[5], objs=h[6], ntlv=h[7], subframe=h[8])
        tlvs, p, end = [], i + HDR.size, i + plen
        for _ in range(hd["ntlv"]):
            if p + 8 > end:
                break
            t, n = struct.unpack_from("<II", buf, p)
            tlvs.append((t, n, buf[p + 8:p + 8 + n]))
            p += 8 + n
        yield hd, tlvs
        i += plen

# TLV names from Radar Toolbox 4.00.00.05, Motion_and_Presence_Detection/src/source/motion_detect.h
# (enum starts at 300 = START; 301 = detected points ... 320 = point cloud antenna symbols).
NAMES = {1: "points", 2: "range profile", 3: "noise profile", 6: "stats", 7: "points side info",
         301: "ext points", 302: "range profile major", 303: "range profile minor",
         306: "ext stats", 307: "presence info", 308: "target list", 309: "target index",
         314: "point cloud indices", 315: "enhanced presence indication", 316: "adc samples",
         317: "classifier info", 318: "rx chan compensation", 319: "quick eval info",
         320: "point cloud antenna symbols"}

def decode_presence(b):
    """315: byte 0 then ceil(zones/4) bytes. In the shipped stream: 01 00 = byte0 1, zone byte 0."""
    return {"byte0": b[0], "zone_bytes": list(b[1:])}

def decode_quick_eval(b):
    """319: mpdProc_SceneryParams (sensorPosition 3 floats, sensorOrientation 2 floats,
    numBoundaryBoxes, numBoundaryArcs, numVirtualZones, pad, boundaryBox[] as 6 floats
    x1 x2 y1 y2 z1 z2 ...) then gIsDefCfgUsed (last byte). Only the first box is decoded.
    Layout read from mpdproc.h; the box field order is inferred from the values."""
    pos = struct.unpack_from("<3f", b, 0)
    ori = struct.unpack_from("<2f", b, 12)
    nbox, narc, nzone = b[20], b[21], b[22]
    box = struct.unpack_from("<6f", b, 24) if nbox else None
    return {"sensor_pos": pos, "orientation": ori, "boxes": nbox, "arcs": narc,
            "zones": nzone, "box0": box, "default_cfg_used": b[-1]}

DECODERS = {315: decode_presence, 319: decode_quick_eval}

def selftest():
    body = struct.pack("<II", 1020, 4) + b"\x01\x02\x03\x04"
    pkt = MAGIC + struct.pack("<8I", 0x05030300, 40 + len(body), 0xA6432, 7, 0, 0, 1, 0) + body
    fs = list(frames(b"junk" + pkt + pkt))
    assert len(fs) == 2 and fs[0][1][0][0] == 1020 and fs[0][0]["frame"] == 7
    print("selftest ok")

if __name__ == "__main__":
    if sys.argv[1] == "--selftest":
        selftest(); sys.exit()
    buf = open(sys.argv[1], "rb").read()
    fs = list(frames(buf))
    print(len(fs), "frames,", len(buf), "bytes")
    good = [f for f in fs if f[1] and all(t in NAMES for t, _, _ in f[1])]
    if good:
        for t, n, b in good[0][1]:
            print("TLV", t, NAMES.get(t, "?"), n, "bytes", DECODERS[t](b) if t in DECODERS else "")
    for hd, tl in fs:
        print(hd["frame"], hd["length"], hd["objs"], hd["cycles"], [(t, n) for t, n, _ in tl])
