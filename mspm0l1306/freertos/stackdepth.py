#!/usr/bin/env python3
"""Worst-case stack per FreeRTOS task and for the interrupt stack, from the compiler's -fstack-usage
files (.su, frame size of every function compiled here) and the image's disassembly (who calls whom).
Functions with no .su file (DriverLib, C library) get their frame from their push/sub sp prologue.
Usage: stackdepth.py <build dir> <image.dis>

A task's stack must hold its deepest call chain plus one saved context: 32 bytes the core pushes on
an interrupt, up to 4 bytes of alignment, and 36 bytes FreeRTOS's PendSV handler saves (EXC_RETURN,
r4 to r11). Stack-overflow check method 2 also needs the bottom 16 bytes untouched. Calls through
function pointers are listed, not followed."""
import glob
import re
import sys

bdir, dis = sys.argv[1], sys.argv[2]
su = {}
for f in glob.glob(bdir + '/*.su'):
    for line in open(f):
        parts = line.rstrip('\n').split('\t')
        if len(parts) >= 2:
            su[parts[0].rsplit(':', 1)[-1]] = int(parts[1])

funcs, order, cur = {}, [], None
for line in open(dis):
    m = re.match(r'^([0-9a-f]{8}) <(.+)>:$', line)
    if m:
        cur = m.group(2)
        funcs[cur] = []
        order.append(cur)
        continue
    m = re.match(r'^\s+[0-9a-f]+:\s+(\S+)\s*(.*)$', line)
    if m and cur:
        funcs[cur].append((m.group(1), m.group(2)))


def frame(f):
    if f in su:
        return su[f], 'su'
    n = 0
    for op, arg in funcs.get(f, [])[:8]:
        if op == 'push':
            n += 4 * len(arg.strip('{}').split(','))
        if op.startswith('sub') and 'sp' in arg.split(',')[0]:
            mm = re.search(r'#(0x[0-9a-fA-F]+|\d+)', arg)
            if mm:
                n += int(mm.group(1), 0)
    return n, 'prologue'


def calls(f):
    c, t, ind = set(), set(), False
    for op, arg in funcs.get(f, []):
        m = re.search(r'<([^>+]+)>', arg)
        if op in ('bl',) and m:
            c.add(m.group(1))
        elif op in ('b', 'b.n', 'b.w') and m and m.group(1) != f and m.group(1) in funcs:
            t.add(m.group(1))
        elif op == 'blx':
            ind = True
    return c, t, ind


memo, indirect = {}, set()


def depth(f, seen=()):
    if f in memo:
        return memo[f]
    if f in seen:
        return 0
    fr, _ = frame(f)
    c, t, ind = calls(f)
    if ind:
        indirect.add(f)
    d = fr + max([depth(x, seen + (f,)) for x in c] or [0])
    d = max([d] + [depth(x, seen + (f,)) for x in t])
    memo[f] = d
    return d


def chain(f, seen=()):
    """deepest path, for the report"""
    if f in seen:
        return [f]
    fr, _ = frame(f)
    c, t, _ = calls(f)
    best, bpath = fr, [f]
    for x in sorted(c):
        d = fr + depth(x)
        if d > best:
            best, bpath = d, [f] + chain(x, seen + (f,))
    for x in sorted(t):
        if depth(x) > best:
            best, bpath = depth(x), [f + '(tail)'] + chain(x, seen + (f,))
    return bpath


CTX = 32 + 4 + 36
GUARD = 16
tasks = [f for f in ('blink_task', 'minimal_task', 'checker_task', 'bench_task', 'peer_task', 'hi_task',
                     'prvIdleTask') if f in funcs]
print('Task stacks (bytes): deepest call chain, + 72 saved context, + 16 overflow-check guard = need')
for t in tasks:
    d = depth(t)
    need = d + CTX + GUARD
    print(f'  {t:14} chain {d:4}  need {need:4} ({(need + 3) // 4} words)   via {" > ".join(chain(t))}')

print('\nInterrupt stack (MSP, the linker .stack):')
d_main = depth('main')
print(f'  main before the scheduler starts: {d_main} bytes   via {" > ".join(chain("main"))}')
path = ['main', 'vTaskStartScheduler', 'xPortStartScheduler', 'vStartFirstTask']
res = sum(frame(f)[0] for f in path if f in funcs)
res += 36 + sum(frame(f)[0] for f in ('vPortSVCHandler_C', 'vRestoreContextOfFirstTask') if f in funcs)
print(f'  left on it for good once tasks run (main > vTaskStartScheduler > xPortStartScheduler > SVC): {res} bytes')
low = max(depth(f) for f in ('SysTick_Handler', 'PendSV_Handler') if f in funcs)
irqs = [f for f in funcs if f.endswith('_IRQHandler') and f in su]  # handlers compiled here, not weak aliases
hi = max([depth(f) for f in irqs] or [0])
for f in ['SysTick_Handler', 'PendSV_Handler'] + irqs:
    if f in funcs:
        print(f'  {f:18} {depth(f):4} bytes   via {" > ".join(chain(f))}')
worst = max(d_main, res + 36 + low + (36 + hi if irqs else 0))
print(f'  worst case: {worst} bytes (kernel interrupt at the lowest priority, '
      f'{"an application interrupt nested on it" if irqs else "no application interrupt"})')
if indirect:
    print('\nCalls through pointers, not followed:', ', '.join(sorted(indirect)))
nosu = sorted({x for t in tasks + ['main'] for x in chain(t) if x.split('(')[0] not in su})
if nosu:
    print('Frames from the prologue (no .su file):', ', '.join(nosu))
