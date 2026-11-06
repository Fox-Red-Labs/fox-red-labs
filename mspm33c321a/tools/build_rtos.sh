#!/bin/bash
# Build bench/rtos/main.c into a copy of TI's FreeRTOS blink_led example (LP_MSPM33C321A), 32 MHz, -O2.
#   build_rtos.sh [opt flag O2|O3]    output <run>/rtos-bench-<opt>/freertos/ticlang/blink_led.out
set -e
OPT=${1:-O2}; HERE="$(cd "$(dirname "$0")/.." && pwd)"; R="${RUNDIR:-$PWD/out/mspm33c321a}"; O="$R/rtos-bench-$OPT"
TOOLPATHS_FAMILY=mspm33-cc; . "$(dirname "$0")/../../tools/frl/toolpaths.sh"   # SDK and CC: MSPM33 SDK and TI Arm Clang, found or FRL_MSPM33_SDK / FRL_TICLANG
rm -rf "$O"; cp -R "$SDK/examples/rtos/LP_MSPM33C321A/kernel/blink_led" "$O"
cp "$HERE/bench/rtos/main.c" "$O/main.c"; cp "$HERE/bench/m33_timer.h" "$O/"
sed -i "" -e "s/-O2/-$OPT/" -e "s#-T\./mspm33c321a.cmd#./mspm33c321a.cmd#" "$O/freertos/ticlang/makefile"
make -C "$O/freertos/ticlang" MSPM33_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh TICLANG_ARMCOMPILER="$CC" GCC_ARMCOMPILER= > "$O/build.log" 2>&1 && echo "rtos $OPT built" || { tail -15 "$O/build.log"; exit 1; }
grep -E '^ +(BCR|BSL)_CONFIG ' "$O"/freertos/ticlang/*.map | awk '$4!="00000000"{print "NONMAIN USED"}'
