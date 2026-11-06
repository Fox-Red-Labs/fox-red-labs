#!/bin/bash
# Build bench/edumkii/i2c_scan (software I2C scan of the Education BoosterPack's OPT3001) from TI's gpio_toggle_output
# project files with our syscfg and main. Output <dir>/edumkii-i2c/ticlang/gpio_toggle_output.out ; RAM struct `sc`.
HERE="$(cd "$(dirname "$0")/.." && pwd)"; R="${1:-$PWD/out/mspm33c321a}"; O="$R/edumkii-i2c"
TOOLPATHS_FAMILY=mspm33-cc; . "$(dirname "$0")/../../tools/frl/toolpaths.sh"   # SDK and CC: MSPM33 SDK and TI Arm Clang, found or FRL_MSPM33_SDK / FRL_TICLANG
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh
rm -rf "$O"; cp -R "$SDK/examples/nortos/LP_MSPM33C321A/driverlib/gpio_toggle_output" "$O"
cp "$HERE/bench/edumkii/i2c_scan.syscfg" "$O/gpio_toggle_output.syscfg"; cp "$HERE/bench/edumkii/i2c_scan.c" "$O/gpio_toggle_output.c"
make -C "$O/ticlang" MSPM33_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CC" GCC_ARMCOMPILER= > "$O/build.log" 2>&1 && echo built || { tail "$O/build.log"; exit 1; }
