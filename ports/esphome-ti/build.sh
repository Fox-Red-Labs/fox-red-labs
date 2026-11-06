#!/bin/bash
# E105 link test: TI's Zigbee onoff_light build (TI Arm Clang, FreeRTOS, F3 SDK 9.21.00.36, LP-EM-CC2755P10) with the C++ that
# ESPHome generates for frl-esphome-fit.yaml, plus the small shim in ./shim. BUILD ONLY: touches no probe, port or board.
#   ./build.sh [work dir]      default out/esphome-ti-build
#   BOARD=LP_EM_CC2340R5 ./build.sh [work dir]    the same link for the CC2340R5 (Cortex-M0+, 36 kB RAM); default LP_EM_CC2755P10
# Result: <work>/out/esphome_fit.out, .map, .hex (hex has the CRC patch TI's makefile applies). Prints LINK_OK and the memory table.
set -u
H="$(cd "$(dirname "$0")" && pwd)"
W="${1:-$PWD/out/esphome-ti-build}"; mkdir -p "$W"
SDK=/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
SYSCFG=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh
ESPHOME="${ESPHOME:-$HOME/.venvs/esphome/bin/esphome}"
BOARD="${BOARD:-LP_EM_CC2755P10}"; ROLE="${ROLE:-zr}"   # ROLE=zr (router, default) or zed (end device)
EX="$SDK/examples/rtos/$BOARD/zigbee/onoff_light"
if [ "$BOARD" = LP_EM_CC2340R5 ]; then
  ARCH=(-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft -mthumb); PORT=ARM_CM0; SYSCFGSRC="$H/components/cc27xx/ti/esphome_fit_cc2340r5.syscfg"
else
  ARCH=(-mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16); PORT=ARM_CM33_NTZ/non_secure; SYSCFGSRC="$H/components/cc27xx/ti/esphome_fit.syscfg"
fi
CC="$CGT/bin/tiarmclang"
OUT="$W/out"; GEN="$W/gen"; mkdir -p "$OUT" "$GEN" "$W/ti"
SYSCFGFILE="$W/esphome_fit.syscfg"; YAML=frl-esphome-fit.yaml; ZROLE=ROUTER
if [ "$ROLE" = zed ]; then sed 's/\["zr"\]/["zed"]/' "$SYSCFGSRC" > "$SYSCFGFILE"; YAML=frl-esphome-fit-ed.yaml; ZROLE=END_DEVICE; else cp "$SYSCFGSRC" "$SYSCFGFILE"; fi
# 1. ESPHome generates its C++ (code generation only, nothing is compiled by ESPHome)
rm -rf "$W/esphome"; mkdir -p "$W/esphome"; cp "$H/$YAML" "$W/esphome/frl-esphome-fit.yaml"
( cd "$W/esphome" && "$ESPHOME" compile --only-generate frl-esphome-fit.yaml > "$W/generate.log" 2>&1 ) || { echo "ESPHome generate failed"; tail "$W/generate.log"; exit 1; }
SRC0="$W/esphome/.esphome/build/frl-esphome-fit/src"
# 2. TI's configuration for a Zigbee router without logging
cp -R "$EX/osif" "$EX/dmm" "$W/ti/"
"$SYSCFG" --compiler ticlang --product "$SDK/.metadata/product.json" --output "$GEN" "$SYSCFGFILE" > "$W/sysconfig.log" 2>&1 || { echo "SysConfig failed"; tail "$W/sysconfig.log"; exit 1; }
cp "$EX/freertos/ticlang/lpf3_zigbee_freertos.cmd" "$GEN/"
# DIAG_RAM_SIZE=0x19000 ./build.sh: pretend the chip has more RAM so an image that does not fit still links and its map shows by how much
[ -n "${DIAG_RAM_SIZE:-}" ] && sed -i.bak "s/\(GenMap_RAM0_SIZE \)0x[0-9a-fA-F]*/\1$DIAG_RAM_SIZE/" "$GEN/ti_utils_build_linker.cmd.genmap"
# 3. overlay: copy ESPHome's generated tree, then lay the shim headers over it (quote-includes look in the includer's directory first)
SRC="$OUT/src"; rm -rf "$SRC"; cp -R "$SRC0" "$SRC"
(cd "$H/components/cc27xx/shim" && find . -name "*.h" | while read f; do mkdir -p "$(dirname "$SRC/$f")"; cp "$f" "$SRC/$f"; done)
COMMON=("${ARCH[@]}" -Oz -flto -gdwarf-3 -Wall
 -Wno-gnu-folding-constant -Wno-unaligned-access -Wno-unused-variable -Wno-unused-function -Wno-sign-compare
 @"$GEN/ti_utils_build_compiler.opt" -DSYNC_WRITE -DZB_TI_F3_ZBOSS -DZB_DEFAULT_VENDOR
 -I"$GEN" -I"$W/ti"
 -I"$SDK/source" -I"$SDK/source/ti/zigbee/osif/include" -I"$SDK/source/ti/zigbee/include"
 -I"$SDK/source/ti/common/cc26xx/flash_interface/" -I"$SDK/kernel/freertos"
 -I"$SDK/source/third_party/freertos/include"
 -I"$SDK/source/third_party/freertos/portable/GCC/$PORT")
# C++ needs the toolchain's own errno.h ahead of TI's POSIX one, and -U_INLINE (strings.h index() does not compile as C++)
CXXF=(-std=gnu++20 -fno-exceptions -fno-rtti -U_INLINE -Wno-deprecated-declarations -DUSE_ZEPHYR -DUSE_NRF52
 -DCONFIG_ZIGBEE_ROLE_$ZROLE -I"$SRC" -idirafter "$SDK/source/ti/posix/ticlang")
POSIX=(-I"$SDK/source/ti/posix/ticlang")
fail=0; objs=()
cc() { f="$1"; shift; o="$OUT/$(echo "${f#$W/}" | tr '/.' '__').o"
  "$CC" "$@" -c "$f" -o "$o" 2> "$o.err" || { echo "FAIL $f"; head -30 "$o.err"; fail=1; return; }; objs+=("$o"); }
for f in "$W/ti/osif/ti_f3_main.c" "$W/ti/osif/ti_f3_led_buttons.c" "$W/ti/dmm/rcl_stub_dmm.c" "$H/components/cc27xx/shim/ti_glue.c" "$GEN"/*.c; do cc "$f" "${COMMON[@]}" "${POSIX[@]}"; done
SRCS=("$SRC/main.cpp")
for d in esphome/core esphome/components/gpio/switch esphome/components/sensor esphome/components/switch; do for f in "$SRC/$d"/*.cpp; do SRCS+=("$f"); done; done
SRCS+=("$SRC/esphome/components/internal_temperature/internal_temperature_common.cpp")
for f in "$SRC"/esphome/components/zigbee/*zephyr*.cpp "$H"/components/cc27xx/shim/*.cpp; do SRCS+=("$f"); done
for f in "${SRCS[@]}"; do cc "$f" "${COMMON[@]}" "${CXXF[@]}"; done
[ $fail -ne 0 ] && { echo "COMPILE FAILED"; exit 1; }
# 4. link as TI's makefile does (no --wrap: TI's linker rejects it)
( cd "$GEN" && "$CC" -Wl,-u,_c_int00 "${objs[@]}" "${COMMON[@]}" -Wl,--reread_libs "-L$SDK/source" ti_utils_build_linker.cmd.genlibs lpf3_zigbee_freertos.cmd \
  "-Wl,-m,$OUT/esphome_fit.map" -Wl,--rom_model -Wl,--warn_sections "-L$CGT/lib" -llibc.a -o "$OUT/esphome_fit.out" ) 2> "$OUT/link.err" || { echo "LINK FAILED"; head -60 "$OUT/link.err"; exit 2; }
echo LINK_OK
"$SDK/tools/common/crc_tool/crc_tool" patch-image --elf "$OUT/esphome_fit.out" --symbol-prefix ti_utils_build_GenMap_sym_CRC --output "$OUT/esphome_fit.crc.out" > "$OUT/crc.log" 2>&1
"$CGT/bin/tiarmobjcopy" -O ihex "$OUT/esphome_fit.crc.out" "$OUT/esphome_fit.hex"
grep -A8 "MEMORY CONFIG" "$OUT/esphome_fit.map" | sed -n 3,7p
