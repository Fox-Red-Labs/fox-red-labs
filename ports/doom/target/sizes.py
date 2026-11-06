#!/usr/bin/env python3
"""Section sizes of a linked image: python3 -I sizes.py <doom.out> [tiarmobjdump path]. Flash and RAM totals match the linker map."""
import subprocess, sys
img = sys.argv[1]
objdump = sys.argv[2] if len(sys.argv) > 2 else 'tiarmobjdump'
out = subprocess.check_output([objdump, '-h', img]).decode().splitlines()
s = {}
for l in out:
    p = l.split()
    if len(p) >= 5 and p[0].isdigit() and p[1].startswith('.') and not p[1].startswith(('.debug', '.symtab', '.strtab', '.shstrtab', '.symtab_meta', '.ARM.attr', '.TI.section')):
        s[p[1]] = (int(p[2], 16), int(p[3], 16))
get = lambda n: s.get(n, (0, 0))[0]
flash = {n: v[0] for n, v in s.items() if 0x10000000 <= v[1] < 0x10100000}
ram = {n: v[0] for n, v in s.items() if 0x30000000 <= v[1] < 0x30400000}
print(' '.join(f'{n}={v}' for n, v in sorted(s.items())))
print(f"FLASH text={get('.text')+get('.intvecs')} const+rodata={get('.const')+get('.rodata')} cinit={get('.cinit')} total_flash={sum(flash.values())}")
print(f"RAM   data={get('.data')} bss={get('.bss')} stack={get('.stack')} sysmem={get('.sysmem')} total_ram={sum(ram.values())}")
