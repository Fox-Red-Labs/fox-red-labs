#!/usr/bin/env bash
# Build the tool tag from TI's basic_ble example. Touches no board, probe or serial port.
#   tool-telemetry/tag/build.sh             LP-EM-CC2340R5 (default)
#   tool-telemetry/tag/build.sh cc2755p10   LP-EM-CC2755P10
# Output: out/tool-telemetry/tag-<board>/ (tooltag.out, tooltag.hex, size.txt, make.log).
# Tools: F3 SDK 9.21.00.36, SysConfig 1.26.3, TI Arm Clang 5.1.1.LTS; override with FRL_F3_SDK, FRL_SYSCONFIG_DIR, FRL_TICLANG.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
BOARD="${1:-cc2340r5}"
case "$BOARD" in
	cc2340r5)  KIT=LP_EM_CC2340R5 ;;
	cc2755p10) KIT=LP_EM_CC2755P10 ;;
	*) echo "board must be cc2340r5 or cc2755p10"; exit 2 ;;
esac
ROOT=/Applications/ti
SDK="${FRL_F3_SDK:-$ROOT/simplelink_lowpower_f3_sdk_9_21_00_36_LTS}"
SYSCFG="${FRL_SYSCONFIG_DIR:-$ROOT/sysconfig_1.26.3}/sysconfig_cli.sh"
CGT="${FRL_TICLANG:-$ROOT/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS}"
for p in "$SDK/examples/rtos/$KIT/ble/basic_ble" "$SYSCFG" "$CGT/bin/tiarmclang"; do [ -e "$p" ] || { echo "missing: $p"; exit 4; }; done
OUT="$PWD/out/tool-telemetry/tag-$BOARD"; rm -rf "$OUT"; mkdir -p "$OUT"
KEYF="${TT_MODEL_KEY:-$HOME/.config/frl/tool-telemetry-model.key}"     # fleet signing key for installed models
[ -f "$KEYF" ] || { mkdir -p "$(dirname "$KEYF")"; (umask 077; LC_ALL=C od -An -N16 -tx1 /dev/urandom | tr -d ' \n' > "$KEYF"); }
cp -R "$SDK/examples/rtos/$KIT/ble/basic_ble" "$OUT/basic_ble"
[ -f "$HERE/../ai/out/tt_model.c" ] || python3 "$HERE/../ai/train.py" > /dev/null
python3 "$HERE/apply.py" "$OUT/basic_ble"
D="$OUT/basic_ble/freertos/ticlang"
make -C "$D" -j8 SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CGT" \
	> "$OUT/make.log" 2>&1 || { grep -m12 -iE "error|warning: impl" "$OUT/make.log"; tail -5 "$OUT/make.log"; exit 1; }
cp "$D/basic_ble.out" "$OUT/tooltag.out"
"$CGT/bin/tiarmobjcopy" -O ihex "$OUT/tooltag.out" "$OUT/tooltag.hex"
"$CGT/bin/tiarmsize" "$OUT/tooltag.out" | tee "$OUT/size.txt"
grep -c "warning" "$OUT/make.log" | sed 's/^/warnings: /'
echo "built: $OUT/tooltag.out"
