#!/bin/bash
# Education BoosterPack inputs and outputs (bench/edumkii/inputs) from TI's hsadc_conversion_software_trigger project files.
#   output <run>/edumkii-inputs/ticlang/hsadc_conversion_software_trigger.out ; RAM struct `io`
HERE="$(cd "$(dirname "$0")/.." && pwd)"; R="${1:-$PWD/out/mspm33c321a}"; O="$R/edumkii-inputs"; EX=hsadc_conversion_software_trigger
TOOLPATHS_FAMILY=mspm33-cc; . "$(dirname "$0")/../../tools/frl/toolpaths.sh"   # SDK and CC: MSPM33 SDK and TI Arm Clang, found or FRL_MSPM33_SDK / FRL_TICLANG
rm -rf "$O"; cp -R "$SDK/examples/nortos/LP_MSPM33C321A/driverlib/$EX" "$O"
cp "$HERE/bench/edumkii/inputs/inputs.syscfg" "$O/$EX.syscfg"; cp "$HERE/bench/edumkii/inputs/main.c" "$O/$EX.c"; cp "$HERE/bench/m33_timer.h" "$O/"
make -C "$O/ticlang" MSPM33_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh TICLANG_ARMCOMPILER="$CC" GCC_ARMCOMPILER= > "$O/build.log" 2>&1 && echo built || { tail "$O/build.log"; exit 1; }
grep -E '^ +(BCR|BSL)_CONFIG ' "$O"/ticlang/*.map | awk '$4!="00000000"{print "NONMAIN USED"}'
