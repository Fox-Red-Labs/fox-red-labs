#!/usr/bin/env python3
"""Rename TI's prebuilt Project Zero (LP-CC1354P10-1) without its source.
The names live in the linker's compressed copy-table record 0. This unpacks it, edits both names,
stores the record uncompressed (literal LZSS) in blank flash at NEW_ADDR, and points the
copy table at it. Usage: rename_ble.py <flash backup .bin> <ccfg .bin> <out .hex> <adv name> <gatt name>"""
import struct, sys
import ti_cinit as t

TABLE, RUN0, NEW_ADDR = 0x34934, 0x20000650, 0x35000
GATT_SLOT, GATT_LEN = 0x20001500, 21          # attDeviceName[GAP_DEVICE_NAME_LEN]
ADV, ADV_LEN = 0x20001552, 17                 # 0x0D 0x09 <12 chars> 0x02 0x01 0x06

def ihex(chunks):
    def rec(a16, typ, data):
        b = bytes([len(data), a16 >> 8, a16 & 0xFF, typ]) + data
        return ":" + (b + bytes([(-sum(b)) & 0xFF])).hex().upper()
    lines, upper = [], None
    for base, data in chunks:
        for off in range(0, len(data), 16):
            a = base + off
            if a >> 16 != upper:
                upper = a >> 16; lines.append(rec(0, 4, upper.to_bytes(2, "big")))
            lines.append(rec(a & 0xFFFF, 0, data[off:off + 16]))
    return "\n".join(lines + [":00000001FF"]) + "\n"

img = bytearray(open(sys.argv[1], "rb").read()); ccfg = open(sys.argv[2], "rb").read()
adv_name, gatt_name = sys.argv[4].encode(), sys.argv[5].encode()
assert len(adv_name) <= 12 and len(gatt_name) <= GATT_LEN - 1
load0, run0 = struct.unpack_from("<II", img, TABLE)
assert run0 == RUN0 and img[load0] == 0, "copy table is not the one this was written for"
data, _ = t.decompress_lzss(img, load0 + 1); data = bytearray(data)
g = GATT_SLOT - RUN0; a = ADV - RUN0
assert data[g:g + 12] == b"Project Zero" and data[a:a + 2] == b"\x0d\x09" and data[a + 14:a + 17] == b"\x02\x01\x06"
data[g:g + GATT_LEN] = gatt_name.ljust(GATT_LEN, b"\0")
# advertising: name AD, then flags AD, then zero padding (an AD length of 0 ends the data)
adv = bytes([len(adv_name) + 1, 0x09]) + adv_name + b"\x02\x01\x06"
data[a:a + ADV_LEN] = adv.ljust(ADV_LEN, b"\0")
rec = bytes([0]) + t.encode_literal(bytes(data))      # handler 0 = __TI_decompress_lzss
assert all(b == 0xFF for b in img[NEW_ADDR:NEW_ADDR + len(rec)]), "target flash not blank"
img[NEW_ADDR:NEW_ADDR + len(rec)] = rec
struct.pack_into("<I", img, TABLE, NEW_ADDR)
end = (NEW_ADDR + len(rec) + 0xFFF) & ~0xFFF
open(sys.argv[3], "w").write(ihex([(0, bytes(img[:end])), (0x50000000, ccfg)]))
print(f"record moved to {NEW_ADDR:#x} ({len(rec)} bytes), image 0..{end:#x}, adv '{adv_name.decode()}', gatt '{gatt_name.decode()}'")
