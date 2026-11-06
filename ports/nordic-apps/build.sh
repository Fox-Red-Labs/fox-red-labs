#!/bin/bash
# Recreate and build "FRL Nordic" (Nordic LED Button Service + Nordic UART Service) for the LP-EM-CC2340R5
# from TI's basic_ble example, SimpleLink Low Power F3 SDK 9.21. Build only.
#   ./build.sh            (needs the F3 SDK, SysConfig and TI Arm Clang at the paths below)
set -euo pipefail
SDK=${SDK:-/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS}
SYSCFG=${SYSCFG:-/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh}
CLANG=${CLANG:-/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS}
HERE="$(cd "$(dirname "$0")" && pwd)"
EX=$SDK/examples/rtos/LP_EM_CC2340R5/ble/basic_ble
# 1. The two services come from the 9-line descriptions in services/ (tools/gatt_gen, target ti-f3)
# GATT_GEN: path to tools/gatt_gen/gatt_gen.py; without it the generated files already in src/ are used
GATT_GEN=${GATT_GEN:-$HERE/../../tools/gatt_gen/gatt_gen.py}
if [ -f "$GATT_GEN" ]; then
  for s in led_button nordic_uart; do
    python3 "$GATT_GEN" "$HERE/services/$s.json" --target ti-f3 --out "$HERE/src"
  done
else
  echo "gatt_gen.py not found: using the generated files in src/"
fi
# 2. Fresh copy of the SDK example (cp, not rsync -a: the SDK folders carry a setgid bit rsync cannot copy here)
rm -rf "$HERE/cc2340r5"
cp -R "$EX" "$HERE/cc2340r5"
chmod -R u+w "$HERE/cc2340r5"
( cd "$HERE/cc2340r5" && patch -p1 -s < "$HERE/basic_ble.patch" )
cp "$HERE/app/app_nordic.c" "$HERE/cc2340r5/app/"
# FRL_TEST_BTN=1 builds the test image whose "btn1"/"btn0" UART lines stand in for BTN-1 (see app_nordic.c)
if [ "${FRL_TEST_BTN:-0}" = 1 ]; then sed -i.bak '1i\
#define FRL_TEST_BTN 1' "$HERE/cc2340r5/app/app_nordic.c"; OUTNAME=frl_nordic_testbtn_cc2340r5; else OUTNAME=frl_nordic_cc2340r5; fi
cp "$HERE/src/led_button_service.c" "$HERE/src/led_button_service.h" \
   "$HERE/src/nordic_uart_service.c" "$HERE/src/nordic_uart_service.h" "$HERE/cc2340r5/app/"
# 3. Three objects and their rules in the example's makefile
python3 - "$HERE/cc2340r5/freertos/ticlang/makefile" <<'PY'
import sys
p = sys.argv[1]
s = open(p).read()
new = ["app_nordic", "led_button_service", "nordic_uart_service"]
s = s.replace("app_app_main.obj ", "app_app_main.obj " + " ".join(n + ".obj" for n in new) + " ", 1)
s = s.replace("CFLAGS += -I../..", "CFLAGS += -I../../app -I../..", 1) if "-I../../app" not in s else s
rules = "".join(f"\n{n}.obj: ../../app/{n}.c $(SYSCFG_H_FILES) prebuild\n\t@ echo Building $@\n\t$(V) $(CC) $(CFLAGS) -c $< -o $@\n" for n in new)
marker = "app_app_main.obj:"
i = s.index(marker)
s = s[:i] + rules.lstrip("\n") + "\n" + s[i:]
open(p, "w").write(s)
PY
cd "$HERE/cc2340r5/freertos/ticlang"
make -j4 SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CLANG"
grep -A8 "MEMORY CONFIGURATION" basic_ble.map | grep -E " FLASH | SRAM "
mkdir -p "$HERE/out"
cp basic_ble.hex "$HERE/out/$OUTNAME.hex"
cp basic_ble.map "$HERE/out/$OUTNAME.map"
echo "built: $HERE/out/$OUTNAME.hex"
echo "flash: /Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c <abs path>/cc2340r5/CC2340R5.ccxml -e -f -u $HERE/out/$OUTNAME.hex"
