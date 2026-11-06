#!/usr/bin/env python3
"""Print flash and RAM use by region from a TI linker map. Usage: sizes.py doom.map"""
import sys, re
rows = []
inmem = False
for l in open(sys.argv[1]):
    if l.startswith('MEMORY CONFIGURATION'): inmem = True; continue
    if inmem and l.startswith('SEGMENT ALLOCATION MAP'): break
    p = l.split()
    if inmem and len(p) >= 5 and re.fullmatch(r'[0-9a-f]{8}', p[1] if len(p) > 1 else ''):
        rows.append(p)
print("%-26s %-9s %9s %9s %9s" % ('region', 'origin', 'length', 'used', 'unused'))
for p in rows:
    if p[0] in ('FLASH', 'FLASH_INT_VEC', 'DRAM', 'TCM_DRAM_NON_SECURE', 'CRAM'):
        print("%-26s %-9s %9d %9d %9d" % (p[0], p[1], int(p[2], 16), int(p[3], 16), int(p[4], 16)) if len(p) > 4 else p)
