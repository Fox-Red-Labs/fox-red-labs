#!/bin/bash
# Build the post-quantum KAT + timing image for the NUCLEO-U083RC (Cortex-M0+, 48 MHz, 256 kB flash, 40 kB RAM) with ST's GCC 14.3.1.
# BUILD ONLY: compiles and links. Never touches a probe, a board or a serial port.
#   ./build.sh [-O3|-Os] [kem|dsa-lowram|dsa-default|lowram|default] [stack bytes|max]      defaults -O3 kem max
#     kem          ML-KEM-768 only (PQC_NO_DSA)
#     dsa-lowram   ML-DSA-44 only (PQC_NO_KEM), built with -DMLD_CONFIG_REDUCE_RAM
#     dsa-default  ML-DSA-44 only, without it (sign allocates 44,704 B)
#     lowram       both algorithms, ML-DSA with REDUCE_RAM;  default: both, without it
#   STACK=max (third argument): link at the largest stack that fits RAM (found by linking, in steps of 256 B).
#   Image: $OUTROOT/bench-pqc-<opt>-<variant>-<stack>/app.elf, app.map and build.log beside it (OUTROOT defaults to out/nucleo-u083rc).
# Output: object sizes (text = code + read-only data), linked flash and RAM from the map, or the linker's overflow in bytes.
# The linker script is ST's STM32U083RCTX_FLASH.ld from the GPIO_IOToggle example with the RAM length raised from 32K to 40K (the part has 40 kB;
# nucleo-u083rc/bench/ram40k proves the upper 8 kB is usable) and _Min_Stack_Size set to the stack size; the stack sits at the top of RAM, so the
# stack is what is left of the 40 kB after .data and .bss and the linker reports "region RAM overflowed by N bytes" when it does not fit.
set -u
OPT="${1:--O3}"; VAR="${2:-kem}"; STK="${3:-max}"
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../../../.." && pwd)"
CC="${CC:-/opt/ST/STM32CubeCLT_1.22.0/GNU-tools-for-STM32/bin/arm-none-eabi-gcc}"; export CC
. "$REPO/nucleo-u083rc/bench/common/u0.sh"
PJ="$REPO/portability/pqc-join/core"; ML="$REPO/third-party/mlkem-native/mlkem"; MD="$REPO/third-party/mldsa-native/mldsa"
NAME="pqc${OPT}-${VAR}"
u0_begin pqc "${OPT#-}-${VAR}" "RNG" "stm32u0xx_hal_rng stm32u0xx_hal_rng_ex" || exit 1
BID="nucleo-u083rc-pqc${OPT}-${VAR}-$(date +%Y%m%d%H%M%S)"
INC+=("-I$PJ" "-I$ML" "-I$MD")
MLF=(-DMLK_CONFIG_PARAMETER_SET=768 -DMLK_CONFIG_NO_RANDOMIZED_API)
MDF=(-DMLD_CONFIG_PARAMETER_SET=44 -DMLD_CONFIG_NO_RANDOMIZED_API); case "$VAR" in *lowram) MDF+=(-DMLD_CONFIG_REDUCE_RAM);; esac
KEM=1; DSA=1; case "$VAR" in kem) DSA=0;; dsa-*) KEM=0;; esac
cc() { "$CC" $ARCH -ffunction-sections -fdata-sections -g "${INC[@]}" "${DEF[@]}" "$@" 2>>"$OUT/build.log" || { echo "FAIL compile $*, see $OUT/build.log"; exit 1; }; }
compile() { local src="$1" obj="$2"; shift 2; cc "$@" -c "$src" -o "$OUT/$obj.o"; OBJS+=("$OUT/$obj.o"); }
FLAGS_BOARD=(-O2 "${MLF[@]}" -DPQC_BUILD_ID="\"$BID\"" -DPQC_OPT="\"$OPT\"")
[ $DSA = 0 ] && FLAGS_BOARD+=(-DPQC_NO_DSA)
[ $KEM = 0 ] && FLAGS_BOARD+=(-DPQC_NO_KEM)
link_with() {   # link_with <stack>: writes $OUT/app.elf; prints the linker's overflow line on failure
  sed -e "s/LENGTH = 32K/LENGTH = 40K/" -e "s/^_Min_Stack_Size = .*/_Min_Stack_Size = $1;/" "$LDS" > "$OUT/pqc.ld"
  "$CC" $ARCH -T "$OUT/pqc.ld" -Wl,--gc-sections -Wl,-Map,"$OUT/app.map" --specs=nano.specs --specs=nosys.specs "${OBJS[@]}" "$OUT/board_u083.o" -lc -lm -lgcc -o "$OUT/app.elf" > "$OUT/link.log" 2>&1
}
# the board file is compiled per stack size (PQ_STACK_BYTES must equal _Min_Stack_Size)
build_board() { rm -f "$OUT/board_u083.o"; cc "${FLAGS_BOARD[@]}" -DPQ_STACK_BYTES="$1" -c "$HERE/board_u083.c" -o "$OUT/board_u083.o"; }
if [ $KEM = 1 ]; then compile "$ML/mlkem_native.c" mlkem "$OPT" "${MLF[@]}"; compile "$PJ/pqc_join.c" join "$OPT" "${MLF[@]}"; fi
compile "$PJ/pqc_sym.c" sym "$OPT"
if [ $DSA = 1 ]; then compile "$MD/mldsa_native.c" mldsa "$OPT" "${MDF[@]}"; compile "$PJ/pqc_sign.c" sign "$OPT" "${MDF[@]}"; fi
U0_TAG=_main compile "$ST_COMMON/bench_main.c" main -Os -DBENCH_APP=pqc_main
echo "objects (text data bss):"; "$(dirname "$CC")/arm-none-eabi-size" "$OUT"/sym.o $( [ $KEM = 1 ] && echo "$OUT/mlkem.o $OUT/join.o" ) $( [ $DSA = 1 ] && echo "$OUT/mldsa.o $OUT/sign.o" ) | sed 's|'"$OUT"'/||'
if [ "$STK" = max ]; then
  s=30720; : > "$OUT/search.log"
  for i in 1 2 3 4 5 6; do
    build_board $s; link_with $s && break
    ov=$(grep -o "overflowed by [0-9]* bytes" "$OUT/link.log" | head -1 | grep -o "[0-9]*"); [ -n "$ov" ] || { cat "$OUT/link.log" | tail -5; exit 3; }
    echo "stack $s overflows by $ov" >> "$OUT/search.log"; s=$(( (s - ov) / 256 * 256 ))
  done
  STK=$s
fi
build_board "$STK"
if ! link_with "$STK"; then
  ov=$(grep -o "overflowed by [0-9]* bytes" "$OUT/link.log" | head -1)
  echo "LINK FAILED at stack $STK: $(grep -o "region .[A-Z]*. overflowed by [0-9]* bytes" "$OUT/link.log" | head -2 | tr '\n' ';')"; echo "shortfall_bytes=$(echo "$ov" | grep -o '[0-9]*')"
  find "$OUT" -maxdepth 1 -name '*.o' -type f -delete; exit 2
fi
find "$OUT" -maxdepth 1 -name '*.o' -type f -delete
echo "built $NAME stack=$STK  $(st_size "$CC" "$OUT/app.elf")"
echo "image: $OUT/app.elf"
nm="$(dirname "$CC")/arm-none-eabi-nm"; echo "pq at $("$nm" "$OUT/app.elf" | awk '$3=="pq"{print $1}')  pqlog at $("$nm" "$OUT/app.elf" | awk '$3=="pqlog"{print $1}')"
