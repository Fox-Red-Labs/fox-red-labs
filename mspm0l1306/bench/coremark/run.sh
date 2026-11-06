#!/bin/bash
# Flash a CoreMark build to the LP-MSPM0L1306 (probe ML130001), paint SRAM, run, read the results from RAM.
#   ./run.sh <opt> <clk> <wait_seconds>
# SRAM is filled with 0xA5A5A5A5 after a halted reset, before any code runs, so the lowest overwritten
# word shows how deep the stack went (the stack sits at the top of SRAM).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; T="$HERE/../../tools"; OUT="$HERE/build-$1-$2"
NM=~/zephyr-toolchain/arm-zephyr-eabi/bin/arm-zephyr-eabi-nm
"$T/ocd.sh" "reset halt" "program $OUT/coremark.out verify" > "$OUT/flash.txt" 2>&1; grep -E "Verified OK|Error" "$OUT/flash.txt" | tail -2
R=$($NM "$OUT/coremark.out" | awk '$3=="cm_result" {print $1}'); L=$($NM "$OUT/coremark.out" | awk '$3=="cm_log" {print $1}')
"$T/ocd.sh" "reset halt" "mww 0x20000000 0xA5A5A5A5 1024" "resume" "sleep $(( $3 * 1000 ))" halt "mdw 0x$R 5" \
  "dump_image $OUT/cm_log.bin 0x$L 800" "dump_image $OUT/sram.bin 0x20000000 4096" resume | grep -E "^0x|dumped" | tee "$OUT/ram.txt"
tr -d '\000' < "$OUT/cm_log.bin" | tr -d '\245' > "$OUT/coremark-output.txt"; cat "$OUT/coremark-output.txt"
python3 - "$OUT/sram.bin" "$OUT/coremark.map" <<'PY'
import sys,re
d=open(sys.argv[1],'rb').read(); m=open(sys.argv[2]).read()
secs={k:(int(a,16),int(n,16)) for k,a,n in re.findall(r'^(\.\w+)\s+0\s+([0-9a-f]{8})\s+([0-9a-f]{8})',m,re.M)}
lo,n=secs['.stack']; top=lo+n
static_end=max(a+n for k,(a,n) in secs.items() if k in('.bss','.data','.sysmem','.vtable'))
s=static_end-0x20000000; s+=(-s)%4
deep=next((i for i in range(s,0x1000,4) if d[i:i+4]!=b'\xa5'*4),0x1000)
print(f"stack: {n} bytes reserved at 0x{lo:08x}; deepest use {top-0x20000000-deep} bytes (lowest write 0x{0x20000000+deep:08x}); static data ends 0x{static_end:08x}; {'OVERFLOW past the reserved stack' if 0x20000000+deep<lo else 'within the reserved stack'}")
PY
