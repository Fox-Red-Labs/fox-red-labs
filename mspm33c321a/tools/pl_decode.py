#!/usr/bin/env python3
"""Decode a run_image.py read-out of the power-loop image (symbols fom and pl) into the fom capture format of
tools/results/fom_decode.py (fom[i]=v lines, written beside the input as <name>.fom.txt) and print the struct pl.
   pl_decode.py <read-out.txt>"""
import re, struct, sys
p = sys.argv[1]; t = open(p).read()
g = lambda n, b: bytes.fromhex(re.search(n + r' @0x\w+: (\w+)', t).group(1))[:b]
w = struct.unpack('<48I', g('fom', 192)); pl = g('pl', 104)
open(p.replace('.txt', '.fom.txt'), 'w').write(''.join('fom[%d]=%d\n' % (i, v) for i, v in enumerate(w)))
magic, n, cfmn, cfmx, cqmn, cqmx = struct.unpack('<6I', pl[:24])
print('pl magic %08X n_math %d  one call cycles: float %d..%d  q15 %d..%d' % (magic, n, cfmn, cfmx, cqmn, cqmx))
names = 'irqs w_min w_max w_sum w_first w_max_rest adc_min adc_max last_duty period'.split()
for k in (0, 1):
    m = dict(zip(names, struct.unpack('<10I', pl[24 + 40 * k:64 + 40 * k])))
    print(['float', 'q15'][k], m, 'mean %.1f' % (m['w_sum'] / max(m['irqs'], 1)))
