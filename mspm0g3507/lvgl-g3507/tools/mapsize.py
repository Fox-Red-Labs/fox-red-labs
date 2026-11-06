#!/usr/bin/env python3
"""Split a TI linker map into LVGL, port, app, libc/other, by output section (flash vs RAM).
Usage: mapsize.py out/<cfg>/<cfg>.map [-v N]   (-v lists the N biggest objects)"""
import re, sys, collections
path = sys.argv[1]; top = int(sys.argv[sys.argv.index('-v') + 1]) if '-v' in sys.argv else 0
txt = open(path).read().split('SECTION ALLOCATION MAP')[1].split('SEGMENT')[0].split('GLOBAL SYMBOLS')[0]
FLASH = {'.intvecs', '.text', '.rodata', '.cinit', '.const', '.init_array', '.ARM.exidx'}
RAM = {'.bss', '.data', '.stack', '.sysmem', '.vtable', '.args'}
def group(o):
    if o.startswith('lvgl_'): return 'LVGL'
    if o in ('port.o',): return 'port'
    if o in ('app.o', 'main.o'): return 'app'
    if o in ('cfg.o', 'st.o'): return 'SysConfig + startup'
    return 'libc / compiler runtime'
sec = None; g = collections.defaultdict(lambda: collections.Counter()); per = collections.Counter()
for line in txt.splitlines():
    m = re.match(r'^(\.\w+)\s+\d+\s+[0-9a-f]+\s+([0-9a-f]+)', line)
    if m:
        sec = m.group(1)
        if sec == '.stack': g['stack (linker)']['RAM'] += int(m.group(2), 16)
        continue
    m = re.match(r'^\s+[0-9a-f]{8}\s+([0-9a-f]{8})\s+(\S+)\s+\((\S+)\)', line)
    if not m or sec is None: continue
    n = int(m.group(1), 16); o = m.group(2)
    kind = 'flash' if sec in FLASH else ('RAM' if sec in RAM else None)
    if kind: g[group(o)][kind] += n; per[(kind, o)] += n
print(f"{'':28}{'flash':>9}{'RAM':>9}")
tf = tr = 0
for k in ['LVGL', 'port', 'app', 'SysConfig + startup', 'libc / compiler runtime', 'stack (linker)']:
    f = g[k]['flash']; r = g[k]['RAM']; tf += f; tr += r
    print(f"{k:28}{f:9}{r:9}")
print(f"{'total':28}{tf:9}{tr:9}")
for (k, o), n in per.most_common(top): print(k, o, n)
