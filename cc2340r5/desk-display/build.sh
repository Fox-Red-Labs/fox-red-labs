#!/bin/bash
# "FRL Desk": Nordic UART Service on the LP-EM-CC2340R5 that draws text lines on an SSD1306 128x64 I2C OLED.
# Built on top of ports/nordic-apps (same patch, same generated services) plus an I2C instance and the OLED driver.
#   ./build.sh   -> out/frl_desk_cc2340r5.hex
set -euo pipefail
SDK=${SDK:-/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS}
SYSCFG=${SYSCFG:-/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh}
CLANG=${CLANG:-/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS}
HERE="$(cd "$(dirname "$0")" && pwd)"
NORDIC="$HERE/../../ports/nordic-apps"
EX=$SDK/examples/rtos/LP_EM_CC2340R5/ble/basic_ble
W="$HERE/work"
rm -rf "$W"; cp -R "$EX" "$W"; chmod -R u+w "$W"
( cd "$W" && patch -p1 -s < "$NORDIC/basic_ble.patch" )
cp "$HERE"/app/*.c "$HERE"/app/*.h "$W/app/"
cp "$NORDIC"/src/led_button_service.[ch] "$NORDIC"/src/nordic_uart_service.[ch] "$W/app/"
python3 - "$W" <<'PY'
import sys
w = sys.argv[1]
p = w + "/freertos/basic_ble.syscfg"
s = open(p).read()
s = s.replace('ble.deviceName = "FRL Nordic";', 'ble.deviceName = "FRL Desk";')
k = s.index("\n", s.index("/* ======== ")) if False else s.index("scripting.addModule")
k = s.rfind("\n", 0, k) + 1
s = s[:k] + 'var Sensors = scripting.addHardware("/ti/boards/boosterpacks/BP-BASSENSORSMKII");\n' + s[k:]
s += '''
/* ======== I2C for the SSD1306 OLED: BoosterPack J1 pin 9 SCL (DIO24), pin 10 SDA (DIO0) ======== */
const I2C = scripting.addModule("/ti/drivers/I2C");
const i2cOled = I2C.addInstance();
i2cOled.$name = "CONFIG_I2C_OLED";
i2cOled.$hardware = Sensors.components.BP_BASSENSORSMKII_I2C;
'''
open(p, "w").write(s)
p = w + "/freertos/ticlang/makefile"
s = open(p).read()
new = ["app_desk", "oled", "led_button_service", "nordic_uart_service"]
s = s.replace("app_app_main.obj ", "app_app_main.obj " + " ".join(n + ".obj" for n in new) + " ", 1)
s = s.replace("CFLAGS += -I../..", "CFLAGS += -I../../app -I../..", 1) if "-I../../app" not in s else s
rules = "".join(f"\n{n}.obj: ../../app/{n}.c $(SYSCFG_H_FILES) prebuild\n\t@ echo Building $@\n\t$(V) $(CC) $(CFLAGS) -c $< -o $@\n" for n in new)
i = s.index("app_app_main.obj:")
s = s[:i] + rules.lstrip("\n") + "\n" + s[i:]
open(p, "w").write(s)
PY
cd "$W/freertos/ticlang"
make -j4 SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CLANG"
grep -A8 "MEMORY CONFIGURATION" basic_ble.map | grep -E " FLASH | SRAM " || true
mkdir -p "$HERE/out"; cp basic_ble.hex "$HERE/out/frl_desk_cc2340r5.hex"; cp basic_ble.map "$HERE/out/frl_desk_cc2340r5.map"
echo "built: $HERE/out/frl_desk_cc2340r5.hex"
