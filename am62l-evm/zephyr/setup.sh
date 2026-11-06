#!/bin/bash
# Zephyr on the TI AM62L EVM (TMDS62LEVM, Cortex-A53) from a clean machine: macOS (Intel or Apple silicon) or Linux
# (x86_64 or arm64). Every download is pinned and checksummed. Builds three images and stops (nothing to flash from a
# script: the board is loaded over U-Boot or JTAG, see README.md):
#   build-ddr        upstream board file as is: Zephyr in LPDDR4 at 0x82000000, started by U-Boot ("go"), under TF-A
#   build-sram-main  Zephyr linked into the 80 KB of MAIN on-chip SRAM the boot ROM leaves free, no DDR, no TF-A (EL3), one core
#   build-sram-all   same, into the 144 KB contiguous span of WKUP plus MAIN on-chip SRAM
# Settings (environment variables): ZP (Zephyr dir, default ~/zephyr-am62l), TC (tools dir, default ~/zephyr-toolchain),
# APP (application dir, default samples/hello_world).
# Modeled on mspm0g3507/zephyr/setup.sh (2026-10-18). Differences: the aarch64 compiler, upstream Zephyr at a
# pinned commit (the am62l_evm board file is newer than any 4.4 release), no OpenOCD (0.12.0 predates AM62L support;
# JTAG goes through CCStudio's DSLite, which knows the chip).
set -euo pipefail
ZP="${ZP:-$HOME/zephyr-am62l}"; TC="${TC:-$HOME/zephyr-toolchain}"; APP="${APP:-samples/hello_world}"
HERE="$(cd "$(dirname "$0")" && pwd)"
case "$ZP$TC" in *" "*) echo "No spaces in ZP or TC: Zephyr's Kconfig step splits the path (\"File not found: /mnt/My\"). A symlink does not help (west and the venv resolve the real path). On a drive whose name has a space, make a disk image on it and build there: hdiutil create -size 16g -fs APFS -type SPARSEBUNDLE -volname zephyr <drive>/zephyr.sparsebundle; hdiutil attach <drive>/zephyr.sparsebundle; ZP=<mount>/zephyr-am62l TC=<mount>/zephyr-toolchain"; exit 2 ;; esac
mkdir -p "$ZP" "$TC"
START=$(date +%s)
NEED_KB=3145728; [ -d "$ZP/zephyr" ] && [ -x "$TC/aarch64-zephyr-elf/bin/aarch64-zephyr-elf-gcc" ] && NEED_KB=307200
for d in "$ZP" "$TC"; do
  FREE_KB=$(df -Pk "$d" | awk 'NR==2 {print $4}')
  [ "${FREE_KB:-0}" -ge "$NEED_KB" ] || { echo "Need $(( NEED_KB / 1048576 )) GB free in $d, found $(( ${FREE_KB:-0} / 1024 )) MB. Set ZP and TC to a drive with room."; exit 2; }
done
OS=$(uname -s); ARCH=$(uname -m)
case "$OS-$ARCH" in
  Darwin-x86_64)  ZTC=macos-x86_64;  ZSHA=2f2940b245eee2ba61f89987f7ee49d8c39c61313677122ea9c9399379112604
                  UTC=x86_64-apple-darwin;       USHA=bb7b805bdcf3d0bc8c74ceb94569e6b3e8aa35bd492acdbb9d46bf46d9d4ff6b ;;
  Darwin-arm64)   ZTC=macos-aarch64; ZSHA=189b14f871513d2c9aaa38beee95462d6e62a161c212a076ec1413224295313f
                  UTC=aarch64-apple-darwin;      USHA=cfe454573412ff7380d15fc7dae5997ab53ffce72fce0a78e91ddbc84001acd2 ;;
  Linux-x86_64)   ZTC=linux-x86_64;  ZSHA=d3c3790cad0b7fe579dc892d7f5e84e07f67008f04f1334ed6f95a3db3f4e608
                  UTC=x86_64-unknown-linux-gnu;  USHA=eadd9d27c134ab5ac1ffd4473b97a741407f3ea238288f03121fc4deddf90220 ;;
  Linux-aarch64|Linux-arm64) ZTC=linux-aarch64; ZSHA=ab177c3c038815651eb9b28066da61ab7a7ccbda642f9a021373198ac513b555
                  UTC=aarch64-unknown-linux-gnu; USHA=9041c73c99fdc5892cf128140beb354eca9231d502068cc36e6cb53a8e59ad14 ;;
  *) echo "Unsupported machine: $OS $ARCH."; exit 2 ;;
esac
if command -v shasum >/dev/null; then SHA="shasum -a 256"; else SHA="sha256sum"; fi
for t in curl tar git; do command -v $t >/dev/null || { echo "Missing $t."; exit 2; }; done
echo "Machine: $OS $ARCH. Zephyr goes in $ZP, tools in $TC."

# 1. Python 3.12, west, CMake, ninja (no Homebrew, no apt; uv brings Python when the machine has none).
cd "$ZP"
if [ -d .venv ]; then :
elif command -v python3.12 >/dev/null; then python3.12 -m venv .venv
else
  export UV_PYTHON_INSTALL_DIR="$TC/python" UV_NO_MODIFY_PATH=1
  if [ ! -x "$TC/uv/uv" ]; then
    UF=uv-${UTC}.tar.gz
    ( cd "$TC" && curl -sfLO "https://github.com/astral-sh/uv/releases/download/0.13.1/$UF" && echo "$USHA  $UF" | $SHA -c && tar xzf "$UF" && rm "$UF" && mv "uv-${UTC}" uv )
  fi
  "$TC/uv/uv" python install 3.12 --quiet
  "$TC/uv/uv" venv --python 3.12 --quiet .venv
fi
. .venv/bin/activate
python -m ensurepip --upgrade >/dev/null 2>&1 || true
python -m pip install -q --upgrade pip "west==1.5.0" "cmake==4.4.4" "ninja==1.13.2"

# 2. Upstream Zephyr at a pinned commit (main, 2026-10-18; am62l_evm with OSPI, SCMI, PWM: no release has all of it yet).
ZREV=a915b2f12675b48b16d56653ea145dcd8f20cbbf
if [ ! -d zephyr ]; then
  west init -m https://github.com/zephyrproject-rtos/zephyr --mr main .
  ( cd zephyr && git fetch -q --depth=1 origin "$ZREV" && git checkout -q "$ZREV" )
fi
west update --narrow -o=--depth=1 cmsis cmsis_6 hal_ti picolibc
pip install -q -r zephyr/scripts/requirements-base.txt

# 3. Compiler: the aarch64 toolchain from Zephyr SDK 0.17.4, used through the cross-compile setting.
cd "$TC"
ZF=toolchain_${ZTC}_aarch64-zephyr-elf.tar.xz
if [ ! -x "$TC/aarch64-zephyr-elf/bin/aarch64-zephyr-elf-gcc" ]; then
  curl -sfLO "https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.17.4/$ZF"
  echo "$ZSHA  $ZF" | $SHA -c
  tar xf "$ZF" && rm "$ZF"
fi
export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile
export CROSS_COMPILE="$TC/aarch64-zephyr-elf/bin/aarch64-zephyr-elf-"

# 4. Build the three images. A failed SRAM build is a result, not a stop: the DDR image is built first and kept.
cd "$ZP/zephyr"
report() { # name: print the memory lines west prints at the end of a build
  grep -E "^Memory region|^ +(SRAM|RAM|DDR|FLASH|SRAM0|sram|ddr)[A-Za-z0-9_]*:|region .* overflowed" "$1" | sed "s/^/  /"; }
west build -p always -b am62l_evm/am62l3/a53 "$APP" -d ../build-ddr 2>&1 | tee ../build-ddr.log | tail -3
set +e
west build -p always -b am62l_evm/am62l3/a53 "$APP" -d ../build-sram-main -- -DEXTRA_DTC_OVERLAY_FILE="$HERE/sram-main.overlay" -DEXTRA_CONF_FILE="$HERE/bare-el3.conf" > ../build-sram-main.log 2>&1; R1=$?
west build -p always -b am62l_evm/am62l3/a53 "$APP" -d ../build-sram-all  -- -DEXTRA_DTC_OVERLAY_FILE="$HERE/sram-all.overlay"  -DEXTRA_CONF_FILE="$HERE/bare-el3.conf" > ../build-sram-all.log 2>&1;  R2=$?
set -e
echo "build-ddr: OK"; report ../build-ddr.log
echo "build-sram-main (80 KB, bare EL3): exit $R1"; report ../build-sram-main.log; [ $R1 -ne 0 ] && grep -E "error|Error|overflowed" ../build-sram-main.log | head -5 | sed "s/^/  /"
echo "build-sram-all (144 KB, bare EL3): exit $R2";  report ../build-sram-all.log;  [ $R2 -ne 0 ] && grep -E "error|Error|overflowed" ../build-sram-all.log  | head -5 | sed "s/^/  /"
echo "Done in $(( $(date +%s) - START )) s. Images: $ZP/build-*/zephyr/zephyr.{elf,bin}"
