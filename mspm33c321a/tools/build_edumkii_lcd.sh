#!/bin/bash
# Build the LCD frame-rate and CPU-load test (bench/edumkii/lcd_main.c, DMA and polled) -> <dir>/edumkii-lcd/ticlang/*.out ; RAM struct `lc2`
HERE="$(cd "$(dirname "$0")/.." && pwd)"; R="${1:-$PWD/out/mspm33c321a}"; O="$R/edumkii-lcd"
TOOLPATHS_FAMILY=mspm33-cc; . "$(dirname "$0")/../../tools/frl/toolpaths.sh"   # SDK and CC: MSPM33 SDK and TI Arm Clang, found or FRL_MSPM33_SDK / FRL_TICLANG
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh
rm -rf "$O"; cp -R "$SDK/examples/nortos/LP_MSPM33C321A/driverlib/gpio_toggle_output" "$O"
cp "$HERE/bench/edumkii/lcd.syscfg" "$O/gpio_toggle_output.syscfg"; cp "$HERE/bench/edumkii/lcd_main.c" "$O/gpio_toggle_output.c"; cp "$HERE"/bench/edumkii/HAL_LCD.[ch] "$HERE/bench/edumkii/font_5x7.h" "$O/"
make -C "$O/ticlang" MSPM33_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CC" GCC_ARMCOMPILER= > "$O/build.log" 2>&1 && echo built || { tail -8 "$O/build.log"; exit 1; }
grep -E '^ +(BCR|BSL)_CONFIG ' "$O"/ticlang/*.map | awk '$4!="00000000"{print "NONMAIN USED"}'
