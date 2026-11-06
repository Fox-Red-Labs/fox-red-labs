#!/usr/bin/env python3
"""Read the size files the board builds leave in out/target and write RESULTS-ROWS.csv for the Arduboy games
(ports/arduboy) and the Peanut-GB front end (ports/peanut-gb), in the column format of data/results/<board>.csv."""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ARD = os.path.dirname(HERE)
PGB = os.path.join(os.path.dirname(ARD), 'peanut-gb')
HEADER = 'board,test,variant,status,value,value_hi,runs,clock_mhz,conditions,date,source,checked,note'
DATE = '2026-10-20'
BOARDS = ['mspm0g3507', 'mspm0g3519', 'mspm0g5187', 'mspm33c321a']
FLASH = {'mspm0g3507': 131072, 'mspm0g3519': 524288, 'mspm0g5187': 131072, 'mspm33c321a': 1048576}
RAM = {'mspm0g3507': 32768, 'mspm0g3519': 131072, 'mspm0g5187': 32768, 'mspm33c321a': 262144}
COND = ('TI Arm Clang 5.1.1.LTS -Os; MSPM0 SDK 2.11.00.07 or MSPM33 SDK 1.04.00.00 startup file and linker script; '
        'null display, no board driver; RAM is .data plus .bss, stack not counted')


def sizes(path):
    sec = {}
    for line in open(path):
        m = re.match(r'^(\.\S+)\s+(\d+)\s+', line)
        if m:
            sec[m.group(1)] = int(m.group(2))
    flash = sum(sec.get(n, 0) for n in ['.intvecs', '.text', '.const', '.cinit', '.pinit', '.rodata', '.ARM.exidx', '.init_array', '.binit'])
    ram = sec.get('.data', 0) + sec.get('.bss', 0)
    stack = sec.get('.stack', 0) + sec.get('.sysmem', 0)
    return flash, ram, stack


def q(s):
    return '"%s"' % s if (',' in s or '"' in s) else s


def row(board, test, variant, status, value, conds, source, checked, note):
    return ','.join([board, test, variant, status, str(value), '', '1' if status == 'measured' else '', '', q(conds), DATE, source, checked, q(note)])


def build(rows, root, items, source, note_extra):
    for board in BOARDS:
        for variant, relpath, label in items:
            d = os.path.join(root, 'out', 'target', relpath(board))
            sz = os.path.join(d, 'size.txt')
            if os.path.exists(sz):
                f, r, st = sizes(sz)
                rows.append(row(board, 'port_flash_bytes', variant, 'measured', f, COND, source, 'linker',
                                'builds and links, not run on the board; %s; flash is %d of %d B' % (label, f, FLASH[board])))
                rows.append(row(board, 'port_ram_bytes', variant, 'measured', r, COND, source, 'linker',
                                'builds and links, not run on the board; SDK default stack %d B on top; RAM is %d of %d B' % (st, r, RAM[board])))
            else:
                log = os.path.join(d, 'link.log')
                m = re.search(r'size 0x([0-9a-f]+)\.\s+Available memory ranges:\s+FLASH\s+size: 0x([0-9a-f]+)\s+unused: 0x([0-9a-f]+)\s+max hole: 0x([0-9a-f]+)', open(log).read().replace('\n', ' ')) if os.path.exists(log) else None
                short = ''
                if m:
                    need, hole = int(m.group(1), 16), int(m.group(4), 16)
                    short = -(need - hole)
                rows.append(row(board, 'port_flash_bytes', variant, 'does_not_fit', short, COND, source, 'linker',
                                'link fails: the ROM image does not fit in %d B of flash%s' % (FLASH[board], '; value is the shortfall in bytes' if short != '' else '')))
            rows.append(row(board, 'hobby_works', variant, 'waiting', '', 'host-only wave: no board was flashed; EDUMKII and Sharp drivers plug into the abstract interface next wave',
                            source, '', 'ran on the Mac host only; run on the board after the drivers land'))


def main():
    rows = [HEADER]
    games = [('arduboy2-snake', 'snake'), ('arduboy2-tileworld', 'tileworld'), ('arduboy2-crate-confusion', 'crate-confusion')]
    build(rows, ARD, [(v, (lambda g: lambda b: '%s/%s' % (b, g))(g), 'Arduboy2 layer plus %s' % g) for v, g in games], 'ports/arduboy/FIT.md', '')
    open(os.path.join(ARD, 'RESULTS-ROWS.csv'), 'w').write('\n'.join(rows) + '\n')
    prow = [HEADER]
    pitems = [('peanut-gb-code', lambda b: b, 'Peanut-GB front end, ROM read from a fixed address'),
              ('peanut-gb-dmg-acid2', lambda b: b + '-acid', 'front end plus the 32 KB dmg-acid2 ROM linked in'),
              ('peanut-gb-tobu', lambda b: b + '-rom', 'front end plus the 256 KB Tobu Tobu Girl ROM linked in')]
    build(prow, PGB, pitems, 'ports/arduboy/FIT.md', '')
    open(os.path.join(PGB, 'RESULTS-ROWS.csv'), 'w').write('\n'.join(prow) + '\n')
    print('wrote %d + %d rows' % (len(rows) - 1, len(prow) - 1))


if __name__ == '__main__':
    main()
