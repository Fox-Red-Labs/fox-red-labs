#!/bin/bash
# Build the post-quantum KAT + timing image for an MSPM0 Cortex-M0+ LaunchPad from the terminal. BUILD ONLY: never touches a probe.
#   ./build_pqc.sh [-O3|-Os|-O2]      -> $OUT/pqc-m0p<opt>/pqc.out, pqc.map, build.log
# Board parameters (defaults = LP-MSPM0G3519): BOARD, DEVDEF, STARTUP, LIBNAME are in the environment; the SysConfig output of the
# SDK example sysctl_mclk_syspll (80 MHz) is generated into $GEN. Stack: STACK bytes (default 61440) in the second SRAM bank.
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../../.." && pwd)"
OPT="${1:--O3}"; BOARD="${BOARD:-LP_MSPM0G3519}"; STARTUP="${STARTUP:-startup_mspm0g351x_ticlang.c}"
SDK=/Applications/ti/mspm0_sdk_2_11_00_07; SYSCFG=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh
CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS; CLANG=$CC/bin/tiarmclang
OUT="${OUT:-$PWD/out/speed-run/$(echo "$BOARD" | tr 'A-Z_' 'a-z-')}"; GEN="${GEN:-$OUT/gen/g80}"
if [ ! -f "$GEN/ti_msp_dl_config.c" ]; then mkdir -p "$GEN"; cp "$SDK/examples/nortos/$BOARD/driverlib/sysctl_mclk_syspll/"*.syscfg "$GEN/"
  "$SYSCFG" --compiler ticlang --product "$SDK/.metadata/product.json" --output "$GEN" "$(ls "$GEN"/*.syscfg | head -1)" > "$GEN/sysconfig.log" 2>&1; fi
B="$OUT/pqc-m0p$OPT"; rm -rf "$B"; mkdir -p "$B"; cd "$B"
ARCH="-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb"
PJ="$REPO/portability/pqc-join/core"; ML="$REPO/third-party/mlkem-native/mlkem"; MD="$REPO/third-party/mldsa-native/mldsa"
BID="$BOARD-pqc$OPT-$(date +%Y%m%d%H%M%S)"
COMMON="$ARCH @$GEN/device.opt -I$GEN -I$HERE -I$REPO/mspm0g3519/bench/common -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source -gdwarf-3 -Wall"
MLF="-DMLK_CONFIG_PARAMETER_SET=768 -DMLK_CONFIG_NO_RANDOMIZED_API -I$ML -I$PJ"; MDF="-DMLD_CONFIG_PARAMETER_SET=44 -DMLD_CONFIG_NO_RANDOMIZED_API -I$MD -I$PJ"
: > build.log
cc() { echo "$CLANG $*" >> build.log; "$CLANG" "$@" >> build.log 2>&1; }
cc -O2 $COMMON $MLF -I$MD ${NO_DSA:+-DPQC_NO_DSA} ${PQC_DEFS:-} ${PQLOG:+-DPQLOG_BYTES=$PQLOG} -DPQC_BUILD_ID="\"$BID\"" -DPQC_OPT="\"$OPT\"" -c "$HERE/pqc_board.c" -o pqc_board.o
cc -O2 $COMMON -c "$GEN/ti_msp_dl_config.c" -o cfg.o
cc -O2 $COMMON -c "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/$STARTUP" -o startup.o
cc $OPT $COMMON $MLF -c "$ML/mlkem_native.c" -o mlkem_native.o
cc $OPT $COMMON $MLF ${EXTRA_CC:-} -c "$PJ/pqc_join.c" -o pqc_join.o   # EXTRA_CC=-fdata-sections: with EXTRA_LD=-Wl,--unused_section_elimination=on drops the unused join buffers (7.3 kB) on a 32 kB part
cc $OPT $COMMON -c "$PJ/pqc_sym.c" -o pqc_sym.o
if [ -z "${NO_DSA:-}" ]; then   # NO_DSA=1: ML-KEM only (a 32 kB RAM part)
cc $OPT $COMMON $MDF -c "$MD/mldsa_native.c" -o mldsa_native.o
cc $OPT $COMMON $MDF -c "$PJ/pqc_sign.c" -o pqc_sign.o
fi
# linker file: the SysConfig one with the stack in SRAM_BANK1 (64 kB bank; the 61,440 B stack leaves 4 kB there)
sed -E "s/--stack_size=[0-9]+/--stack_size=${STACK:-61440}/; s/^(SECTIONS)/\1/" "$GEN/device_linker.cmd" | \
  if [ -n "${NO_BANK:-}" ]; then cat > linker.cmd; else awk '/\.sysmem/ {print; print "    .stack  :   > SRAM_BANK1 (HIGH)"; next} {print}' > linker.cmd; fi   # NO_BANK=1: one SRAM, stack stays in it
"$CLANG" $ARCH -Wl,-u,_c_int00 ./*.o ${EXTRA_LD:-} -l"$GEN/device.cmd.genlibs" "-L$SDK/source" ./linker.cmd -Wl,-m,pqc.map -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc.a -o pqc.out >> build.log 2>&1 \
  || { echo "BUILD FAILED"; grep -iE "error|undefined|overflow" build.log | head -15; exit 1; }
grep -E '^ +(FLASH|SRAM_BANK0|SRAM_BANK1) ' pqc.map; "$CC/bin/tiarmsize" pqc.out | tail -1; echo "built $B/pqc.out ($BID)"
