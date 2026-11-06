#!/bin/bash
# Build TI's ota_example for the LP-EM-CC35X1 at a chosen image version and sign it with the SDK example key.
# Host side only: touches no board, probe or serial port and never runs the Toolbox programmer.
#
#   ./build.sh 0.0.1.0     first image, the one that is programmed with the Toolbox
#   ./build.sh 0.0.2.0     update image to serve over the air (banner shows the version)
#
# Output: $OUT/v<version>/ with ota_example.out and toolbox/ (primary_vendor_image.sign.bin is the file the OTA
# server serves; tool_settings.json is for the Toolbox). Default OUT is out/cc35x1-ota.
set -u
VERSION="${1:?usage: build.sh 0.0.N.0}"
[[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "version must look like 0.0.2.0"; exit 2; }
SDK=/Applications/ti/simplelink_wifi_sdk_10_20_00_39
TB=/Applications/ti/simplelink_wifi_toolbox_macos_4_3_23
SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh
CGT=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
OUT="${OUT:-$PWD/out/cc35x1-ota}/v$VERSION"
rm -rf "$OUT"; mkdir -p "$OUT"
cp -R "$SDK/examples/rtos/LP_EM_CC35X1/demos/ota_example/." "$OUT/"
# Banner carries the version so the console shows which image runs.
sed -i '' "s#CC35xx OTA Example Application#CC35xx OTA Example Application FRL $VERSION#" "$OUT/ota_example.c"
D="$OUT/freertos/ticlang"
make -C "$D" SIMPLELINK_WIFI_SDK_INSTALL_DIR="$SDK" SIMPLELINK_WIFI_TOOLBOX_INSTALL_DIR="$TB" SYSCONFIG_TOOL="$SYSCFG" \
  TICLANG_ARMCOMPILER="$CGT" GCC_ARMCOMPILER= > "$OUT/build.log" 2>&1 || { echo "build failed, see $OUT/build.log"; exit 1; }
sed -i '' "s/^primary_vendor_image_version=.*/primary_vendor_image_version=$VERSION/" "$D/action_request_extra.txt"
make -s -f "$TB/scripts/makefile" all SDK_DIR="$SDK" SYSCONFIG_ARTIFACT="$D" BUILD_DIR="$D" BUILD_ARTIFACT="$D/ota_example.out" \
  TOOLBOX_DIR="$TB" > "$OUT/images.log" 2>&1 || { echo "signing failed, see $OUT/images.log"; exit 3; }
echo "$D"; ls "$D/toolbox" | grep -E "primary_vendor_image|tool_settings.json"
