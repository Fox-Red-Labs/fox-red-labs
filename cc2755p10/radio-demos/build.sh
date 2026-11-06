#!/usr/bin/env bash
# Build the radio-board demos (blink, message, button) from TI's drivers/empty project. Touches no board, probe or serial port.
#   cc2755p10/radio-demos/build.sh <board> <demo>...      board: cc2755p10 or cc2340r5; demo: blink, message, button (default: all three)
# Writes prebuilt/<board>/radio-<demo>/ (radio-<demo>.hex, .out, manifest.json, NOTICE.txt) at the repo root.
# Tools are found as the rest of the repo finds them; override with FRL_F3_SDK, FRL_SYSCONFIG_DIR, FRL_TICLANG.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../.." && pwd)"
BOARD="${1:?board: cc2755p10 or cc2340r5}"; shift || true
DEMOS=("$@"); [ ${#DEMOS[@]} -gt 0 ] || DEMOS=(blink message button)
case "$BOARD" in
	cc2755p10) KIT=LP_EM_CC2755P10; TABLE=0x000DF800 ;;   # last 2 kB sector below the HSM area (0xE8000)
	cc2340r5)  KIT=LP_EM_CC2340R5;  TABLE=0x0007F800 ;;   # last 2 kB sector of the 512 kB main flash
	*) echo "board must be cc2755p10 or cc2340r5"; exit 2 ;;
esac
ROOT=/Applications/ti
SDK="${FRL_F3_SDK:-}"
for d in "$ROOT" "$HOME/ti"; do [ -n "$SDK" ] || { [ -d "$d/simplelink_lowpower_f3_sdk_9_21_00_36_LTS" ] && SDK="$d/simplelink_lowpower_f3_sdk_9_21_00_36_LTS"; }; done
SYSCFG="${FRL_SYSCONFIG_DIR:-$ROOT/sysconfig_1.26.3}/sysconfig_cli.sh"
[ -e "$SYSCFG" ] || SYSCFG="$ROOT/sysconfig_1.28.0/sysconfig_cli.sh"
CGT="${FRL_TICLANG:-$ROOT/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS}"
EX="$SDK/examples/nortos/$KIT/drivers/empty"
ZEX="$SDK/examples/rtos/$KIT/zigbee/onoff_light"
for p in "$EX" "$SYSCFG" "$CGT/bin/tiarmclang"; do [ -e "$p" ] || { echo "missing: $p"; exit 4; }; done
for DEMO in "${DEMOS[@]}"; do
	case "$DEMO" in blink) N=1 ;; message) N=2 ;; button) N=3 ;; zigbee) N=0 ;; *) echo "unknown demo $DEMO"; exit 2 ;; esac
	TMP="$(mktemp -d -t frl-radio-demo)"; trap 'rm -rf "$TMP"' EXIT
	if [ "$DEMO" = zigbee ]; then
		# TI's Zigbee on/off light, unchanged: it logs to the backchannel UART at 115200
		[ -e "$ZEX" ] || { echo "missing: $ZEX"; exit 4; }
		cp -R "$ZEX" "$TMP/empty"
		make -C "$TMP/empty/freertos/ticlang" -j8 SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL="$SYSCFG" \
			TICLANG_ARMCOMPILER="$CGT" > "$TMP/make.log" 2>&1 || { tail -30 "$TMP/make.log"; exit 1; }
		python3 -I "$HERE/package.py" "$BOARD" "$KIT" "$DEMO" "$TMP/empty/freertos/ticlang/onoff_light.out" "$SDK" "$SYSCFG" "$CGT" "$REPO"
		rm -rf "$TMP"; trap - EXIT; continue
	fi
	cp -R "$EX" "$TMP/empty"
	cp "$HERE/frl_demo.c" "$TMP/empty/empty.c"
	sed "s#@BOARD@#$KIT#" "$HERE/frl_demo.syscfg" > "$TMP/empty/empty.syscfg"
	CFLAGS="-DFRL_DEMO=$N -DFRL_TABLE_ADDR=$TABLE" make -C "$TMP/empty/ticlang" -j8 SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR="$SDK" SYSCONFIG_TOOL="$SYSCFG" \
		TICLANG_ARMCOMPILER="$CGT" > "$TMP/make.log" 2>&1 || { tail -30 "$TMP/make.log"; exit 1; }
	python3 -I "$HERE/package.py" "$BOARD" "$KIT" "$DEMO" "$TMP/empty/ticlang/empty.out" "$SDK" "$SYSCFG" "$CGT" "$REPO"
	rm -rf "$TMP"; trap - EXIT
done
