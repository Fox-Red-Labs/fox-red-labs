#!/usr/bin/env python3
"""Matter setup payload (QR text and manual code) for TI's test values. Usage: payload.py [passcode disc vid pid]"""
import sys
pc, disc, vid, pid = (int(x, 0) for x in (sys.argv[1:5] or ["20202021", "3840", "0xFFF1", "0x8001"]))
# QR: fields packed LSB first
fields = [(3, 0), (16, vid), (16, pid), (2, 0), (8, 6), (12, disc), (27, pc), (4, 0)]  # rendezvous 6 = BLE (bit 1) plus on network (bit 2), the value the board prints
v = 0; pos = 0
for bits, val in fields:
    v |= val << pos; pos += bits
data = v.to_bytes(11, "little")
A = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-."
out = ""
for i in range(0, 11, 3):
    chunk = data[i:i+3]; n = int.from_bytes(chunk, "little")
    for _ in range({3: 5, 2: 4, 1: 2}[len(chunk)]):
        out += A[n % 38]; n //= 38
qr = "MT:" + out
# Manual 11-digit code without VID/PID (Matter spec 5.1.4.1)
def verhoeff(s):
    d = [[0,1,2,3,4,5,6,7,8,9],[1,2,3,4,0,6,7,8,9,5],[2,3,4,0,1,7,8,9,5,6],[3,4,0,1,2,8,9,5,6,7],[4,0,1,2,3,9,5,6,7,8],[5,9,8,7,6,0,4,3,2,1],[6,5,9,8,7,1,0,4,3,2],[7,6,5,9,8,2,1,0,4,3],[8,7,6,5,9,3,2,1,0,4],[9,8,7,6,5,4,3,2,1,0]]
    p = [[0,1,2,3,4,5,6,7,8,9],[1,5,7,6,2,8,3,0,9,4],[5,8,0,3,7,9,6,1,4,2],[8,9,1,6,0,4,3,5,2,7],[9,4,5,3,1,2,6,8,7,0],[4,2,8,6,5,7,3,9,0,1],[2,7,9,3,8,0,6,4,1,5],[7,0,4,6,9,1,3,2,5,8]]
    inv = [0,4,3,2,1,5,6,7,8,9]
    c = 0
    for i, ch in enumerate(reversed(s)):
        c = d[c][p[(i + 1) % 8][int(ch)]]
    return str(inv[c])
body = "%1d%05d%04d" % ((disc >> 10) & 0x3, (((disc >> 8) & 0x3) << 14) | (pc & 0x3FFF), pc >> 14)
print(qr)
print(body + verhoeff(body))
