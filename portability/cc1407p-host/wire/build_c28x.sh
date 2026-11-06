#!/bin/bash
# Build the wire packer test for the C28x (F2800137) as a RAM image.   build_c28x.sh <outdir> <vectors dir from build_host.sh>
set -e
R=$(cd "$(dirname "$0")/../../.." && pwd); T=$R/third-party/ti-cc140xp-host; P=$R/portability/cc1407p-host; W=$P/wire
C2K=${C2000WARE:-/Applications/ti/c2000ware_core_26_02_00_00}
CGT=${C2000_CGT:-/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS}
OUT=${1:?outdir}; VEC=${2:?dir holding vectors.h}; mkdir -p "$OUT/obj"; cd "$OUT/obj"; rm -f *.obj
CF="--opt_level=2 -v28 -ml -mt --float_support=fpu32 --abi=eabi --c99 --define=_LAUNCHXL_F2800137 --define=F280013x=1 --define=CPU1 --define=RAM --define=generic_ram_lnk --diag_suppress=10063 --display_error_number --preinclude=$P/c28x/c28x_compat.h -I$VEC -I$W -I$T/host -I$T -I$P/shim -I$CGT/include"
for f in $W/test_wire.c $W/trx_wire.c $W/wire_main_c28x.c $T/rcl_settings_wisun.c $C2K/device_support/f280013x/common/source/f280013x_codestartbranch.asm; do
  b=$(basename "${f%.*}"); [ "${f##*.}" = asm ] && b=cs_$b
  "$CGT/bin/cl2000" $CF -c "$f" --output_file="$b.obj" > "$b.log" 2>&1 || { cat "$b.log" | head -30; exit 1; }
done
echo "errors: $(cat *.log | grep -c 'error #')  warnings: $(cat *.log | grep -c 'warning #')"
cat *.log > "$OUT/compile.log"
"$CGT/bin/cl2000" $CF -z --entry_point code_start --stack_size=0x3F0 --heap_size=0x100 -i"$CGT/lib" --reread_libs --rom_model \
  -m "$OUT/wire.map" -o "$OUT/wire_c28x.out" *.obj $C2K/device_support/f280013x/common/cmd/280013x_generic_ram_lnk.cmd -llibc.a > "$OUT/link.log" 2>&1 || { tail -25 "$OUT/link.log"; exit 1; }
echo "$OUT/wire_c28x.out"
grep -E "^\s*(\.text|\.bss|\.cinit|\.const|\.ebss|\.econst|\.stack|\.esysmem)\s" "$OUT/wire.map" | head
