#!/bin/bash
# Copy one F3 SDK example for LP_EM_CC2340R5 and build it with TI Clang. Prints the linker memory summary.
#   ./build_example.sh rtos/ble/basic_ble [name]
set -uo pipefail
SDK=/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS
EX="$1"; NAME="${2:-$(basename "$EX")}"
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$SDK/examples/${EX%%/*}/LP_EM_CC2340R5/${EX#*/}"
[ -d "$SRC" ] || { echo "no example $SRC"; exit 2; }
rm -rf "$HERE/$NAME"; mkdir -p "$HERE/$NAME"; rsync -a "$SRC/" "$HERE/$NAME/"
MK=$(find "$HERE/$NAME" -path "*ticlang/makefile" | head -1)
cd "$(dirname "$MK")"
T0=$(date +%s)
make -j4 SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR=$SDK SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh \
  TICLANG_ARMCOMPILER=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS > "$HERE/$NAME.build.txt" 2>&1
RC=$?; T1=$(date +%s)
MAP=$(ls *.map 2>/dev/null | head -1)
echo "$NAME exit=$RC build_s=$((T1-T0)) map=$MAP"
[ -n "$MAP" ] && grep -A8 "MEMORY CONFIGURATION" "$MAP" | grep -E "^\s+(FLASH|SRAM|NVRAM|CCFG)"
grep -h -E "allowDebugPort|allowChipErase " ti_devices_config.c 2>/dev/null | tr -s ' '
