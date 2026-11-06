#!/usr/bin/env python3
"""Map every 128-byte unit of a WHX file to an access class, for the cache simulator.
  python3 -I whx_classes.py <DOOM1.whx> <out.bin> prints the class names with their byte totals.
Classes follow the lump order of the shareware DOOM1 conversion (lump numbers checked against the named-lump table)."""
import struct, sys
d = open(sys.argv[1], 'rb').read()
_, n, ito = struct.unpack_from('<4sii', d, 0)
offs = struct.unpack_from('<%dI' % (n + 1), d, ito)
def st(i): return offs[i] & 0xffffff
def en(i): return offs[i + 1] & 0xffffff
LV = ['things', 'linedefs', 'sidedefs', 'vertexes', 'segs', 'ssectors', 'nodes', 'sectors', 'reject', 'blockmap']
def cls_of_lump(i):
    if i == 0: return 'playpal'
    if i == 1: return 'colormap'
    if 6 <= i < 105:
        k = (i - 6) % 11
        return 'lvl-' + LV[k - 1] if k else 'lvl-marker'
    if i in (105, 106): return 'textures'
    if 2 <= i < 6: return 'demo-endoom'
    if 107 <= i < 232: return 'audio'
    if 232 <= i < 552: return 'pics'
    if i in (552, 1036): return 'sprite-meta'
    if 553 <= i < 1036: return 'sprites'
    if i == 1037: return 'vpatch-numbers'
    if 1038 <= i < 1205: return 'vpatches'
    if i in (1206, 1205): return 'flat-meta'
    if 1207 <= i < 1262: return 'flats'
    return 'other'
names = ['dir']; idx = {'dir': 0}
units = (len(d) + 127) // 128
arr = bytearray(units)
def cid(c):
    if c not in idx: idx[c] = len(names); names.append(c)
    return idx[c]
tot = {}
for i in range(n):
    c = cid(cls_of_lump(i))
    for u in range(st(i) // 128, (en(i) + 127) // 128):
        if u < units: arr[u] = c
    tot[c] = tot.get(c, 0) + en(i) - st(i)
tot[0] = st(0)
open(sys.argv[2], 'wb').write(bytes(arr))
open(sys.argv[2] + '.names', 'w').write('\n'.join(names))
for c, t in sorted(tot.items(), key=lambda x: -x[1]): print('%-15s %8d B' % (names[c], t))
