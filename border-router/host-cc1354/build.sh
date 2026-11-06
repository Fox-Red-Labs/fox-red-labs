#!/bin/bash
# Build the Thread border-router host for the LP-EM-CC1354P10-1 (OpenThread FTD, radio = TI RCP over UART).
# Usage: build.sh <build dir>        Output: <build dir>/bin/otbr-host-cc1354.out (+ .bin, .map)
#
# Needs: ~/ot-ti (TI's OpenThread port, release thread-v1.4-ti-2.1, with its openthread submodule) and the
# toolchain the Thread work already set up under ~/ot-ti/_tools (GCC 12.2 from the Zephyr SDK, CMake < 4,
# ninja), SysConfig 1.26.3, and the installed F2 SDK 8.33 (cc2340r5/thread/README.md).
set -e
B="${1:?build dir}"
HERE="$(cd "$(dirname "$0")" && pwd)"
OT_TI="${OT_TI:-$HOME/ot-ti}"
SDK="${TI_F2_SDK:-/Applications/ti/simplelink_cc13xx_cc26xx_sdk_8_33_00_16}"
export PATH="$OT_TI/_tools/gcc/bin:$OT_TI/_tools/venv/bin:$PATH"
export TI_SYSCONFIG_ROOT="${TI_SYSCONFIG_ROOT:-/Applications/ti/sysconfig_1.26.3}"

# ot-ti's CMake reads <SDK>/../../../src/nvocmp.c, a path that only exists when the SDK is ot-ti's own
# submodule. Mirror that shape with symlinks so the installed SDK works (same trick as the CC1354 Thread build).
mkdir -p "$B/otsdk/a/b"
ln -sfn "$SDK" "$B/otsdk/a/b/sdk"
ln -sfn "$OT_TI/src" "$B/otsdk/src"
export TI_SIMPLELINK_SDK_DIR="$B/otsdk/a/b/sdk"

cmake -GNinja -S "$HERE" -B "$B" \
  -DOT_TI="$OT_TI" \
  -DCMAKE_TOOLCHAIN_FILE="$OT_TI/src/arm-none-eabi-m33.cmake" \
  -DOT_PLATFORM=external \
  -DTI_SIMPLELINK_BOARD=LP_EM_CC1354P10_1 \
  -DOT_PING_SENDER=ON
cmake --build "$B" -- -k 0
arm-none-eabi-size "$B/bin/otbr-host-cc1354.out"
