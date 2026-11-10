#!/bin/bash
# Build one SDK example for the LP-AM243 with the SDK makefile and TI Arm Clang 4.0.4.LTS: build_sdk_example.sh <dir containing ti-arm-clang> [make target]
# Prints: BUILD <ok|fail> <seconds> <out file>. The .mcelf step needs python 'construct' and is skipped: we load the .out into RAM.
# Needs the nortos/freertos libs built once with make -f makefile.am243x.
D="$1"; SDK=$HOME/ti/mcu_plus_sdk_am243x_12_01_00_33
MKARGS="DEVICE=am243x PROFILE=release TOOLS_PATH=/Applications/ti CCS_PATH=/Applications/ti/ccs2101/ccs CGT_TI_ARM_CLANG_PATH=/Applications/ti/ti-cgt-armllvm_release SYSCFG_PATH=/Applications/ti/sysconfig_1.27.0"
cd "$D/ti-arm-clang" 2>/dev/null || { echo "BUILD fail 0 no-dir"; exit 1; }
N=$(grep -m1 '^OUTNAME' makefile | sed 's/.*:=//;s/\$(PROFILE)/release/;s/\$(CONFIG)/release/' | tr -d ' ')
rm -f "$N"; s=$(date +%s); make -j2 ${2:-$N} $MKARGS > build.log 2>&1; rc=$?; e=$(date +%s)
if [ $rc -eq 0 ] && [ -f "$N" ]; then echo "BUILD ok $((e-s)) $PWD/$N"; else echo "BUILD fail $((e-s)) $PWD/$N"; fi
