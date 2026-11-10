#!/bin/bash
# Zephyr blinky on an LP-MSPM0G3507 from a clean machine: macOS (Intel or Apple silicon) or Linux (x86_64 or arm64).
# Windows: run it inside WSL (the Linux path); not tested here. Every download is pinned and checksummed.
# No administrator password, no Homebrew: Python 3.12 comes from uv into the toolchain folder when the machine
# has none. 2.1 GB of disk, checked before anything is downloaded. From nothing installed, build only: 2 min 48 s on a
# 2018 Intel MacBook, about 1 min 15 s on an M4 Mac mini (2026-10-18). The flash adds 7 s.
#
# Settings, all environment variables:
#   ZP=<dir>            where Zephyr goes (default ~/zephyrproject)
#   TC=<dir>            where the compiler, OpenOCD, uv and Python go (default ~/zephyr-toolchain)
#   PROBE_SERIAL=<sn>   flash only the board whose debug probe has this serial number. Set it whenever two
#                       boards are plugged in. With one probe the script finds its serial itself and pins it.
#   NO_FLASH=1          stop after the build (no board needed)
#   APP=<dir>           build this Zephyr application instead of samples/basic/blinky (for example red-blink)
#
# Grew out of setup-intel-mac.sh (2026-10-05, Intel Mac only). Four steps still differ from Zephyr's own
# getting-started guide; each is marked OVERRIDE.
set -euo pipefail
ZP="${ZP:-$HOME/zephyrproject}"; TC="${TC:-$HOME/zephyr-toolchain}"
PROBE_SERIAL="${PROBE_SERIAL:-}"; APP="${APP:-}"
mkdir -p "$ZP" "$TC"
START=$(date +%s)
# Enough room? 2.1 GB is used the first time; ask for 2.5. A repeat with the tools and Zephyr already in place needs 0.3.
# Checked first so a half-install never happens.
NEED_KB=2621440; [ -d "$ZP/zephyr" ] && [ -x "$TC/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc" ] && NEED_KB=307200
for d in "$ZP" "$TC"; do
  FREE_KB=$(df -Pk "$d" | awk 'NR==2 {print $4}')
  [ "${FREE_KB:-0}" -ge "$NEED_KB" ] || { echo "Need $(( NEED_KB / 1048576 )).$(( NEED_KB % 1048576 * 10 / 1048576 )) GB free in $d, found $(( ${FREE_KB:-0} / 1024 )) MB. Free some space, or set ZP and TC to a folder on a drive with room."; exit 2; }
done

# 0. Which machine. The four supported combinations and their pinned, checksummed downloads.
OS=$(uname -s); ARCH=$(uname -m)
case "$OS-$ARCH" in
  Darwin-x86_64)  ZTC=macos-x86_64;  ZSHA=25caef36014ee9597d60da3020511f7298f53b08663a0adce33a58d0a9e251bc
                  OTC=darwin-x64;    OSHA=668ad25350103a4357e11629ec833eae5982e973889ce25bad0c2963e37fa8bf
                  UTC=x86_64-apple-darwin;       USHA=bb7b805bdcf3d0bc8c74ceb94569e6b3e8aa35bd492acdbb9d46bf46d9d4ff6b ;;
  Darwin-arm64)   ZTC=macos-aarch64; ZSHA=c0fbf111d2232dd533d813f49ddfbcc45014c4e2da7f2a17bf80599ac5bfbdeb
                  OTC=darwin-arm64;  OSHA=667342c086984f3e5a55b4e0d5f711add13fb04de040fca493303000e6c19327
                  UTC=aarch64-apple-darwin;      USHA=cfe454573412ff7380d15fc7dae5997ab53ffce72fce0a78e91ddbc84001acd2 ;;
  Linux-x86_64)   ZTC=linux-x86_64;  ZSHA=1b61084fe12076a3639aa2d028b5258f41eb3d0f7d46a16264949e1d213f1915
                  OTC=linux-x64;     OSHA=94b3790983beaf8ed57e646c0620dd66d705fddae03d290823a6ed3b439468d6
                  UTC=x86_64-unknown-linux-gnu;  USHA=eadd9d27c134ab5ac1ffd4473b97a741407f3ea238288f03121fc4deddf90220 ;;
  Linux-aarch64|Linux-arm64) ZTC=linux-aarch64; ZSHA=697300f3426db31386537d02b56fc5f4de4b63a4b4da3c5492374c4da269d5d7
                  OTC=linux-arm64;   OSHA=db73a3ab91c556ecec2405a7e02d404b11139df6aba1031cad94a7e6766d06cc
                  UTC=aarch64-unknown-linux-gnu; USHA=9041c73c99fdc5892cf128140beb354eca9231d502068cc36e6cb53a8e59ad14 ;;
  *) echo "Unsupported machine: $OS $ARCH. Supported: macOS or Linux on x86_64 or arm64 (Windows: use WSL)."; exit 2 ;;
esac
if command -v shasum >/dev/null; then SHA="shasum -a 256"; else SHA="sha256sum"; fi   # macOS 26 ships a sha256sum without -c
for t in curl tar git; do command -v $t >/dev/null || { echo "Missing $t. On macOS run: xcode-select --install. On Debian or Ubuntu: sudo apt install curl tar git"; exit 2; }; done
echo "Machine: $OS $ARCH. Zephyr goes in $ZP, tools in $TC."

# 1. Python 3.12 and the build tools. OVERRIDE: no Homebrew, no apt. If the machine has no python3.12, uv installs
#    one into $TC (no administrator password). west and CMake come from pip; dtc is skipped, the build runs without it.
cd "$ZP"
if [ -d .venv ]; then
  :
elif command -v python3.12 >/dev/null; then
  python3.12 -m venv .venv
else
  export UV_PYTHON_INSTALL_DIR="$TC/python" UV_NO_MODIFY_PATH=1
  if [ ! -x "$TC/uv/uv" ]; then   # uv 0.13.1 as a pinned, checksummed release tarball, like the compiler and OpenOCD, not a curl-to-shell installer
    UF=uv-${UTC}.tar.gz
    ( cd "$TC" && curl -sfLO "https://github.com/astral-sh/uv/releases/download/0.13.1/$UF" && echo "$USHA  $UF" | $SHA -c && tar xzf "$UF" && rm "$UF" && mv "uv-${UTC}" uv )
  fi
  "$TC/uv/uv" python install 3.12 --quiet
  "$TC/uv/uv" venv --python 3.12 --quiet .venv
fi
. .venv/bin/activate
python -m ensurepip --upgrade >/dev/null 2>&1 || true
python -m pip install -q --upgrade pip "west==1.5.0" "cmake==4.4.4" "ninja==1.13.2"

# 2. Zephyr at a pinned release, shallow, and only the modules this board needs.
if [ ! -d zephyr ]; then
  west init -m https://github.com/zephyrproject-rtos/zephyr --mr v4.4.2 --clone-opt=--depth=1 .
fi
west update --narrow -o=--depth=1 hal_ti cmsis cmsis_6 picolibc
pip install -q -r zephyr/scripts/requirements-base.txt

# 3. Compiler. OVERRIDE: Zephyr SDK 1.0.1 has no macOS build for every chip here, so `west sdk install` is not used.
#    The Arm compiler from SDK 0.17.4 through the cross-compile setting works on all four machines; not an officially supported pairing.
cd "$TC"
ZF=toolchain_${ZTC}_arm-zephyr-eabi.tar.xz
if [ ! -x "$TC/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc" ]; then
  curl -sfLO "https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.17.4/$ZF"
  echo "$ZSHA  $ZF" | $SHA -c
  tar xf "$ZF" && rm "$ZF"
fi
export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile
export CROSS_COMPILE="$TC/arm-zephyr-eabi/bin/arm-zephyr-eabi-"

# 4. Flashing tool. OVERRIDE: a prebuilt xPack OpenOCD instead of building OpenOCD from source. It already knows the chip and the XDS110 probe.
OF=xpack-openocd-0.12.0-7-${OTC}.tar.gz
OCD="$TC/xpack-openocd-0.12.0-7"
if [ ! -x "$OCD/bin/openocd" ]; then
  curl -sfLO "https://github.com/xpack-dev-tools/openocd-xpack/releases/download/v0.12.0-7/$OF"
  echo "$OSHA  $OF" | $SHA -c
  tar xzf "$OF" && rm "$OF"
fi

# 5. Build.
cd "$ZP/zephyr"
if [ -n "$APP" ]; then west build -p always -b lp_mspm0g3507 "$APP" -d ../build-blinky; else west build -p always -b lp_mspm0g3507 samples/basic/blinky -d ../build-blinky; fi
echo "Built in $(( $(date +%s) - START )) s."
[ -n "${NO_FLASH:-}" ] && { echo "NO_FLASH set. Built, not flashed."; exit 0; }

# Refuse to flash if the board is not there, or if it is ambiguous which board is meant. Probe serials: CCStudio's
# xdsdfu when it is installed, else the USB list (system_profiler on macOS, /sys on Linux). The serial is always passed on.
XDSDFU="${FRL_XDSDFU:-$(python3 "$(cd "$(dirname "$0")/../.." && pwd)/tools/frl/toolpath.py" xdsdfu --optional 2>/dev/null)}"   # CCStudio or UniFlash; absent is fine, the USB list is used
probe_serials() {
  if [ -x "$XDSDFU" ]; then "$XDSDFU" -e 2>/dev/null | awk '/Serial Num/ {print $3}'; return; fi
  if [ "$OS" = Linux ]; then
    for d in /sys/bus/usb/devices/*; do
      [ -f "$d/idVendor" ] && [ "$(cat "$d/idVendor")" = 0451 ] && [ "$(cat "$d/idProduct")" = bef3 ] && cat "$d/serial"
    done 2>/dev/null; return
  fi
  system_profiler SPUSBDataType -json 2>/dev/null | python3 -I -c '
import json, sys
try: tree = json.load(sys.stdin)
except Exception: sys.exit(0)
def walk(node):
    for x in node.get("_items", []):
        vid = str(x.get("vendor_id", "")).split(" ")[0].lower(); pid = str(x.get("product_id", "")).lower()
        if vid == "0x0451" and pid == "0xbef3" and x.get("serial_num"): print(x["serial_num"])
        walk(x)
for top in tree.get("SPUSBDataType", []): walk(top)'
}
PROBES=$(probe_serials || true)
COUNT=$(printf '%s\n' "$PROBES" | grep -c . || true)
if [ -n "$PROBE_SERIAL" ]; then
  if [ "$COUNT" -gt 0 ] && ! printf '%s\n' "$PROBES" | grep -qx "$PROBE_SERIAL"; then
    echo "Probe $PROBE_SERIAL is not connected. Connected probes: $(printf '%s' "$PROBES" | tr '\n' ' '). Built, not flashed. Plug the board in and re-run."; exit 3
  fi
elif [ "$COUNT" -eq 1 ]; then PROBE_SERIAL="$PROBES"
elif [ "$COUNT" -eq 0 ]; then echo "No XDS110 debug probe found on USB. Built, not flashed. Plug the LaunchPad in (a data cable, not a charge-only one) and re-run."; exit 3
else echo "Found $COUNT debug probes (${PROBES:-none}). Plug in exactly one board, or set PROBE_SERIAL to the one you mean. Built, not flashed."; exit 3
fi
if [ "$OS" = Linux ] && [ ! -w "$(ls -d /dev/bus/usb/*/* 2>/dev/null | head -1)" ] && [ ! -f /etc/udev/rules.d/99-xds110.rules ]; then
  echo "Linux needs one udev rule so a user can open the probe (once, with sudo):"
  echo '  echo '"'"'ATTRS{idVendor}=="0451", ATTRS{idProduct}=="bef3", MODE="0666"'"'"' | sudo tee /etc/udev/rules.d/99-xds110.rules && sudo udevadm control --reload && sudo udevadm trigger'
  echo "Then unplug and replug the board and re-run."
fi
west flash -d ../build-blinky --openocd "$OCD/bin/openocd" --openocd-search "$OCD/openocd/scripts" --cmd-pre-init "adapter serial $PROBE_SERIAL"

# 6. OVERRIDE: board-level reset. After a debugger soft reset the chip sat in Zephyr's NMI handler and did not blink.
"$OCD/bin/openocd" -s "$OCD/openocd/scripts" -f boards/ti/lp_mspm0g3507/support/openocd.cfg -c "adapter serial $PROBE_SERIAL" -c "init; mspm0_board_reset; shutdown"
echo "The blue LED should now blink once a second (red, every half second, with APP=red-blink). $(( $(date +%s) - START )) s in all."
