#!/bin/bash
# Zephyr blinky on an LP-EM-CC2340R5 with its XDS110 probe, from an Intel Mac with no Homebrew.
# What worked on 2026-10-05 (macOS 15.7.9, Zephyr 4.4.2, UniFlash 9.6.0).
# About 1.5 minutes if Zephyr is already installed, about 4 from nothing.
# Needs: python3.12, git, ninja on PATH, and UniFlash 9.6.0 plus CCStudio 21.0.1 under /Applications/ti.
# Uses about 2 GB of disk for a new install. Steps that differ from the official guides are marked OVERRIDE.
# Optional settings, all environment variables:
#   ZP=<dir>            where Zephyr goes (default ~/zephyrproject). An existing install there is reused.
#   TC=<dir>            where the compiler goes (default ~/zephyr-toolchain). An existing one is reused.
#   PROBE_SERIAL=<sn>   flash only the board whose debug probe has this serial number.
#                       Set it whenever two boards are plugged in. List probes with:
#                       /Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e
#   NO_FLASH=1          stop after the build
set -euo pipefail
ZP="${ZP:-$HOME/zephyrproject}"; TC="${TC:-$HOME/zephyr-toolchain}"
PROBE_SERIAL="${PROBE_SERIAL:-}"
HERE="$(cd "$(dirname "$0")" && pwd)"
XDS=/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110
DSLITE=/Applications/ti/uniflash_9.6.0/dslite.sh
mkdir -p "$ZP" "$TC"; cd "$ZP"

# 1. Build tools. OVERRIDE: pip instead of Homebrew for west and CMake.
[ -d .venv ] || python3.12 -m venv .venv
. .venv/bin/activate
if [ ! -d .west ]; then
  pip install -q --upgrade pip west cmake
  # 2. Zephyr at a pinned release, shallow, and only the modules this board needs.
  west init -m https://github.com/zephyrproject-rtos/zephyr --mr v4.4.2 --clone-opt=--depth=1 .
  west update --narrow -o=--depth=1 hal_ti cmsis cmsis_6 picolibc
  pip install -q -r zephyr/scripts/requirements-base.txt
fi

# 3. Compiler. OVERRIDE: Zephyr SDK 1.0.1 has no Intel macOS build, so `west sdk install` cannot work here.
#    Use the Arm compiler from SDK 0.17.4 through the cross-compile setting. Not an officially supported pairing.
if [ ! -x "$TC/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc" ]; then
  cd "$TC"
  curl -sfLO https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.17.4/toolchain_macos-x86_64_arm-zephyr-eabi.tar.xz
  echo "25caef36014ee9597d60da3020511f7298f53b08663a0adce33a58d0a9e251bc  toolchain_macos-x86_64_arm-zephyr-eabi.tar.xz" | shasum -a 256 -c
  tar xf toolchain_macos-x86_64_arm-zephyr-eabi.tar.xz && rm toolchain_macos-x86_64_arm-zephyr-eabi.tar.xz
fi
export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile
export CROSS_COMPILE="$TC/arm-zephyr-eabi/bin/arm-zephyr-eabi-"

# 4. Build. The file to flash is zephyr_crc32.hex, not zephyr.hex: it has the checksums the chip's boot code checks.
cd "$ZP/zephyr"
west build -p always -b lp_em_cc2340r5 samples/basic/blinky -d ../build-cc2340-blinky
[ -n "${NO_FLASH:-}" ] && { echo "NO_FLASH set. Built, not flashed."; exit 0; }

# Refuse to flash if the board is not there, or if it is ambiguous which board is meant.
PROBES=$("$XDS/xdsdfu" -e 2>/dev/null | awk '/Serial Num/ {print $3}')
COUNT=$(printf '%s\n' "$PROBES" | grep -c . || true)
if [ -n "$PROBE_SERIAL" ] && ! printf '%s\n' "$PROBES" | grep -qx "$PROBE_SERIAL"; then
  echo "Probe $PROBE_SERIAL is not connected. Connected probes: ${PROBES:-none}. Built, not flashed. Plug the board in and re-run."; exit 3
fi
if [ -z "$PROBE_SERIAL" ]; then
  if [ "$COUNT" -ne 1 ]; then
    echo "Found $COUNT debug probes (${PROBES:-none}). Plug in exactly one board, or set PROBE_SERIAL to the one you mean. Built, not flashed."; exit 3
  fi
  PROBE_SERIAL="$PROBES"
fi

# 5. Flash. OVERRIDE: `west flash` for this board only knows J-Link. Use UniFlash with the XDS110 probe.
#    The connection file names the probe by serial number. No verify: it reports one mismatch at
#    0x4E020750 on this chip with every image tried, and the programs run.
CCXML="$ZP/build-cc2340-blinky/CC2340R5-$PROBE_SERIAL.ccxml"
sed "s/LS470GTU/$PROBE_SERIAL/g" "$HERE/../CC2340R5.ccxml" > "$CCXML"
"$DSLITE" --mode flash -c "$CCXML" -e -f -u ../build-cc2340-blinky/zephyr/zephyr_crc32.hex 2>&1 | grep -v -E "Mapping registers|Building search|Applying virtual|Initializing|Executing Startup" | tee ../build-cc2340-blinky/flash.log
if grep -q -E "^(fatal|Failed|error)" ../build-cc2340-blinky/flash.log; then
  echo "Flash failed. If the error is -615 (SWD header), pull the probe off the board and press it back on firmly."; exit 4
fi

# 6. Reset through the same probe so the program starts clean.
"$XDS/xds110reset" -s "$PROBE_SERIAL" >/dev/null
echo "Flashed probe $PROBE_SERIAL. The green LED should now be one second on, one second off."
echo "The serial port /dev/cu.usbmodem${PROBE_SERIAL}1 at 115200 prints \"LED state: ON/OFF\" every second."
