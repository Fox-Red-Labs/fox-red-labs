#!/usr/bin/env python3
"""LUMPCACHE=1 build: the engine's lump_data() (w_wad.h) asks fox_lump_data() in platform_core.c, which keeps lumps in RAM.
Usage: lumpcache_patch.py <path to w_wad.h>   (edits that copy, never the pinned source)"""
import sys
p = sys.argv[1]
s = open(p).read()
old = '    return whd_map_base + ((*lump)&0xffffffu);\n'
assert old in s
s = s.replace(old, '    return fox_lump_data(lump);\n', 1)
decl = 'extern const uint8_t *whd_map_base;\n'
assert decl in s
s = s.replace(decl, decl + 'const uint8_t *fox_lump_data(const lumpinfo_t *lump);\n', 1)
open(p, 'w').write(s)
