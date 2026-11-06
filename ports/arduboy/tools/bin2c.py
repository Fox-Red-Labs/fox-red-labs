#!/usr/bin/env python3
"""Turn a binary file into a C array: bin2c.py in.bin out.c array_name length_name"""
import sys

src, dst, arr, ln = sys.argv[1:5]
data = open(src, 'rb').read()
with open(dst, 'w') as f:
    f.write('#include <stdint.h>\nconst uint8_t %s[%d] = {\n' % (arr, len(data)))
    for i in range(0, len(data), 16):
        f.write(','.join('0x%02x' % b for b in data[i:i + 16]) + ',\n')
    f.write('};\nconst uint32_t %s = %d;\n' % (ln, len(data)))
