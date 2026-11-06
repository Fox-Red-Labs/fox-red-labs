#!/bin/bash
# Build TI's CC1407P host driver + this repo's bare HAL + a fake-radio bench program for the C28x (F2800137), RAM image.
#   build_c28x.sh <outdir>          prints the .out path and the section sizes in 16-bit words
#   build_c28x.sh <outdir> check    compile only, with the shim off, to show the 524 errors (plain C28x build)
# Flags are those of c2000-launchpad/tools/c2kbuild.sh. TI's driver files are used unmodified.
set -e
R=$(cd "$(dirname "$0")/../../.." && pwd); T=$R/third-party/ti-cc140xp-host; P=$R/portability/cc1407p-host
C2K=${C2000WARE:-/Applications/ti/c2000ware_core_26_02_00_00}
CGT=${C2000_CGT:-/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS}
OUT=${1:?outdir}; MODE=${2:-build}; mkdir -p "$OUT/obj"; cd "$OUT/obj"; rm -f *.obj
PRE="--preinclude=$P/c28x/c28x_compat.h"; [ "$MODE" = noshim ] && PRE=""
CF="--opt_level=2 -v28 -ml -mt --float_support=fpu32 --abi=eabi --c99 --gen_func_subsections=on --define=_LAUNCHXL_F2800137 --define=F280013x=1 --define=CPU1 --define=RAM --define=generic_ram_lnk --diag_suppress=10063 --display_error_number $PRE -I$T/host -I$T -I$P -I$P/shim -I$CGT/include"
n=0; fail=0
for f in $T/host/trx_host.c $T/host/handlers/trx_host_handlers.c $T/host/private/trx_host_private.c $T/host/transport/spi_transport.c \
         $T/host/utility/utils.c $T/host/utility/list/list.c $T/host/utility/buffer_manager/buffer_manager.c \
         $T/rcl_settings_wisun.c $P/hal_bare.c $P/c28x/hooks_fake.c $P/c28x/bench_main.c \
         $C2K/device_support/f280013x/common/source/f280013x_codestartbranch.asm; do
  b=$(basename "${f%.*}"); [ "${f##*.}" = asm ] && b=cs_$b
  if "$CGT/bin/cl2000" $CF -c "$f" --output_file="$b.obj" > "$b.log" 2>&1; then n=$((n+1)); else fail=$((fail+1)); fi
done
echo "compiled: $n  failed: $fail  errors: $(cat *.log | grep -c 'error #')  warnings: $(cat *.log | grep -c 'warning #')"
cat *.log > "$OUT/compile.log"
[ "$MODE" = build ] || exit 0
[ $fail = 0 ] || exit 1
"$CGT/bin/cl2000" $CF -z --entry_point code_start --stack_size=0x3F0 --heap_size=0x100 -i"$CGT/lib" --reread_libs --rom_model \
  -m "$OUT/bench.map" -o "$OUT/cc1407p_c28x.out" *.obj $C2K/device_support/f280013x/common/cmd/280013x_generic_ram_lnk.cmd -llibc.a > "$OUT/link.log" 2>&1 || { tail -25 "$OUT/link.log"; exit 1; }
echo "$OUT/cc1407p_c28x.out"
"$CGT/bin/ofd2000" -x --obj_display=sections "$OUT/cc1407p_c28x.out" >/dev/null 2>&1 || true
"$CGT/bin/size2000" "$OUT/cc1407p_c28x.out" 2>/dev/null || grep -E "^\s*(\.text|\.bss|\.cinit|\.const|\.ebss|\.econst|\.stack|\.esysmem)\s" "$OUT/bench.map" | head
