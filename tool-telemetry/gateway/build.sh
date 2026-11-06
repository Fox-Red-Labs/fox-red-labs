#!/usr/bin/env bash
# Build the tool-telemetry gateway for the LP-EM-CC35X1: TI's network_terminal demo plus tt_gateway.c (Bluetooth scanner,
# tag table, log, HTTP server and the phone app). Builds in a copy outside the repo, signs the images with the SDK example
# key (the Toolbox's image builder, host side only), programs nothing.
#   tool-telemetry/gateway/build.sh            the gateway
#   PLAIN=1 tool-telemetry/gateway/build.sh    TI's network_terminal unchanged (for comparison)
# Optional uplink: SITE_SSID="<network name>" WIFI_ENV=<file with WIFI_PASSWORD=...> also joins a site network as a
# station; the name and password go into the firmware copy only, never into the repo or this script's output.
# Output: out/tool-telemetry/gateway/ (nt/freertos/ticlang/network_terminal.out, toolbox/tool_settings.json).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; RUN="$PWD/out/tool-telemetry/gateway"
SDK=/Applications/ti/simplelink_wifi_sdk_10_20_00_39; TB=/Applications/ti/simplelink_wifi_toolbox_macos_4_3_23
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh; CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
FREE=$(df -k /System/Volumes/Data | awk 'NR==2{print $4}'); [ "$FREE" -gt 153600 ] || { echo "disk gate: under 150 MB free (the build needs about 30 MB)"; exit 5; }
rm -rf "$RUN"; mkdir -p "$RUN"; C="$RUN/nt"
cp -R "$SDK/examples/rtos/LP_EM_CC35X1/demos/network_terminal" "$C"
# The hub's Wi-Fi password: random per hub, kept outside the repo, reused by later builds so phones stay joined.
SECRET="${TTGW_SECRET_FILE:-$HOME/.config/frl/tool-telemetry-hub.env}"
if [ -z "${TTGW_AP_PASS:-}" ]; then
	[ -f "$SECRET" ] || { mkdir -p "$(dirname "$SECRET")"; umask 077; echo "TTGW_AP_PASS=$(LC_ALL=C tr -dc 'a-km-np-z2-9' < /dev/urandom | head -c 12)" > "$SECRET"; }
	TTGW_AP_PASS=$(sed -n 's/^TTGW_AP_PASS=//p' "$SECRET")
fi
# Bench builds leave the hub's model, command and lease endpoints open (the tags still check every signature).
# SECURE=1 makes a control token (kept in the secrets file, sent by the home base scripts) and requires it.
TTGW_CTRL_TOKEN=""
if [ "${SECURE:-0}" = 1 ]; then
	grep -q '^TTGW_CTRL_TOKEN=' "$SECRET" || (umask 077; echo "TTGW_CTRL_TOKEN=$(LC_ALL=C tr -dc 'a-km-np-zA-HJ-NP-Z2-9' < /dev/urandom | head -c 20)" >> "$SECRET")
	TTGW_CTRL_TOKEN=$(sed -n 's/^TTGW_CTRL_TOKEN=//p' "$SECRET")   # never printed
fi
export TTGW_AP_PASS TTGW_CTRL_TOKEN
# Optional position of the site for the dashboards: SITE_LAT and SITE_LON (or in SITE_ENV), a few decimals is enough.
if [ -n "${SITE_ENV:-}" ]; then
	[ -n "${SITE_LAT:-}" ] || SITE_LAT=$(sed -n 's/^SITE_LAT=//p' "$SITE_ENV"); [ -n "${SITE_LON:-}" ] || SITE_LON=$(sed -n 's/^SITE_LON=//p' "$SITE_ENV")
fi
export SITE_LAT="${SITE_LAT:-}" SITE_LON="${SITE_LON:-}"
# Site uplink: SITE_ENV=<file with SITE_SSID=...> (or SITE_SSID set directly) plus WIFI_ENV for its password.
if [ -n "${SITE_ENV:-}" ] && [ -z "${SITE_SSID:-}" ]; then SITE_SSID=$(sed -n 's/^SITE_SSID=//p' "$SITE_ENV"); export SITE_SSID; fi
# Uplink: UPLINK_URL=http://<home base ip>:8090/api/ingest (and UPLINK_TOKEN if the home base wants one): the hub posts
# its new log items there every 30 s over whatever network it joined (site Wi-Fi, a phone hotspot, a cellular router).
export UPLINK_URL="${UPLINK_URL:-}" UPLINK_TOKEN="${UPLINK_TOKEN:-}"
if [ "${PLAIN:-0}" != 1 ]; then
	python3 "$HERE/../app/embed.py" "$HERE/../app/index.html" "$C/tt_app.h"
	python3 "$HERE/apply.py" "$C"
fi
make -C "$C/freertos/ticlang" -j8 SIMPLELINK_WIFI_SDK_INSTALL_DIR="$SDK" SIMPLELINK_WIFI_TOOLBOX_INSTALL_DIR="$TB" \
	SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CGT" GCC_ARMCOMPILER= > "$RUN/b.log" 2>&1 \
	|| { echo FAILED; grep -m12 -iE "error" "$RUN/b.log"; exit 1; }
"$CGT/bin/tiarmsize" "$C/freertos/ticlang/network_terminal.out" | tee "$RUN/size.txt"
d="$C/freertos/ticlang"
make -s -f "$TB/scripts/makefile" all SDK_DIR="$SDK" SYSCONFIG_ARTIFACT="$d" BUILD_DIR="$d" \
	BUILD_ARTIFACT="$d/network_terminal.out" TOOLBOX_DIR="$TB" > "$RUN/images.log" 2>&1 || true
TS="$d/toolbox/tool_settings.json"; [ -f "$TS" ] || { echo "no tool_settings.json (image step failed), see $RUN/images.log"; exit 1; }
echo "signed images: $d/toolbox/"
echo "hub Wi-Fi password: in $SECRET (not printed)"
[ -n "$TTGW_CTRL_TOKEN" ] && echo "control endpoints: token required (SECURE=1)" || echo "control endpoints: open, for the bench (build with SECURE=1 to require a token)"
echo "flash with: tool-telemetry/gateway/flash.sh"
