#!/bin/bash
# Build the post-quantum KAT and timing image for the LP-EM-CC2340R5 (CC2340R52, Cortex-M0+, 48 MHz, 36 kB RAM) from the terminal.
# BUILD ONLY: never touches a probe or a board.
#   ./build.sh kem|dsa|dsa-nosign [-Os|-O3|-O2]      STACK=<bytes>|auto (default auto: all the RAM the static data leaves)
#     kem          ML-KEM-768 known-answer tests and timing, no ML-DSA
#     dsa          ML-DSA-44 (-DMLD_CONFIG_REDUCE_RAM) keygen, sign, verify
#     dsa-nosign   ML-DSA-44 (-DMLD_CONFIG_REDUCE_RAM) keygen and verify only
# -> $OUT/<mode><opt>/image.out, image.hex, image.map, build.log, ccfg.txt   (OUT default: ./out)
# Tools: SimpleLink Low Power F3 SDK 9.21.00.36, TI Arm Clang 5.1.1, SysConfig 1.26.3 (SYSCONFIG_DIR if it is elsewhere).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../../../.." && pwd)"
MODE="${1:?usage: build.sh kem|dsa|dsa-nosign [-Os|-O3|-O2]}"; OPT="${2:--Os}"; STACK="${STACK:-auto}"
SDK=/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS
SYSCFG_TOOL=""; for d in "${SYSCONFIG_DIR:-}" /Applications/ti/sysconfig_1.26.3 /Volumes/Seagate/ti/sysconfig_1.26.3; do [ -x "$d/sysconfig_cli.sh" ] && SYSCFG_TOOL="$d/sysconfig_cli.sh" && break; done
[ -n "$SYSCFG_TOOL" ] || { echo "SysConfig 1.26.3 not found: set SYSCONFIG_DIR" >&2; exit 2; }
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS; CLANG="$CGT/bin/tiarmclang"
ARCH="-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft"; BENCH="$REPO/cc2340r5/bench"; LIB="$BENCH/lib"
OUT="${OUT:-$PWD/out}"; B="$OUT/$MODE$OPT"; G="$OUT/gen"; rm -rf "$B"; mkdir -p "$B" "$G" "$OUT/board-h"
if [ ! -f "$G/ti_drivers_config.c" ] || ! cmp -s "$HERE/pqc.syscfg" "$G/pqc.syscfg"; then
  rm -rf "$G"; mkdir -p "$G"; cp "$HERE/pqc.syscfg" "$G/"
  "$SYSCFG_TOOL" --compiler ticlang --product "$SDK/.metadata/product.json" --output "$G" "$HERE/pqc.syscfg" > "$G/sysconfig.log" 2>&1 || { echo "SYSCONFIG FAILED"; tail -8 "$G/sysconfig.log"; exit 1; }
fi
python3 "$REPO/portability/boards/gen_board_h.py" cc2340r5 "$OUT/board-h/bench_board.h" || exit 1
PJ="$REPO/portability/pqc-join/core"; ML="$REPO/third-party/mlkem-native/mlkem"; MD="$REPO/third-party/mldsa-native/mldsa"
BID="cc2340r5-pqc-$MODE$OPT-$(date +%Y%m%d%H%M%S)"
COMMON=(-mthumb $ARCH "@$G/ti_utils_build_compiler.opt" -I"$G" -I"$LIB" -I"$OUT/board-h" "-I$SDK/source" "-I$SDK/kernel/nortos" "-I$SDK/kernel/nortos/posix" -gdwarf-3 -Wall)
MLF=(-DMLK_CONFIG_PARAMETER_SET=768 -DMLK_CONFIG_NO_RANDOMIZED_API -I"$ML" -I"$PJ")
MDF=(-DMLD_CONFIG_PARAMETER_SET=44 -DMLD_CONFIG_NO_RANDOMIZED_API -DMLD_CONFIG_REDUCE_RAM -I"$MD" -I"$PJ")
case "$MODE" in kem) BD=(-DPQC_NO_DSA);; dsa) BD=();; dsa-nosign) BD=(-DPQC_DSA_NOSIGN);; *) echo "unknown mode $MODE"; exit 2;; esac
cd "$B"; : > build.log; ok=1
cc1() { echo "$CLANG $*" >> build.log; "$CLANG" "$@" >> build.log 2>&1 || ok=0; }
for f in "$BENCH/board/main_nortos.c" "$G"/ti_*.c; do [ -f "$f" ] && cc1 -O2 "${COMMON[@]}" -c "$f" -o "$(basename "${f%.c}").o"; done
cc1 -O2 "${COMMON[@]}" "${MLF[@]}" -I"$MD" ${BD[@]+"${BD[@]}"} -DPQC_BUILD_ID="\"$BID\"" -DPQC_OPT="\"$OPT\"" -c "$HERE/pqc_board.c" -o pqc_board.o
cc1 "$OPT" "${COMMON[@]}" "${MLF[@]}" -fdata-sections -c "$PJ/pqc_join.c" -o pqc_join.o   # -fdata-sections + unused-section elimination drop the join buffers
cc1 "$OPT" "${COMMON[@]}" -c "$PJ/pqc_sym.c" -o pqc_sym.o
if [ "$MODE" = kem ]; then cc1 "$OPT" "${COMMON[@]}" "${MLF[@]}" -c "$ML/mlkem_native.c" -o mlkem_native.o
else cc1 "$OPT" "${COMMON[@]}" "${MDF[@]}" -c "$MD/mldsa_native.c" -o mldsa_native.o; cc1 "$OPT" "${COMMON[@]}" "${MDF[@]}" -c "$PJ/pqc_sign.c" -o pqc_sign.o; fi
link() {   # link <stack bytes>
  sed -E "s/^--stack_size=[0-9]+/--stack_size=$1/; s/^--heap_size=[0-9]+/--heap_size=0/" "$BENCH/board/lpf3_nortos.cmd" > linker.cmd
  "$CLANG" $ARCH -Wl,-u,_c_int00 ./*.o -Wl,--unused_section_elimination=on "-L$SDK/source" "-L$SDK/kernel/nortos" "-Wl,--search_path=$G" \
    "$G/ti_utils_build_linker.cmd.genlibs" ./linker.cmd -Wl,-m,image.map -Wl,--rom_model -Wl,--warn_sections "-L$CGT/lib" -llibc.a -o image.out >> build.log 2>&1
}
sz() { python3 -c "import re,sys;m=open('image.map').read();r=re.search(r'^\s+%s\s+(\w+)\s+(\w+)\s+(\w+)'%sys.argv[1],m,re.M);print(int(r.group(2),16),int(r.group(3),16))" "$1"; }
if [ $ok = 1 ]; then
  AUTO=0; if [ "$STACK" = auto ]; then AUTO=1
    link 1024 || echo "(probe link at 1 kB stack failed)" >> build.log
    if [ -f image.out ]; then read RAMLEN RAMUSED <<< "$(sz SRAM)"; STACK=$(( (RAMLEN - (RAMUSED - 1024)) / 8 * 8 )); echo "auto stack: RAM $RAMLEN, static $((RAMUSED-1024)), stack $STACK" >> build.log; else STACK=0; fi
    rm -f image.out image.map
  fi
  if [ "$AUTO" = 1 ]; then for i in $(seq 40); do link "$STACK" && break; STACK=$((STACK-64)); done; fi   # the linker leaves a little slack for alignment
  [ -f image.out ] || link "$STACK" || ok=0
fi
[ $ok = 1 ] && { "$SDK/tools/common/crc_tool/crc_tool" patch-image --elf image.out --symbol-prefix ti_utils_build_GenMap_sym_CRC --output image.out >> build.log 2>&1 || ok=0; }
[ $ok = 1 ] && { "$CGT/bin/tiarmobjcopy" -O ihex image.out image.hex >> build.log 2>&1 || ok=0; }
if [ $ok = 0 ]; then echo "BUILD FAILED (stack $STACK)"; grep -iE "error|undefined|overflow|does not fit" build.log | head -8; exit 1; fi
cfg="$G/ti_devices_config.c"
for p in allowDebugPort allowEnergyTrace allowFlashVerify allowFlashProgram allowChipErase allowToolsClientMode allowReturnToFactory; do
  grep -qE "\.$p[[:space:]]*=[[:space:]]*CCFG_PERMISSION_ALLOW" "$cfg" || { echo "CCFG: $p is not ALLOW"; exit 1; }; done
echo "cc23 CCFG: all permission fields = CCFG_PERMISSION_ALLOW" > ccfg.txt; echo "$BID" > build-id.txt
read RAMLEN RAMUSED <<< "$(sz SRAM)"; read FLLEN FLUSED <<< "$(sz FLASH)"
echo "flash used $FLUSED of $FLLEN; RAM used $RAMUSED of $RAMLEN (stack $STACK, static $((RAMUSED-STACK)))"
"$CGT/bin/tiarmsize" image.out | tail -1
rm -f ./*.o; echo "built $B/image.out ($BID)"
