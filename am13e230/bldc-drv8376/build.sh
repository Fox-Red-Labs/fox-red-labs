#!/bin/bash
# Build and flash the DRV8376EVM brushless motor demo for the LP-AM13E230 from the terminal (same toolchain and flags as ../blink.sh).
#   ./build.sh            build only        ./build.sh --flash [probe-serial]   build, then flash over the onboard XDS110
# Output: out/am13e230/bldc-drv8376/bldc.out (outside the repo).
#
# Tools, in this order: FRL_AM13_SDK, FRL_SYSCONFIG, FRL_TICLANG and FRL_DSLITE if set; else the TI install folders under
# /Applications/ti (or FRL_TI_ROOT): the AM13E230x SDK 26.01, SysConfig 1.28, TI Arm Clang 5.1.1.LTS either standalone or inside
# CCStudio, and DSLite from UniFlash or CCStudio. A missing tool stops with its name and download page, not a path error.
# The flash step needs a connection file am13e230/AM13E23019_<serial>.ccxml; it is written from am13e230/tools/ccxml.template
# when it does not exist. With no serial given it takes the one XDS110 on USB that already has such a file, else the only probe.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; BOARD="$(cd "$HERE/.." && pwd)"
OUT="${OUT:-$PWD/out/am13e230}/bldc-drv8376"
TI="${FRL_TI_ROOT:-/Applications/ti}"
pick() { local v; for v in "$@"; do [ -e "$v" ] && { echo "$v"; return; }; done; }
missing() { echo "Not installed: $1"; echo "  get it: $2"; echo "  or set $3 to where it is"; exit 2; }
SDK="${FRL_AM13_SDK:-$(pick "$TI"/am13e230x_sdk_26_01_* "$TI"/am13e230x_sdk_*)}"
[ -n "$SDK" ] || missing "the AM13E230x SDK 26.01.00.03" "https://www.ti.com/tool/LP-AM13E230 (free, myTI login; about 0.4 GB)" FRL_AM13_SDK
SYSCFG_PATH="${FRL_SYSCONFIG:-$(pick "$TI"/sysconfig_1.28.* "$TI"/sysconfig_1.2[89]* "$TI"/sysconfig_*)}"
[ -n "$SYSCFG_PATH" ] || missing "SysConfig 1.28.0" "https://www.ti.com/tool/SYSCONFIG (free; about 0.15 GB)" FRL_SYSCONFIG
NODE="$SYSCFG_PATH/nodejs/node"
CC="${FRL_TICLANG:-$(pick "$TI"/ti-cgt-armllvm_5.1.1* "$TI"/ti-cgt-armllvm_* "$TI"/ccs*/ccs/tools/compiler/ti-cgt-armllvm_5.1.1* "$TI"/ccs*/ccs/tools/compiler/ti-cgt-armllvm_*)}"
[ -n "$CC" ] || missing "TI Arm Clang 5.1.1.LTS" "https://www.ti.com/tool/ARM-CGT (free; about 0.3 GB), or CCStudio which includes it" FRL_TICLANG
rm -rf "$OUT"; mkdir -p "$OUT/gen" "$OUT/obj"
t0=$(python3 -c 'import time;print(time.time())')
"$NODE" "$SYSCFG_PATH/dist/cli.js" --product "$SDK/.metadata/product.json" --quiet \
  --output "$OUT/gen" "$HERE/bldc.syscfg" > "$OUT/sysconfig.log" 2>&1 || { echo "SysConfig failed: $OUT/sysconfig.log"; cat "$OUT/sysconfig.log"; exit 1; }
ARCH="-mcpu=cortex-m33 -march=armv8.1-m.main+cdecp0 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard"
INC="-I$SDK/source/compiler/m33_ti_arm_clang -I$SDK/source/device/am13e230x/include -I$SDK/source/device/am13e230x/include/hw \
 -I$SDK/ti_sdk_config/am13e230x/default/device_support/include -I$SDK/source/arch/include -I$SDK/source/arch/m33/include \
 -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg/m33 -I$SDK/source/cmsis/Core/Include \
 -I$SDK/source/driverlib/am13e230x -I$SDK/source/utils/log/include -I$SDK/ti_sdk_config/am13e230x/default/utils_cfg -I$SDK/source \
 -I$OUT/gen -I$SDK/examples/empty/am13e230x_lp"
LIBS="-Wl,-i$SDK/build/am13e230x/lib/Release -llibti_sdk_cfg_default_m33_ti_arm_clang.a -llibarch_ti_sdk_cfg_default_m33_ti_arm_clang.a \
 -llibdriverlib_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibutils_nortos_ti_sdk_cfg_default_m33_ti_arm_clang.a -llibam13e230x_m33_ti_arm_clang.a"
for f in "$HERE/main.c" "$OUT"/gen/*.c; do
  "$CC/bin/tiarmclang" -O2 $ARCH -g -Wall $INC -c "$f" -o "$OUT/obj/$(basename "${f%.c}").o" >> "$OUT/build.log" 2>&1 || { echo "Compile failed: $OUT/build.log"; cat "$OUT/build.log"; exit 1; }
done
( cd "$OUT/obj" && "$CC/bin/tiarmclang" $ARCH ./*.o $LIBS "$SDK/ti_sdk_config/am13e230x/default/linker_m33_ti_arm_clang.cmd" \
  -Wl,--rom_model -Wl,--reread_libs -Wl,--diag_suppress=10063 -Wl,-m,"$OUT/bldc.map" "-L$CC/lib" -llibc.a -o "$OUT/bldc.out" ) >> "$OUT/build.log" 2>&1 \
  || { echo "Link failed: $OUT/build.log"; cat "$OUT/build.log"; exit 1; }
t1=$(python3 -c 'import time;print(time.time())')
echo "built $OUT/bldc.out in $(python3 -c "print(f'{$t1-$t0:.1f}')") s: $(grep -E '^ +(RAM_S|RAM_C|FLASH_BANK0) ' "$OUT/bldc.map" | awk '{print $1, "used", $4}' | tr '\n' ' ')"
if [ "${1:-}" = "--flash" ]; then
  DSLITE="${FRL_DSLITE:-$(pick "$TI"/uniflash_*/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite "$TI"/ccs*/ccs/ccs_base/DebugServer/bin/DSLite)}"
  [ -n "$DSLITE" ] || missing "DSLite (the command-line flasher inside UniFlash or CCStudio)" "https://www.ti.com/tool/UNIFLASH (free; about 0.5 GB)" FRL_DSLITE
  XDSDFU="$(pick "$TI"/ccs*/ccs/ccs_base/common/uscif/xds110/xdsdfu "$TI"/uniflash_*/deskdb/content/TICloudAgent/osx/ccs_base/common/uscif/xds110/xdsdfu)"
  SER="${2:-}"
  if [ -z "$SER" ]; then   # the probe: one that already has a connection file here, else the only XDS110 on USB
    ON_USB="$( [ -n "$XDSDFU" ] && "$XDSDFU" -e 2>/dev/null | sed -n 's/.*Serial Num:[[:space:]]*\([A-Za-z0-9]*\).*/\1/p' )"
    for c in "$BOARD"/AM13E23019_*.ccxml; do s=$(basename "$c" .ccxml); s=${s#AM13E23019_}; echo "$ON_USB" | grep -qx "$s" && SER=$s && break; done
    [ -n "$SER" ] || { n=$(echo "$ON_USB" | grep -c .); [ "$n" = 1 ] && SER=$ON_USB; }
    [ -n "$SER" ] || { echo "Which probe? $n XDS110 probes are on USB:"; echo "$ON_USB" | sed 's/^/  /'; echo "run ./build.sh --flash <serial> (the LaunchPad kit's serial is in its USB name: ls /dev/cu.usbmodem*)"; exit 2; }
  fi
  CCXML="$BOARD/AM13E23019_$SER.ccxml"
  [ -e "$CCXML" ] || { sed -e "s/@SERIAL@/$SER/g" -e "s/@DEV@/AM13E23019/g" "$BOARD/tools/ccxml.template" > "$CCXML"; echo "wrote $CCXML for probe $SER"; }
  "$DSLITE" flash -c "$CCXML" -e -f -v -r 1 -u -t 60 "$OUT/bldc.out" > "$OUT/flash.log" 2>&1; grep -q "verification successful" "$OUT/flash.log" || {
    echo "Flash failed: $OUT/flash.log"; tail -5 "$OUT/flash.log"
    grep -q -- "-61[45]" "$OUT/flash.log" && echo "Error -614 or -615 means the chip gave no debug answer: unplug the LaunchPad kit's USB-C cable, plug it back in, run again (am13e230/FAQ.md)."
    exit 1; }
  t2=$(python3 -c 'import time;print(time.time())')
  echo "flashed in $(python3 -c "print(f'{$t2-$t1:.1f}')") s; the motor program is running; open the serial port at 115200 and press ? for help"
fi
