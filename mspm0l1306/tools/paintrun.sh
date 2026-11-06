#!/bin/bash
# Flash an image to the LP-MSPM0L1306 (probe ML130001), paint SRAM with 0xA5 before any code runs,
# run for <ms>, halt, read <words> at symbol <sym>, report the deepest stack write.
#   paintrun.sh <image.out> <map> <sym> <words> <ms>
set -e
OUT="$1"; MAP="$2"; SYM="$3"; N="$4"; MS="$5"; D="$(dirname "$OUT")"; T="$(cd "$(dirname "$0")" && pwd)"
NM=~/zephyr-toolchain/arm-zephyr-eabi/bin/arm-zephyr-eabi-nm
A=$($NM "$OUT" | awk -v s="$SYM" '$3==s {print $1}')
"$T/ocd.sh" "reset halt" "program $OUT verify" > "$D/flash.txt" 2>&1; grep -qE "Verified OK" "$D/flash.txt" || { echo "flash failed"; tail -5 "$D/flash.txt"; exit 1; }
"$T/ocd.sh" "reset halt" "mww 0x20000000 0xA5A5A5A5 1024" resume "sleep $MS" halt "mdw 0x$A $N" "dump_image $D/sram.bin 0x20000000 4096" resume | grep -E "^0x" | tee "$D/ram.txt"
python3 - "$D/sram.bin" "$MAP" <<'PY'
import sys,re
d=open(sys.argv[1],'rb').read(); m=open(sys.argv[2]).read()
secs={k:(int(a,16),int(n,16)) for k,a,n in re.findall(r'^(\.\w+)\s+0\s+([0-9a-f]{8})\s+([0-9a-f]{8})',m,re.M)}
lo,n=secs['.stack']; top=lo+n
static_end=max(a+n for k,(a,n) in secs.items() if k in('.bss','.data','.sysmem','.vtable'))
s=static_end-0x20000000; s+=(-s)%4
deep=next((i for i in range(s,0x1000,4) if d[i:i+4]!=b'\xa5'*4),0x1000)
print(f"stack: {n} B reserved; deepest use {top-0x20000000-deep} B; static RAM {static_end-0x20000000} B; {'OVERFLOW' if 0x20000000+deep<lo else 'ok'}")
PY
