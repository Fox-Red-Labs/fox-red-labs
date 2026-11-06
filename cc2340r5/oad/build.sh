#!/bin/bash
# Build the image set for a Bluetooth over-the-air update (OAD) of the LP-EM-CC2340R5, dual-image flavour.
#   ./build.sh            (needs the F3 SDK, SysConfig and TI Arm Clang at the paths below)
# Makes two copies of TI's basic_ble_oad_dual_image example, changes the name and the blink in each,
# builds both with TI's own post-build step (imgtool sign), and copies the files that matter to images/.
#   v1: name "FRL OAD 1", green LED toggles every 1000 ms, signed as version 1.0.0 (flash this with the probe)
#   v2: name "FRL OAD 2", red LED toggles every 250 ms, signed as version 2.0.0 (send this over the air)
# The boot image is TI's prebuilt MCUboot for dual-image OAD, from the SDK's prebuilt_hexfiles folder.
set -euo pipefail
SDK=${SDK:-/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS}
SYSCFG=${SYSCFG:-/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh}
CLANG=${CLANG:-/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS}
PY=${PY:-$HOME/.venvs/ble/bin/python}
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
EX=$SDK/examples/rtos/LP_EM_CC2340R5/ble/basic_ble_oad_dual_image
MCUBOOT=$SDK/examples/rtos/LP_EM_CC2340R5/ble/prebuilt_hexfiles/mcuboot_dual_image_LP_EM_CC2340R5_nortos_ticlang.hex
NAME=basic_ble_oad_dual_image
mkdir -p "$HERE/images" "$HERE/logs"

build() {  # build <version> <device name> <LED> <blink ms>
  local v=$1 dir="$HERE/build/v$1"
  rm -rf "$dir"; mkdir -p "$dir"
  rsync -a "$EX/" "$dir/"
  ( cd "$dir" && patch -p1 -s < "$HERE/app.patch" )
  printf '/* Written by build.sh: what makes image v%s look different */\n#define FRL_LED %s\n#define FRL_BLINK_MS %s\n' \
    "$v" "$3" "$4" > "$dir/app/frl_version.h"
  sed -i '' "s/^ble.deviceName = \"dual_image_app\";/ble.deviceName = \"$2\";/" "$dir/freertos/$NAME.syscfg"
  grep -q "ble.deviceName = \"$2\";" "$dir/freertos/$NAME.syscfg"
  local t0=$SECONDS
  ( cd "$dir/freertos/ticlang" && make -j4 SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR="$SDK" \
      SYSCONFIG_TOOL="$SYSCFG" TICLANG_ARMCOMPILER="$CLANG" ) > "$HERE/logs/build-v$v.txt" 2>&1
  echo "v$v built in $((SECONDS - t0)) s (log: logs/build-v$v.txt)"
  grep -o "\"$2\"\|'$2'" "$dir/freertos/ticlang/ti_ble_config.c" | head -1 | sed 's/^/  name in ti_ble_config.c: /'
}

build 1 "FRL OAD 1" CONFIG_GPIO_LED_GREEN 1000
build 2 "FRL OAD 2" CONFIG_GPIO_LED_RED 250

B1=$HERE/build/v1/freertos/ticlang; B2=$HERE/build/v2/freertos/ticlang
cp "$B1/${NAME}_v1.bin" "$HERE/images/frl_oad_v1.bin"     # flash at 0x6000 with the probe
cp "$B2/${NAME}_v2.bin" "$HERE/images/frl_oad_v2.bin"     # send over the air
cp "$B1/$NAME.hex" "$HERE/images/frl_oad_v1_app_only.hex" # for the CCFG check only; not flashed
cp "$B2/$NAME.hex" "$HERE/images/frl_oad_v2_app_only.hex"
cp "$MCUBOOT" "$HERE/images/mcuboot_dual_image.hex"

{
  echo "# Image set, built $(date '+%Y-%m-%d %H:%M') on $(sw_vers -productName) $(sw_vers -productVersion)"
  echo "# SDK $(basename "$SDK"), $(basename "$CLANG"), SysConfig $(basename "$(dirname "$SYSCFG")")"
  echo
  echo "## Files (bytes, SHA-256)"
  for f in mcuboot_dual_image.hex frl_oad_v1.bin frl_oad_v2.bin; do
    printf '%-26s %8d  %s\n' "$f" "$(stat -f %z "$HERE/images/$f")" "$(shasum -a 256 "$HERE/images/$f" | cut -d' ' -f1)"
  done
  echo
  echo "## Linker memory summary"
  for v in 1 2; do
    echo "v$v:"; grep -A8 "MEMORY CONFIGURATION" "$HERE/build/v$v/freertos/ticlang/$NAME.map" | grep -E "^\s+(FLASH|SRAM|CCFG|MCUBOOT|APP|NVS)" || true
  done
  echo
  echo "## CCFG lock fields (the CCFG that reaches the chip is MCUboot's; the app .bin has none)"
  python3 "$HERE/images.py" ccfg "$HERE/images/mcuboot_dual_image.hex" "$HERE/images/frl_oad_v1_app_only.hex" "$HERE/images/frl_oad_v2_app_only.hex"
  echo
  echo "## MCUboot holds the public key that matches the signing key"
  python3 "$HERE/images.py" haskey "$HERE/images/mcuboot_dual_image.hex" "$SDK/source/third_party/mcuboot/boot/ti/source/mcuboot_app/keys.c"
  echo
  echo "## Dry run of the Mac OAD client on both images"
  "$PY" "$REPO/tools/oad/oad_client.py" --dry-run --keys-c "$SDK/source/third_party/mcuboot/boot/ti/source/mcuboot_app/keys.c" "$HERE/images/frl_oad_v1.bin"
  echo
  "$PY" "$REPO/tools/oad/oad_client.py" --dry-run --keys-c "$SDK/source/third_party/mcuboot/boot/ti/source/mcuboot_app/keys.c" "$HERE/images/frl_oad_v2.bin"
} 2>&1 | tee "$HERE/images.txt"
