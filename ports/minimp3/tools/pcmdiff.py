#!/usr/bin/env python3 -I
"""Compare two raw int16 PCM files: sample count, number of differing samples, max absolute difference.
  pcmdiff.py a.raw b.raw     (b may be longer or shorter by an encoder delay: use --skip N to drop N samples from b)"""
import sys, struct, array
a = array.array('h'); b = array.array('h')
a.frombytes(open(sys.argv[1], 'rb').read()); b.frombytes(open(sys.argv[2], 'rb').read())
skip = int(sys.argv[sys.argv.index('--skip') + 1]) if '--skip' in sys.argv else 0
b = b[skip:]; n = min(len(a), len(b))
d = [abs(a[i] - b[i]) for i in range(n)]
print('samples a=%d b=%d compared=%d differing=%d max_abs_diff=%d' % (len(a), len(b), n, sum(1 for x in d if x), max(d) if d else 0))
