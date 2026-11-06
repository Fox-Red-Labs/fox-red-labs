#!/bin/bash
# Build the screen test (bench/edumkii/screen_main.c with CCStudio's HAL_LCD driver) at a given SPI clock.
#   build_edumkii_screen.sh <spi-hz> [dir]       output <dir>/edumkii-screen-<mhz>/ticlang/gpio_toggle_output.out ; RAM struct `lc`
HERE="$(cd "$(dirname "$0")/.." && pwd)"; HZ=${1:-8000000}; R="${2:-$PWD/out/mspm33c321a}"; O="$R/edumkii-screen-$((HZ/1000000))"
TOOLPATHS_FAMILY=mspm33-cc; . "$(dirname "$0")/../../tools/frl/toolpaths.sh"   # SDK and CC: MSPM33 SDK and TI Arm Clang, found or FRL_MSPM33_SDK / FRL_TICLANG
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh
rm -rf "$O"; cp -R "$SDK/examples/nortos/LP_MSPM33C321A/driverlib/gpio_toggle_output" "$O"
sed "s/targetBitRate              = 8000000/targetBitRate              = $HZ/" "$HERE/bench/edumkii/screen.syscfg" > "$O/gpio_toggle_output.syscfg"
cp "$HERE/bench/edumkii/screen_main.c" "$O/gpio_toggle_output.c"; cp "$HERE"/bench/edumkii/HAL_LCD.[ch] "$HERE/bench/edumkii/font_5x7.h" "$O/"
sed -i '' "s/#include \"ti_msp_dl_config.h\"/#include \"ti_msp_dl_config.h\"\n#define SPI_BITRATE $HZ/" "$O/gpio_toggle_output.c"
make -C "$O/ticlang" MSPM33_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CC" GCC_ARMCOMPILER= > "$O/build.log" 2>&1 && echo built || { tail "$O/build.log"; exit 1; }
