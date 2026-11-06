#!/usr/bin/env python3
"""Decode the results a FreeRTOS image left in RAM, from the SRAM dump that
../tools/paintrun.sh saves next to the image (build-<name>/sram.bin), and check them.
Usage: check.py build-<name>      (blink, bench, bench-lean, edge-static, edge-heap)
Symbol addresses come from build-<name>/ram-symbols.txt, written by build.sh."""
import os
import struct
import sys

d = sys.argv[1].rstrip('/')
name = os.path.basename(d)[len('build-'):]
ram = open(os.path.join(d, 'sram.bin'), 'rb').read()
syms = {}
for line in open(os.path.join(d, 'ram-symbols.txt')):
    a, s, _, n = line.split()
    syms[n] = int(a, 16)


def words(sym, n):
    off = syms[sym] - 0x20000000
    return struct.unpack('<%dI' % n, ram[off:off + 4 * n])


ok = True


def check(cond, what):
    global ok
    print(('  ok    ' if cond else '  FAIL  ') + what)
    ok = ok and cond


f = words('fr_fault', 4)
task = struct.pack('<I', f[3]).split(b'\0')[0].decode('ascii', 'replace')
print(f'fr_fault: code 0x{f[0]:08x} line {f[1]} pc 0x{f[2]:08x} task "{task}"')
check(f[0] == 0, 'no fault recorded (0xDEAD0001 stack overflow, 0xDEAD0002 assert, 0xDEAD0003 hard fault)')

if name.startswith('blink'):
    m, tog, ticks, hb, hi, sw = words('fr_blink', 6)
    print(f'fr_blink: magic 0x{m:08x} toggles {tog} ticks {ticks} stack {sw} words, '
          f'never used: blink {hb}, idle {hi}')
    check(m == 0xB1E00001, 'magic 0xB1E00001 (4 toggles done)')
    check(ticks == 500 * (tog - 1), 'ticks = 500 x (toggles - 1): the tick runs at 1,000 per second')
    check(hb > 4 and hi > 4, 'every task kept more than the 4-word overflow guard unused')

elif name.startswith('bench'):
    w = words('fr_bench', 43)
    names = ['yield_rt', 'sem_wake', 'notify_wake', 'queue_rt', 'irq_entry', 'irq_sem_task', 'irq_notify_task']
    m, n, timg, systick, tread = w[:5]
    print(f'fr_bench: magic 0x{m:08x} samples {n} TIMG0 {timg} counts vs SysTick {systick} cycles, '
          f'timer read {tread} cycles')
    print(f'  {"test":16}{"best":>7}{"worst":>7}{"worst, no tick":>16}{"average":>9}   (cycles, raw)')
    for i, t in enumerate(names):
        b, wo, wn, s = w[5 + 4 * i:9 + 4 * i]
        print(f'  {t:16}{b:>7}{wo:>7}{wn:>16}{s / max(n, 1):>9.1f}')
    err = w[33]
    sw, hw, ticks = w[34:38], w[38:42], w[42]
    print(f'  errors {err}; ticks at the end {ticks}')
    for t, a, b in zip(('bench', 'peer', 'hi', 'idle'), sw, hw):
        print(f'  stack {t:6} {a:3} words, {b:3} never used')
    check(m == 0xBE4C0001, 'magic 0xBE4C0001 (all tests done)')
    check(n == 1000, '1,000 samples per test')
    check(abs(timg - systick) <= 4, 'TIMG0 counts match SysTick cycles: TIMG0 runs at the CPU clock')
    check(err == 0, 'errors 0: every pass ran the other task exactly once, every queue reply matched')
    check(all(x > 4 for x in hw), 'every task kept more than the 4-word overflow guard unused')

elif name.startswith('edge'):
    (m, checks, made, alive, sw, hmin, hchk, hidle, htot, hfree, hmin_ever, ran_out) = words('fr_edge', 12)
    print(f'fr_edge: magic 0x{m:08x} checks {checks} made {made} alive {alive} stack {sw} words; '
          f'never used: minimal {hmin}, checker {hchk}, idle {hidle}')
    if 'heap' in name:
        print(f'  heap {htot} bytes, free {hfree} after the last create, lowest ever {hmin_ever}, '
              f'ran out {ran_out}')
        check(ran_out == 1, 'xTaskCreate failed for lack of memory (not the 24-task cap)')
    check(m == 0xED6E0001, 'magic 0xED6E0001 (two checks done)')
    check(alive == made, 'every minimal task ran in the last second')
    check(min(hmin, hchk, hidle) > 4, 'every task kept more than the 4-word overflow guard unused')

print('ALL OK' if ok else 'SOMETHING FAILED: see above')
