#!/bin/bash
# Build the classical crypto timing image (SHA-256, ECDSA P-256, ECDH P-256) for an MSPM0 Cortex-M0+ LaunchPad. BUILD ONLY.
#   ./build_sec.sh [-O2|-O3] [ti|uecc-asm|uecc-c]   -> $OUT/sec-<engine>-m0p<opt>/sec.out     (same board variables as build_pqc.sh)
#   ti: TI MSP Crypto Library (SHA-256, ECDSA, ECDH); uecc-asm / uecc-c: micro-ecc with its Thumb-1 asm or portable C
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../../.." && pwd)"
OPT="${1:--O2}"; ENG="${2:-ti}"; BOARD="${BOARD:-LP_MSPM0G3519}"; STARTUP="${STARTUP:-startup_mspm0g351x_ticlang.c}"; LIBDIR="${LIBDIR:-mspm0gx51x}"
SDK=/Applications/ti/mspm0_sdk_2_11_00_07; CC=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS; CLANG=$CC/bin/tiarmclang
OUT="${OUT:-$PWD/out/speed-run/$(echo "$BOARD" | tr 'A-Z_' 'a-z-')}"; GEN="${GEN:-$OUT/gen/g80}"
[ -f "$GEN/ti_msp_dl_config.c" ] || { echo "run build_pqc.sh first (it generates $GEN)"; exit 1; }
B="$OUT/sec-$ENG-m0p$OPT"; rm -rf "$B"; mkdir -p "$B"; cd "$B"
ARCH="-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb"; PJ="$REPO/portability/pqc-join/core"
BID="$BOARD-sec$OPT-$(date +%Y%m%d%H%M%S)"
COMMON="$ARCH @$GEN/device.opt -I$GEN -I$HERE -I$REPO/mspm0g3519/bench/common -I$PJ -I$SDK/source/third_party/CMSIS/Core/Include -I$SDK/source -DCRYPTO_USE_SDK_HSF -DCRYPTO_EC_WIN=3 -DCRYPTO_HW_USE_MSPM0_AESADV=1 -DCRYPTO_HW_USE_MSPM0_TRNG=1 -gdwarf-3 -Wall"
: > build.log
cc() { echo "$CLANG $*" >> build.log; "$CLANG" "$@" >> build.log 2>&1; }
cc -O2 $COMMON -c "$GEN/ti_msp_dl_config.c" -o cfg.o
cc -O2 $COMMON -c "$SDK/source/ti/devices/msp/m0p/startup_system_files/ticlang/$STARTUP" -o startup.o
cc $OPT $COMMON -c "$PJ/pqc_sym.c" -o pqc_sym.o
LIBS=""
if [ "$ENG" = ti ]; then
  cc -O2 $COMMON -DSEC_BUILD_ID="\"$BID\"" -c "$HERE/sec_board.c" -o sec_board.o
  LIBS="$SDK/source/ti/crypto/lib/ticlang/m0p/$LIBDIR/cryptolib.a"
else
  UE="$REPO/mspm0g3507/third-party-bench/vendor/micro-ecc"; UF="-DuECC_SUPPORTS_secp160r1=0 -DuECC_SUPPORTS_secp192r1=0 -DuECC_SUPPORTS_secp224r1=0 -DuECC_SUPPORTS_secp256k1=0 -DuECC_SUPPORT_COMPRESSED_POINT=0"
  if [ "$ENG" = uecc-asm ]; then mkdir -p mecc; for x in "$UE"/*.inc "$UE"/*.h "$UE"/uECC.c; do sed 's/\.syntax divided/.syntax unified/' "$x" > mecc/$(basename "$x"); done; UD=mecc
  else UD="$UE"; UF="$UF -DuECC_PLATFORM=uECC_arch_other -DuECC_WORD_SIZE=4"; fi
  cc $OPT $COMMON $UF -I$UD -w -c "$UD/uECC.c" -o uECC.o
  cc -O2 $COMMON $UF -I$UD -DSEC_BUILD_ID="\"$BID-$ENG\"" -c "$HERE/sec_uecc.c" -o sec_board.o
  rm -rf mecc
fi
sed -E "s/--stack_size=[0-9]+/--stack_size=${STACK:-16384}/" "$GEN/device_linker.cmd" > linker.cmd
"$CLANG" $ARCH -Wl,-u,_c_int00 ./*.o $LIBS -l"$GEN/device.cmd.genlibs" "-L$SDK/source" ./linker.cmd -Wl,-m,sec.map -Wl,--rom_model -Wl,--warn_sections "-L$CC/lib" -llibc.a -o sec.out >> build.log 2>&1 \
  || { echo "BUILD FAILED"; grep -iE "error|undefined|overflow" build.log | head -15; exit 1; }
"$CC/bin/tiarmsize" sec.out | tail -1; echo "built $B/sec.out ($BID)"
