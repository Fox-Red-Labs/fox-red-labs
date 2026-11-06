#!/usr/bin/env python3
"""Wrap a firmware binary in a Zigbee OTA upgrade file (Zigbee Cluster Library, OTA file format).
Stand-in for TI's zOTAfileGen, which ships only in the CC13xx/CC26xx SDK.
Usage: make_zigbee_ota.py <in.bin> <out.zigbee> <manuf hex> <image type hex> <file version hex>"""
import struct, sys
src, out, manuf, itype, ver = sys.argv[1], sys.argv[2], int(sys.argv[3], 16), int(sys.argv[4], 16), int(sys.argv[5], 16)
data = open(src, "rb").read()
HDR_LEN = 56
sub = struct.pack("<HI", 0x0000, len(data)) + data          # tag 0x0000 = upgrade image
name = b"Fox Red Labs TI F3 OTA test".ljust(32, b"\0")
total = HDR_LEN + len(sub)
hdr = struct.pack("<IHHHHHIH32sI", 0x0BEEF11E, 0x0100, HDR_LEN, 0x0000, manuf, itype, ver, 0x0002, name, total)
assert len(hdr) == HDR_LEN
open(out, "wb").write(hdr + sub)
print(f"{out}: {total} bytes, manufacturer 0x{manuf:04X}, image type 0x{itype:04X}, version 0x{ver:08X}")
