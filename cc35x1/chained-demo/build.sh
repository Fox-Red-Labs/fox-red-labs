#!/bin/bash
# Build the chained demo (BLE provisioning, Wi-Fi, MQTT, OTA) for the LP-EM-CC35X1 from the terminal.
# Compile, link, sign with the SDK example key (host side only). Touches no board, probe or serial port and never
# runs the Wi-Fi Toolbox programmer. Same tool paths as cc35x1/prebuild.sh.
#
#   ./build.sh                       build version 0.0.1.0 into out/cc35x1/chained-demo/v0.0.1.0
#   VERSION=0.0.2.0 ./build.sh       a second image with a higher version, for the OTA test
#   OUT_ROOT=<folder> CONFIG=<settings.h> ./build.sh   build elsewhere, with a settings file outside the repo
#
# Settings come from $CONFIG (default chained_config.h beside this script, git-ignored) or, if it is missing, from
# chained_config.example.h (placeholder broker, no network). After a good link the object files are deleted; the
# .out, .map, logs and the signed images in toolbox/ stay.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../.." && pwd)"
SDK=/Applications/ti/simplelink_wifi_sdk_10_20_00_39
TB=/Applications/ti/simplelink_wifi_toolbox_macos_4_3_23
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS   # SDK tested with 4.0.4.LTS; 5.1.1 is installed
VERSION="${VERSION:-0.0.1.0}"
[[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "VERSION must look like 0.0.2.0"; exit 2; }
OUT="${OUT_ROOT:-$PWD/out/cc35x1/chained-demo}/v$VERSION"
CONFIG="${CONFIG:-$HERE/chained_config.h}"   # a settings file kept outside the repo also works

for p in "$SDK" "$TB/simplelink-wifi-toolbox" "$SYSCFG" "$CGT/bin/tiarmclang"; do
	[ -e "$p" ] || { echo "missing: $p"; exit 4; }
done
free_mb=$(df -m / | awk 'NR==2 {print $4}')
[ "$free_mb" -ge 600 ] || { echo "only $free_mb MB free on /; need 600 MB. Stopping."; exit 5; }

# Host test of the board-independent layer first
"$REPO/portability/iot-chain/test/run.sh" || { echo "host test failed"; exit 6; }

rm -rf "$OUT"; mkdir -p "$OUT/config"
if [ -f "$CONFIG" ]; then
	cp "$CONFIG" "$OUT/config/chained_config.h"; echo "config: $(basename "$CONFIG")"
else
	cp "$HERE/chained_config.example.h" "$OUT/config/chained_config.h"; echo "config: chained_config.example.h (placeholders)"
fi

MK=(SIMPLELINK_WIFI_SDK_INSTALL_DIR="$SDK" SIMPLELINK_WIFI_TOOLBOX_INSTALL_DIR="$TB" SYSCONFIG_TOOL="$SYSCFG"
	TICLANG_ARMCOMPILER="$CGT" GCC_ARMCOMPILER= HERE="$HERE" REPO="$REPO" CONFIG_DIR="$OUT/config"
	CHAINED_FW_VERSION="$VERSION")
t0=$(date +%s)
if ! make -C "$OUT" -f "$HERE/makefile" "${MK[@]}" > "$OUT/build.log" 2>&1; then
	echo "BUILD FAILED after $(( $(date +%s) - t0 )) s; first errors (full log: $OUT/build.log):"
	grep -m5 -iE "error|undefined|not found|No rule" "$OUT/build.log" | cut -c1-300
	exit 1
fi
echo "linked in $(( $(date +%s) - t0 )) s: $OUT/chained_demo.out"

# Image version for the Toolbox image builder. TI's syscfg leaves the Toolbox ActionReq module at its default
# (0.0.1.0); the OTA server compares this version, so a second image needs a higher one.
sed -i '' "s/^primary_vendor_image_version=.*/primary_vendor_image_version=$VERSION/" "$OUT/action_request_extra.txt"

# Sign: the Toolbox's image builder, the CCStudio post-build step run by hand (toolbox scripts/README.md)
IMG_FAILED=0
if make -s -f "$TB/scripts/makefile" all SDK_DIR="$SDK" SYSCONFIG_ARTIFACT="$OUT" BUILD_DIR="$OUT" \
	BUILD_ARTIFACT="$OUT/chained_demo.out" TOOLBOX_DIR="$TB" > "$OUT/images.log" 2>&1; then
	NBIN=$(ls "$OUT/toolbox" 2>/dev/null | grep -c '\.bin$' || true)
	if [ "$NBIN" -gt 0 ]; then
		echo "signed images: $OUT/toolbox/ ($NBIN .bin files)"
	else
		echo "image builder finished but wrote no .bin files in $OUT/toolbox, see $OUT/images.log"; IMG_FAILED=1
	fi
else
	echo "image builder FAILED, see $OUT/images.log"; tail -5 "$OUT/images.log"; IMG_FAILED=1
fi

# Sizes from the map file, on the same basis as docs/showcase/_fragments/cc35x1.md
python3 - "$OUT/chained_demo.map" > "$OUT/sizes.txt" <<'PY'
import re, sys
m = open(sys.argv[1]).read()
mem = {r[0]: (int(r[2], 16), int(r[3], 16)) for r in re.findall(r'^\s+(\S+)\s+([0-9a-f]{8})\s+[0-9a-f]+\s+([0-9a-f]+)\s+([0-9a-f]+)\s', m, re.M)}
heap = int(re.search(r'^\s+[0-9a-f]{8}\s+[0-9a-f]{8}\s+([0-9a-f]{8})\s+[0-9a-f]{8}\s+rw-\s+\.bss\.ucHeap', m, re.M).group(1), 16)
k = lambda b: b / 1024
flash = mem['FLASH_INT_VEC'][0] + mem['FLASH_NON_SECURE'][0]
dram_used, dram_free = mem['DRAM_NON_SECURE']
tcm = mem['TCM_DRAM_NON_SECURE'][0]
print(f"flash_used_bytes={flash} ({k(flash):,.0f} kB)")
print(f"static_ram_no_heap_bytes={dram_used - heap} ({k(dram_used - heap):,.0f} kB)")
print(f"freertos_heap_bytes={heap} ({k(heap):,.1f} kB)")
print(f"dram_free_bytes={dram_free} ({k(dram_free):,.0f} kB)")
print(f"tcm_dram_used_bytes={tcm} ({k(tcm):,.0f} kB)")
PY
cat "$OUT/sizes.txt"

# Keep .out, .map, logs, generated config and signed images; drop object files (disk is tight on this Mac)
rm -f "$OUT"/*.obj
rm -rf "$OUT/toolbox/temp"
echo "kept: $(du -sh "$OUT" | cut -f1) in $OUT"
# No usable signed image means the build is not done: fail after the sizes and logs are kept.
[ "$IMG_FAILED" -eq 0 ] || { echo "BUILD INCOMPLETE: no signed images (exit 7)"; exit 7; }
