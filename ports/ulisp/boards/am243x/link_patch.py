#!/usr/bin/env python3
"""Linker-file patch for the uLisp image: 64 kB main stack, MSRAM region 1.3 MB, and the symbol __stack_floor (bottom of that stack) for uLisp's depth check."""
import re, sys
p = sys.argv[1]; t = open(p).read()
t, n = re.subn(r"--stack_size=\d+", "--stack_size=65536", t); assert n == 1
t, n = re.subn(r"(MSRAM\s*: ORIGIN = 0x70080000 , LENGTH = )0x[0-9a-fA-F]+", r"\g<1>0x150000", t); assert n == 1
t += "\n__stack_floor = __STACK_END - 0x10000;\n"
open(p, "w").write(t)
