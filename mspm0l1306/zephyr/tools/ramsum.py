#!/usr/bin/env python3
# Usage: ramsum.py <build>/zephyr/zephyr_pre0.map
# RAM output sections and the biggest RAM input pieces from a GNU ld map file (works on failed links).
import re, sys
lines = open(sys.argv[1]).read().splitlines()
secs, pieces, cur = [], [], None
i = 0
while i < len(lines):
    l = lines[i]
    m = re.match(r'^([A-Za-z_][\w.]*)\s+(0x[0-9a-f]+)\s+(0x[0-9a-f]+)', l)
    if not m and re.match(r'^([A-Za-z_][\w.]*)$', l) and i + 1 < len(lines):
        m2 = re.match(r'^\s+(0x[0-9a-f]+)\s+(0x[0-9a-f]+)', lines[i + 1])
        if m2:
            m = (l.strip(), m2.group(1), m2.group(2)); i += 1
    elif m:
        m = (m.group(1), m.group(2), m.group(3))
    if m:
        a, s = int(m[1], 16), int(m[2], 16)
        cur = m[0] if 0x20000000 <= a < 0x20100000 else None
        if cur and s: secs.append((m[0], s))
    else:
        p = re.match(r'^ (\.\S+)\s+(0x[0-9a-f]+)\s+(0x[0-9a-f]+)\s+(\S+)', l)
        if not p and re.match(r'^ (\.\S+)$', l) and i + 1 < len(lines):
            p2 = re.match(r'^\s+(0x[0-9a-f]+)\s+(0x[0-9a-f]+)\s+(\S+)', lines[i + 1])
            if p2: p = (l.strip(), p2.group(1), p2.group(2), p2.group(3)); i += 1
        elif p: p = p.groups()
        if p and cur and 0x20000000 <= int(p[1], 16) < 0x20100000 and int(p[2], 16) >= 128:
            pieces.append((int(p[2], 16), cur, p[0], p[3].split('/')[-1]))
    i += 1
print('sections:', ', '.join(f'{n} {s}' for n, s in secs if n != 'RAM'), '| total', sum(s for n, s in secs if n != 'RAM'))
for s, c, n, f in sorted(pieces, reverse=True)[:8]:
    print(f'   {s:6d}  {c:8s} {n}  ({f})')
