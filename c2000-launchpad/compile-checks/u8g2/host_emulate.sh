#!/bin/bash
# Host emulation, not the C28x: build the same program twice with the Mac's clang, once with 8-bit uint8_t and
# once with uint8_t widened to 16 bits (as on the C28x), and compare the bytes each would send to the display.
# This tests only u8g2's reliance on 8-bit wraparound; int stays 32 bits here (16 on the C28x).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; U=$HERE/../../../third-party/u8g2; T=$(mktemp -d)
python3 -I - "$U/extract/fonts_extract.c" "$T/fonts_braces.c" <<'PY'
import re, sys
src = open(sys.argv[1], encoding="latin-1").read()
out = ['#include "u8g2.h"']
for m in re.finditer(r'const uint8_t (\w+)\[(\d+)\][^=]*=\s*((?:"(?:[^"\\]|\\.)*"\s*)+);', src):
    name, n, lits = m.group(1), int(m.group(2)), m.group(3)
    raw = "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', lits))
    data = raw.encode("latin-1").decode("unicode_escape").encode("latin-1") + b"\0"
    assert len(data) == n, (name, len(data), n)
    out.append("const uint8_t %s[%d] = {%s};" % (name, n, ",".join(str(b) for b in data)))
open(sys.argv[2], "w").write("\n".join(out) + "\n")
PY
SRCS=$(ls "$U"/csrc/*.c | grep -v -e u8g2_d_setup.c -e u8g2_d_memory.c)
cc -O1 -w -DFRL_HOST -I"$U/csrc" $SRCS "$T/fonts_braces.c" "$HERE/u8g2_check.c" -o "$T/b8"
printf '#include <stdint.h>\n#define uint8_t uint16_t\n#define int8_t int16_t\n' > "$T/wide.h"
cc -O1 -w -DFRL_HOST -include "$T/wide.h" -I"$U/csrc" $SRCS "$T/fonts_braces.c" "$HERE/u8g2_check.c" -o "$T/b16"
"$T/b8" > "$HERE/logs/host-8bit.txt"; "$T/b16" > "$HERE/logs/host-16bit.txt"
tail -1 "$HERE/logs/host-8bit.txt"; tail -1 "$HERE/logs/host-16bit.txt"
if diff -q <(sed '$d' "$HERE/logs/host-8bit.txt") <(sed '$d' "$HERE/logs/host-16bit.txt") >/dev/null; then echo "same bytes"; else echo "DIFFERENT"; diff "$HERE/logs/host-8bit.txt" "$HERE/logs/host-16bit.txt" | head; fi
rm -rf "$T"
