#!/bin/bash
# Compile and link check of u8g2 2.37.1 (third-party/u8g2) for the C28x, LAUNCHXL-F2800137 flash layout. No board.
#   check.sh plain|c2000ware|uchar
#   plain: the compiler's own stdint.h (it has no uint8_t on the C28x); c2000ware: uint8_t as C2000Ware's
#   driverlib defines it (uint16_t); uchar: uint8_t as unsigned char (also 16 bits on the C28x).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; U=$HERE/../../../third-party/u8g2; B=$HERE/../../bench
C2K=/Applications/ti/c2000ware_core_26_02_00_00; CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS
MODE=${1:-plain}; OUT=$(mktemp -d); LOG="$HERE/logs/c28x-$MODE.txt"
case $MODE in c2000ware) PRE="--preinclude=$HERE/../trx-host/stubs/c2000ware_u8.h";; uchar) PRE="--preinclude=$HERE/stubs_uchar.h";; *) PRE="";; esac
CF="-v28 -ml -mt --float_support=fpu32 --abi=eabi --opt_level=2 --c99 --display_error_number --define=_FLASH $PRE -I$U/csrc -I$CGT/include"
{
echo "# cl2000 $("$CGT/bin/cl2000" -version 2>&1 | head -1 | sed 's/.*Version/Version/'); u8g2 2.37.1; mode $MODE"
for f in "$U"/csrc/*.c "$U"/extract/fonts_extract.c "$HERE/u8g2_check.c"; do
  echo "== $(basename "$f")"; "$CGT/bin/cl2000" $CF -c "$f" --obj_directory="$OUT" 2>&1; echo "exit $?"
done
echo "== link (without u8g2_d_setup and u8g2_d_memory, which reference every display and buffer)"
"$CGT/bin/cl2000" $CF -c "$C2K/device_support/f280013x/common/source/f280013x_codestartbranch.asm" --obj_directory="$OUT" 2>&1
( cd "$OUT" && rm -f u8g2_d_setup.obj u8g2_d_memory.obj && "$CGT/bin/cl2000" $CF -z --entry_point code_start --stack_size=0x400 --heap_size=0 \
   -i"$CGT/lib" --reread_libs --rom_model -m "$HERE/logs/u8g2-$MODE.map" -o u8g2.out *.obj "$B/bench_flash.cmd" -llibc.a 2>&1 ); echo "exit $?"
} > "$LOG" 2>&1
rm -rf "$OUT"
echo "files compiled: $(grep -c '^exit 0' "$LOG") of $(grep -c '^== ' "$LOG") steps"; grep -cE 'error #' "$LOG" | sed 's/^/errors: /'; grep -cE 'warning #' "$LOG" | sed 's/^/warnings: /'
