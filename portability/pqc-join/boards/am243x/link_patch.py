#!/usr/bin/env python3
"""build_bench.sh LINK_PATCH hook for the pqc image: bigger main stack (PQC_STACK_BYTES) and a 1.3 MB MSRAM region (up to the SDK's user shared memory)."""
import os, re, sys
p = sys.argv[1]
t = open(p).read()
t, n = re.subn(r"--stack_size=\d+", "--stack_size=" + os.environ.get("PQC_STACK_BYTES", "98304"), t); assert n == 1
t, n = re.subn(r"(MSRAM\s*: ORIGIN = 0x70080000 , LENGTH = )0x[0-9a-fA-F]+", r"\g<1>0x150000", t); assert n == 1
open(p, "w").write(t)
