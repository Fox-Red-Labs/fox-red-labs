#!/bin/bash
# Flash a GCC CoreMark build (build-gcc.sh) to the LP-MSPM0L1306 (probe ML130001), paint SRAM, run, read the
# result, the log and, in the same OpenOCD session, the clock and flash settings the run used:
# SYSCTL MCLKCFG (0x400B0104, FLASHWAIT in bits 11:8) and CPUSS CTL (0x40401300: prefetch, cache, literal cache).
#   ./run-gcc.sh <folder suffix, e.g. O3-32 or O3-32-val> <wait_seconds> [run label, e.g. run1]
# Writes coremark-output.txt, ram.txt, regs.txt and sram.bin into the build folder (or <folder>/<label>/),
# then prints the EEMBC reporting line (report.py).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; T="$HERE/../../tools"; B="$HERE/build-gcc-$1"; OUT="$B${3:+/$3}"; mkdir -p "$OUT"
NM=~/zephyr-toolchain/arm-zephyr-eabi/bin/arm-zephyr-eabi-nm
"$T/ocd.sh" "reset halt" "program $B/coremark.elf verify" > "$OUT/flash.txt" 2>&1; grep -qE "Verified OK" "$OUT/flash.txt" || { echo "flash failed"; tail -5 "$OUT/flash.txt"; exit 1; }
sym() { $NM "$B/coremark.elf" | awk -v s="$1" '$3==s {print $1}'; }
R=$(sym cm_result); L=$(sym cm_log); E=$(sym _end)
"$T/ocd.sh" "reset halt" "mww 0x20000000 0xA5A5A5A5 1024" resume "sleep $(( $2 * 1000 ))" halt "mdw 0x$R 5" \
  "mdw 0x400B0104" "mdw 0x40401300" "dump_image $OUT/cm_log.bin 0x$L 800" "dump_image $OUT/sram.bin 0x20000000 4096" resume \
  | grep -E "^0x" | tee "$OUT/ram.txt"
tr -d '\000' < "$OUT/cm_log.bin" | tr -d '\245' > "$OUT/coremark-output.txt"; cat "$OUT/coremark-output.txt"
python3 - "$OUT/sram.bin" "$E" <<'PY'
import sys
d=open(sys.argv[1],'rb').read(); s=int(sys.argv[2],16)-0x20000000; s+=(-s)%4
deep=next((i for i in range(s,0x1000,4) if d[i:i+4]!=b'\xa5'*4),0x1000)
print(f"stack: top of SRAM down to static data end 0x{0x20000000+s:08x} ({0x1000-s} B free for it); deepest use {0x1000-deep} B; {'OVERFLOW into static data' if deep<=s else 'ok'}")
PY
python3 "$HERE/report.py" "$OUT"
