#!/bin/bash
# Recreate and build the Bluetooth LE media remote (HID over GATT) for the LP-EM-CC2340R5
# from TI's basic_ble example. Build only; the flash command is printed at the end.
#   ./build.sh            (needs the F3 SDK, SysConfig and TI Arm Clang at the paths below)
set -euo pipefail
SDK=${SDK:-/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS}
SYSCFG=${SYSCFG:-/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh}
CLANG=${CLANG:-/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS}
HERE="$(cd "$(dirname "$0")" && pwd)"
EX=$SDK/examples/rtos/LP_EM_CC2340R5/ble/basic_ble
python3 "$HERE/host/report_map.py" > /dev/null || { python3 "$HERE/host/report_map.py"; exit 1; }
rm -rf "$HERE/cc2340r5"; mkdir -p "$HERE/cc2340r5"
rsync -a "$EX/" "$HERE/cc2340r5/"
cp "$HERE/app/hid_remote.c" "$HERE/app/hid_remote.h" "$HERE/cc2340r5/app/"
( cd "$HERE/cc2340r5" && patch -p1 -s < "$HERE/basic_ble.patch" )
cd "$HERE/cc2340r5/freertos/ticlang"
make -j4 SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CLANG"
python3 "$HERE/host/verify_build.py" "$PWD"
grep -A8 "MEMORY CONFIGURATION" basic_ble.map | grep -E "FLASH|SRAM"
echo "built: $PWD/basic_ble.hex"
echo "flash: /Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c <CC2340R5.ccxml> -e -f -u $PWD/basic_ble.hex"
