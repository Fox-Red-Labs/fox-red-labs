#!/usr/bin/env python3
# Usage: stackdepth.py <build dir built with -DEXTRA_CFLAGS=-fcallgraph-info=su> <function> [...]
# Worst-case static stack depth from GCC -fcallgraph-info=su files. Direct calls only.
import re, sys, os, functools
root, starts = sys.argv[1], sys.argv[2:]
size, edges, dyn = {}, {}, set()
for dp, _, fs in os.walk(root):
    for f in fs:
        if not f.endswith('.ci'): continue
        for l in open(os.path.join(dp, f), errors='replace'):
            m = re.match(r'node: \{ title: "([^"]+)" label: "[^\n"]*?\\n[^"]*?\\n(\d+) bytes \(([^)]*)\)', l)
            if m:
                n = m.group(1); size[n] = max(size.get(n, 0), int(m.group(2)))
                if 'dynamic' in m.group(3): dyn.add(n)
                continue
            m = re.match(r'edge: \{ sourcename: "([^"]+)" targetname: "([^"]+)"', l)
            if m: edges.setdefault(m.group(1), set()).add(m.group(2))
sys.setrecursionlimit(10000)
memo = {}
def depth(n, stack=()):
    if n in stack: return 0, [n + ' (recursion cut)']
    if n in memo: return memo[n]
    best, path = 0, []
    for c in edges.get(n, ()):
        d, p = depth(c, stack + (n,))
        if d > best: best, path = d, p
    r = (size.get(n, 0) + best, [f"{n} {size.get(n, '?')}"] + path)
    memo[n] = r
    return r
for s in starts:
    d, p = depth(s)
    print(f"{s}: {d} bytes worst direct-call path")
    for x in p: print("   ", x)
