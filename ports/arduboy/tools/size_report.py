#!/usr/bin/env python3
"""Read `tiarmsize -A` output and print flash and RAM for one build.
flash = code and constants and init data copied from flash; ram = .data + .bss + .stack + .sysmem."""
import re
import sys

path, board, game = sys.argv[1:4]
sec = {}
for line in open(path):
    m = re.match(r'^(\.\S+)\s+(\d+)\s+(0x[0-9a-fA-F]+|\d+)', line)
    if m:
        sec[m.group(1)] = int(m.group(2))
flash_names = ['.intvecs', '.text', '.const', '.cinit', '.pinit', '.rodata', '.ARM.exidx', '.init_array', '.binit']
ram_static = ['.data', '.bss']
ram_other = ['.stack', '.sysmem', '.args', '.vtable']
flash = sum(sec.get(n, 0) for n in flash_names)
rs = sum(sec.get(n, 0) for n in ram_static)
ro = sum(sec.get(n, 0) for n in ram_other)
print('%s %s: flash %d B, ram %d B (data+bss %d B, SDK default stack %d B)' % (board, game, flash, rs + ro, rs, ro))
print('SIZE,%s,%s,%d,%d,%d' % (board, game, flash, rs + ro, rs))
