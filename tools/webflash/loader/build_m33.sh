#!/bin/bash
# Build the Cortex-M33 flash loader and the per-sector CRC routine; print the hex strings mspm0-dap.js embeds.
set -eu
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CC="${TICLANG:-$(ls -d /Applications/ti/ccs*/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS 2>/dev/null | tail -1)}/bin"
tmp="$(mktemp -d)"
for n in mspm33_loader crc_sectors; do
  "$CC/tiarmclang" -mcpu=cortex-m0plus -mthumb -Os -ffreestanding -fno-builtin -ffunction-sections -c "$here/$n.c" -o "$tmp/$n.o"
  "$CC/tiarmobjdump" -r "$tmp/$n.o" | grep -q "R_ARM" && { echo "relocations in $n"; exit 1; } || true
  "$CC/tiarmobjcopy" -O binary --only-section=.text.entry "$tmp/$n.o" "$tmp/$n.bin"
  echo "$n $(wc -c < "$tmp/$n.bin") bytes"; python3 -c "print(open('$tmp/$n.bin','rb').read().hex())"
done
