#!/bin/bash
# Build the MSPM0 RAM checksum routine and print its bytes as the hex string mspm0-dap.js embeds.
set -eu
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CC="${TICLANG:-$(ls -d /Applications/ti/ccs*/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS 2>/dev/null | tail -1)}/bin"
tmp="$(mktemp -d)"
"$CC/tiarmclang" -mcpu=cortex-m0plus -mthumb -Os -ffreestanding -fno-builtin -ffunction-sections -c "$here/crc32.c" -o "$tmp/l.o"
# entry first, then program; position-independent (no relocations allowed)
"$CC/tiarmobjdump" -r "$tmp/l.o" | grep -q "R_ARM" && { "$CC/tiarmobjdump" -r "$tmp/l.o"; echo "relocations found"; exit 1; } || true
"$CC/tiarmobjcopy" -O binary --only-section=.text.entry "$tmp/l.o" "$tmp/entry.bin"
printf '' > "$tmp/program.bin"
"$CC/tiarmobjdump" -d "$tmp/l.o" > "$here/crc32.lst"
python3 - "$tmp/entry.bin" "$tmp/program.bin" <<'PY'
import sys
e, p = (open(f, "rb").read() for f in sys.argv[1:3])
print(len(e), len(p))
print((e + p).hex())
PY
