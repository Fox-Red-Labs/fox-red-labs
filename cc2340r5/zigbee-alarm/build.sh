#!/bin/bash
# One-command build: ./build.sh  ->  freertos/ticlang/alarm.hex (TI Arm Clang 5.1.1, SysConfig 1.26.3, F3 SDK 9.21.00.36)
cd "$(dirname "$0")/freertos/ticlang" || exit 1
make -j4 SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR=/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS \
  SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh \
  TICLANG_ARMCOMPILER=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS "$@"
