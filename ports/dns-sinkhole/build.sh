#!/bin/bash
# Build the DNS sinkhole firmware for the LP-EM-CC35X1 in a copy of TI's mqtt_client demo, outside the repo.
#   APP=min   ./build.sh     stage 1: fixed answer 192.0.2.1 for every A query (E236 first step)
#   APP=full  BLOCKLIST=<table.bin> ./build.sh   the sinkhole: hash table in flash, forwarding, counters, status page
# Optional: UPSTREAM=<ipv4 of the resolver to forward to; default = the gateway the board gets>, RUN=<build folder>.
# The network name comes from the Mac's current Wi-Fi network (or HOME_SSID), the password from WIFI_ENV
# (default ~/.config/frl/home-wifi.env, key WIFI_PASSWORD=). Both go into the build copy only, never into the repo.
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"; APP="${APP:-full}"
RUN="${RUN:-$PWD/out/dns/build-$APP}"
: "${WIFI_ENV:=$HOME/.config/frl/home-wifi.env}"
HOME_SSID="${HOME_SSID:-$(ipconfig getsummary en0 | awk -F' : ' '$1 ~ /^ *SSID$/ {print $2; exit}')}"
[ -n "$HOME_SSID" ] || { echo "set HOME_SSID to the Wi-Fi network name"; exit 2; }
SDK=/Applications/ti/simplelink_wifi_sdk_10_20_00_39; TB=/Applications/ti/simplelink_wifi_toolbox_macos_4_3_23
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh; CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
rm -rf "$RUN"; mkdir -p "$RUN"; C="$RUN/ex"; cp -R "$SDK/examples/rtos/LP_EM_CC35X1/demos/mqtt_client" "$C"
cp "$HERE/firmware/dns_boot.h" "$HERE/firmware/dns_checks.h" "$C/"
if [ "$APP" = min ]; then cp "$HERE/firmware/dns_min.c" "$C/mqtt_client_app.c"
else
	cp "$HERE/firmware/dns_sinkhole.c" "$C/mqtt_client_app.c"
	[ -f "${BLOCKLIST:-}" ] || { echo "APP=full needs BLOCKLIST=<table.bin> (gen_blocklist.py)"; exit 2; }
	cp "$BLOCKLIST" "$C/blocklist.bin"
fi
python3 - "$C" "$HOME_SSID" "$WIFI_ENV" <<'PY'
import sys
C, ssid, envf = sys.argv[1:4]
pw = [l.split('=', 1)[1].strip() for l in open(envf) if l.startswith('WIFI_PASSWORD=')][0].strip('"\'')
p = C + '/wifi_settings.h'; t = open(p).read()
assert '"Example_SSID"' in t and '"Example_PASS"' in t
open(p, 'w').write(t.replace('"Example_SSID"', '"%s"' % ssid).replace('"Example_PASS"', '"%s"' % pw))
PY
M="$C/freertos/ticlang/makefile"
{ echo; echo "# added by ports/dns-sinkhole/build.sh"
  echo "CFLAGS += -DBLK_BIN_PATH=\\\"$C/blocklist.bin\\\""
  [ -z "${UPSTREAM:-}" ] || echo "CFLAGS += -DDNS_UPSTREAM_STR=\\\"$UPSTREAM\\\""; } >> "$M"
d="$C/freertos/ticlang"
make -C "$d" SIMPLELINK_WIFI_SDK_INSTALL_DIR="$SDK" SIMPLELINK_WIFI_TOOLBOX_INSTALL_DIR="$TB" SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CGT" GCC_ARMCOMPILER= > "$RUN/b.log" 2>&1 \
 || { echo FAILED; grep -m8 -iE "error" "$RUN/b.log"; exit 1; }
make -s -f "$TB/scripts/makefile" all SDK_DIR="$SDK" SYSCONFIG_ARTIFACT="$d" BUILD_DIR="$d" BUILD_ARTIFACT="$d/mqtt_client.out" TOOLBOX_DIR="$TB" > "$RUN/sign.log" 2>&1 \
 || { echo "SIGN FAILED"; tail -5 "$RUN/sign.log"; exit 1; }
echo "ok: $d/toolbox/tool_settings.json"; ls "$d/toolbox" 2>/dev/null | head -5
