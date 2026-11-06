#!/usr/bin/env bash
# Build the Bluetooth light from TI's basic_ble example. Touches no board, probe or serial port.
#   cc2755p10/ble-light/build.sh                 LP-EM-CC2755P10 (default)
#   cc2755p10/ble-light/build.sh cc2340r5        LP-EM-CC2340R5
# Writes prebuilt/<board>/ble-light/ (ble-light.out, ble-light.hex, manifest.json, NOTICE.txt) at the repo root.
# Tools are found the way the rest of the repo finds them; override with FRL_F3_SDK, FRL_SYSCONFIG_DIR, FRL_TICLANG.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../.." && pwd)"
BOARD="${1:-cc2755p10}"
case "$BOARD" in
	cc2755p10) KIT=LP_EM_CC2755P10; MK=cc27xx ;;
	cc2340r5)  KIT=LP_EM_CC2340R5;  MK=cc2340 ;;
	*) echo "board must be cc2755p10 or cc2340r5"; exit 2 ;;
esac
ROOT=/Applications/ti
SDK="${FRL_F3_SDK:-}"
for d in "$ROOT" "$HOME/ti"; do [ -n "$SDK" ] || { [ -d "$d/simplelink_lowpower_f3_sdk_9_21_00_36_LTS" ] && SDK="$d/simplelink_lowpower_f3_sdk_9_21_00_36_LTS"; }; done
SYSCFG="${FRL_SYSCONFIG_DIR:-$ROOT/sysconfig_1.26.3}/sysconfig_cli.sh"
CGT="${FRL_TICLANG:-$ROOT/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS}"
for p in "$SDK/examples/rtos/$KIT/ble/basic_ble" "$SYSCFG" "$CGT/bin/tiarmclang"; do [ -e "$p" ] || { echo "missing: $p"; exit 4; }; done
TMP="$(mktemp -d -t frl-ble-light)"; trap 'rm -rf "$TMP"' EXIT
cp -R "$SDK/examples/rtos/$KIT/ble/basic_ble" "$TMP/basic_ble"
python3 "$HERE/apply.py" "$TMP/basic_ble"
make -C "$TMP/basic_ble/freertos/ticlang" -j8 SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL="$SYSCFG" \
	TICLANG_ARMCOMPILER="$CGT" > "$TMP/make.log" 2>&1 || { tail -30 "$TMP/make.log"; exit 1; }
python3 -I "$HERE/package.py" "$BOARD" "$KIT" "$TMP/basic_ble/freertos/ticlang/basic_ble.out" "$SDK" "$SYSCFG" "$CGT" "$REPO"
