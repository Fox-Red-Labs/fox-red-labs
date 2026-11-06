#!/bin/bash
# Build-only: TI basic_ble -> FRL-GP1 gamepad (ESP32-BLE-Gamepad generic mode) for the LP-EM-CC2340R5.
# Never flashes. Everything is generated under ./build from the SDK example plus files in this folder.
set -euo pipefail
SDK=${SDK:-/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS}
SYSCFG=${SYSCFG:-/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh}
CLANG=${CLANG:-/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS}
HERE="$(cd "$(dirname "$0")" && pwd)"
# LIVE=1: the board's two buttons drive gamepad buttons 1 and 2, own name and maker string.
# Default: the library example's fixed pattern and names, which the golden check compares.
LIVE=${LIVE:-0}
if [ "$LIVE" = 1 ]; then GP_NAME="CC2340R5 Gamepad"; else GP_NAME="ESP32 BLE Gamepad"; fi
EX=$SDK/examples/rtos/LP_EM_CC2340R5/ble/basic_ble
rm -rf "$HERE/build"; mkdir -p "$HERE/build"
rsync -a "$EX/" "$HERE/build/"
cp "$HERE/board/hid_gamepad.c" "$HERE/board/hid_gamepad.h" "$HERE/core/"*.c "$HERE/core/"*.h "$HERE/build/app/"
cd "$HERE/build"
patch -p1 -s < "$HERE/board/basic_ble.base.patch"
sed -i '' -e 's/hid_remote\.h/hid_gamepad.h/; s/HidRemote_start/HidGamepad_start/' app/app_main.c
S=freertos/basic_ble.syscfg
sed -i '' -e "s/ble.deviceName = \"FRL Remote\";/ble.deviceName = \"${GP_NAME}\";/" \
  -e 's/appearanceValue = 0x03C0;/appearanceValue = 0x03C4;/' \
  -e 's/numOfUUIDs16Complete = 2;/numOfUUIDs16Complete = 1;/' \
  -e '/UUID116Complete = 0x180F;/d' \
  -e 's/displayUART.enableANSI = true;/displayUART.enableANSI = false;/' \
  -e 's/displayUART.uartBufferSize = 128;/displayUART.uartBufferSize = 300;/' $S
M=freertos/ticlang/makefile
python3 - "$M" <<'PY'
import sys,re
p=sys.argv[1]; s=open(p).read()
s=s.replace("app_hid_remote.obj common_Services_battery_battery_server.obj","app_hid_gamepad.obj app_frl_gamepad.obj app_frl_gp_check.obj common_Services_battery_battery_server.obj",1)
old="app_hid_remote.obj: ../../app/hid_remote.c $(SYSCFG_H_FILES) prebuild\n\t@ echo Building $@\n\t$(V) $(CC) $(CFLAGS) -c $< -o $@\n"
assert old in s
new="".join("app_%s.obj: ../../app/%s.c $(SYSCFG_H_FILES) prebuild\n\t@ echo Building $@\n\t$(V) $(CC) $(CFLAGS) -c $< -o $@\n\n"%(n,n) for n in ("hid_gamepad","frl_gamepad","frl_gp_check"))
s=s.replace(old,new.rstrip("\n")+"\n",1)
open(p,"w").write(s)
PY
grep -c "$GP_NAME" $S
cd freertos/ticlang
EXTRA=""; [ "$LIVE" = 1 ] && EXTRA="-DFRL_LIVE"
printf '\nCFLAGS += $(CFLAGS_EXTRA)\n' >> makefile
make -j4 CFLAGS_EXTRA="$EXTRA" SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CLANG"
grep -A8 "MEMORY CONFIGURATION" basic_ble.map | grep -E "FLASH|SRAM|NAME"
echo "built: $PWD/basic_ble.hex (NOT flashed)"
