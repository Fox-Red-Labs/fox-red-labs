#!/usr/bin/env python3
"""Flash and RAM of one TI Arm Clang link, from its map file, split into FreeRTOS kernel, app,
TI device code and C runtime. Usage: mapsize.py <file.map>
Totals come from the map's MEMORY CONFIGURATION (what the linker placed, alignment included);
the split comes from its MODULE SUMMARY (code + read-only data = flash, read-write data = RAM)."""
import re
import sys

txt = open(sys.argv[1]).read()
mem = {m.group(1): (int(m.group(2), 16), int(m.group(3), 16))
       for m in re.finditer(r'^\s+(FLASH|SRAM)\s+\w+\s+(\w+)\s+(\w+)', txt, re.M)}
summ = txt.split('MODULE SUMMARY')[1].split('LINKER GENERATED')[0]

KERNEL = {'tasks.o', 'list.o', 'queue.o', 'port.o', 'portasm.o', 'heap_4.o'}
APP = {'blink.o', 'bench.o', 'edge.o', 'fr_common.o'}
groups = {}
lib = None
for line in summ.splitlines():
    s = line.strip()
    if s.startswith('/') or s == '<current directory>':
        lib = s
        continue
    m = re.match(r'^(\S[^:]*?):?\s+(\d+)\s+(\d+)\s+(\d+)\s*$', s)
    if not m or s.startswith(('Total', 'Grand', '+')):
        continue
    name, code, ro, rw = m.group(1), int(m.group(2)), int(m.group(3)), int(m.group(4))
    if name in KERNEL:
        g = 'FreeRTOS kernel and port'
    elif name in APP:
        g = 'App (this folder)'
    elif name == 'Stack':
        g = 'Interrupt and startup stack (.stack)'
    elif name == 'Heap':
        g = 'C library heap (.sysmem)'
    elif name == 'Linker Generated':
        g = 'Linker tables'
    elif lib and 'driverlib' in lib or name.startswith(('startup', 'ti_msp_dl_config')):
        g = 'TI startup, SysConfig output, DriverLib'
    else:
        g = 'C runtime (libc, compiler helpers)'
    f, r = groups.get(g, (0, 0))
    groups[g] = (f + code + ro, r + rw)

fu, ru = mem['FLASH'][1], mem['SRAM'][1]
print(f"FLASH used {fu:,} bytes of {mem['FLASH'][0]:,}")
print(f"SRAM  used {ru:,} bytes of {mem['SRAM'][0]:,}; {mem['SRAM'][0] - ru:,} bytes left")
print(f"{'':42}{'flash':>7}{'RAM':>7}")
for g in ('FreeRTOS kernel and port', 'App (this folder)', 'TI startup, SysConfig output, DriverLib',
          'C runtime (libc, compiler helpers)', 'Linker tables', 'Interrupt and startup stack (.stack)',
          'C library heap (.sysmem)'):
    if g in groups:
        print(f"{g:42}{groups[g][0]:>7,}{groups[g][1]:>7,}")
print("(module rows exclude alignment padding; the totals above include it)")
