#!/usr/bin/env python3
"""Summarise a wake-word serial log: rates against the Mac clock, detections, margins.   stats.py LOG"""
import re, sys
L = [l for l in open(sys.argv[1]) if '#inf=' in l and l.split()[0].replace('.', '').isdigit() and 'cyc=' in l or ('#inf=' in l and 'npu_cyc' not in l)]
g = lambda l, k: int(re.search(r'(?<![a-z_])' + k + r'=(-?\d+)', l).group(1))
t = lambda l: float(l.split()[0])
a, b = L[0], L[-1]; dt = t(b) - t(a)
cyc = sum((g(y, 'cyc') - g(x, 'cyc')) % 2**32 for x, y in zip(L, L[1:]))
out = {'lines': len(L), 'secs': round(dt, 1), 'inferences': g(b, 'inf') - g(a, 'inf'), 'inf_per_s': round((g(b, 'inf') - g(a, 'inf')) / dt, 2),
       'events': g(b, 'ev') - g(a, 'ev'), 'max_margin': max(g(l, 'maxmargin') for l in L), 'MCLK_MHz': round(cyc / dt / 1e6, 1)}
if 'adc_isr=' in b: out['adc_Hz'] = round((g(b, 'adc_isr') - g(a, 'adc_isr')) / dt)
for k in ('npu_cyc', 'lpc_cyc', 'peak', 'meanabs'):
    v = [g(l, k) for l in L if re.search(r'(?<![a-z_])' + k + '=', l)]
    if v: out[k + '_median'] = sorted(v)[len(v) // 2]
print(out)
