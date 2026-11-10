#!/bin/bash
# Superseded 2026-10-18 by setup.sh beside it (macOS Intel or Apple silicon, Linux x86_64 or arm64, no Python 3.12 needed).
# Kept as the exact Intel-only sequence that ran on 2026-10-05; other scripts in this repository still call it.
# Zephyr blinky on an LP-MSPM0G3507 from an Intel Mac with no Homebrew.
# Exactly what worked on 2026-10-05 (macOS 15.7.9, Zephyr 4.4.2). About 9.5 minutes end to end.
# Needs: python3.12, git, ninja on PATH. Uses about 2 GB of disk.
# Four steps differ from Zephyr's own getting-started guide. Each is marked OVERRIDE.
# Optional settings, all environment variables:
#   ZP=<dir>            where Zephyr goes (default ~/zephyrproject)
#   TC=<dir>            where the compiler and OpenOCD go (default ~/zephyr-toolchain)
#   PROBE_SERIAL=<sn>   flash only the board whose debug probe has this serial number.
#                       Set it whenever two boards are plugged in. With one probe the script finds
#                       its serial itself and still pins it. List probes with CCStudio's
#                       /Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e
#                       or, without CCStudio, system_profiler SPUSBDataType (XDS110 is 0451:bef3).
#   NO_FLASH=1          stop after the build
set -euo pipefail
ZP="${ZP:-$HOME/zephyrproject}"; TC="${TC:-$HOME/zephyr-toolchain}"
PROBE_SERIAL="${PROBE_SERIAL:-}"
mkdir -p "$ZP" "$TC"; cd "$ZP"

# 1. Build tools. OVERRIDE: pip instead of Homebrew for west and CMake. dtc is skipped; the build runs without it.
python3.12 -m venv .venv && . .venv/bin/activate
pip install -q --upgrade pip west cmake

# 2. Zephyr at a pinned release, shallow, and only the modules this board needs.
west init -m https://github.com/zephyrproject-rtos/zephyr --mr v4.4.2 --clone-opt=--depth=1 .
west update --narrow -o=--depth=1 hal_ti cmsis cmsis_6 picolibc
pip install -q -r zephyr/scripts/requirements-base.txt

# 3. Compiler. OVERRIDE: Zephyr SDK 1.0.1 has no Intel macOS build, so `west sdk install` cannot work here.
#    Use the Arm compiler from SDK 0.17.4 through the cross-compile setting. Not an officially supported pairing.
cd "$TC"
curl -sfLO https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.17.4/toolchain_macos-x86_64_arm-zephyr-eabi.tar.xz
echo "25caef36014ee9597d60da3020511f7298f53b08663a0adce33a58d0a9e251bc  toolchain_macos-x86_64_arm-zephyr-eabi.tar.xz" | shasum -a 256 -c
tar xf toolchain_macos-x86_64_arm-zephyr-eabi.tar.xz && rm toolchain_macos-x86_64_arm-zephyr-eabi.tar.xz
export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile
export CROSS_COMPILE="$TC/arm-zephyr-eabi/bin/arm-zephyr-eabi-"

# 4. Flashing tool. OVERRIDE: a prebuilt xPack OpenOCD instead of building OpenOCD from source.
curl -sfLO https://github.com/xpack-dev-tools/openocd-xpack/releases/download/v0.12.0-7/xpack-openocd-0.12.0-7-darwin-x64.tar.gz
echo "668ad25350103a4357e11629ec833eae5982e973889ce25bad0c2963e37fa8bf  xpack-openocd-0.12.0-7-darwin-x64.tar.gz" | shasum -a 256 -c
tar xzf xpack-openocd-0.12.0-7-darwin-x64.tar.gz && rm xpack-openocd-0.12.0-7-darwin-x64.tar.gz
OCD="$TC/xpack-openocd-0.12.0-7"

# 5. Build and flash. Close any CCStudio debug session first; it holds the debug probe.
cd "$ZP/zephyr"
west build -p always -b lp_mspm0g3507 samples/basic/blinky -d ../build-blinky
[ -n "${NO_FLASH:-}" ] && { echo "NO_FLASH set. Built, not flashed."; exit 0; }
# Refuse to flash if the board is not there, or if it is ambiguous which board is meant.
# Probe serials come from CCStudio's xdsdfu when it is installed, otherwise from the USB list
# (system_profiler), so the check never silently drops out. The serial is always passed on.
XDSDFU="${FRL_XDSDFU:-$(python3 "$(cd "$(dirname "$0")/../.." && pwd)/tools/frl/toolpath.py" xdsdfu --optional 2>/dev/null)}"   # CCStudio or UniFlash; absent is fine, the USB list is used
probe_serials() {
  if [ -x "$XDSDFU" ]; then
    "$XDSDFU" -e 2>/dev/null | awk '/Serial Num/ {print $3}'
  else
    system_profiler SPUSBDataType -json 2>/dev/null | python3 -I -c '
import json, sys
try:
    tree = json.load(sys.stdin)
except Exception:
    sys.exit(0)
def walk(node):
    for x in node.get("_items", []):
        vid = str(x.get("vendor_id", "")).split(" ")[0].lower()
        pid = str(x.get("product_id", "")).lower()
        if vid == "0x0451" and pid == "0xbef3" and x.get("serial_num"):
            print(x["serial_num"])
        walk(x)
for top in tree.get("SPUSBDataType", []):
    walk(top)
'
  fi
}
PROBES=$(probe_serials || true)
COUNT=$(printf '%s\n' "$PROBES" | grep -c . || true)
if [ -n "$PROBE_SERIAL" ]; then
  if [ "$COUNT" -gt 0 ] && ! printf '%s\n' "$PROBES" | grep -qx "$PROBE_SERIAL"; then
    echo "Probe $PROBE_SERIAL is not connected. Connected probes: $(printf '%s' "$PROBES" | tr '\n' ' '). Built, not flashed. Plug the board in and re-run."; exit 3
  fi
  [ "$COUNT" -gt 0 ] || echo "Could not list debug probes (no xdsdfu, nothing from system_profiler). Flashing the explicitly named probe $PROBE_SERIAL; OpenOCD stops if it is absent."
elif [ "$COUNT" -eq 1 ]; then
  PROBE_SERIAL="$PROBES"
else
  echo "Found $COUNT debug probes (${PROBES:-none}). Plug in exactly one board, or set PROBE_SERIAL to the one you mean. Built, not flashed."; exit 3
fi
PIN=(--cmd-pre-init "adapter serial $PROBE_SERIAL"); RPIN=(-c "adapter serial $PROBE_SERIAL")
west flash -d ../build-blinky --openocd "$OCD/bin/openocd" --openocd-search "$OCD/openocd/scripts" "${PIN[@]}"

# 6. OVERRIDE: board-level reset. After a debugger soft reset the chip sat in Zephyr's NMI handler and did not blink.
"$OCD/bin/openocd" -s "$OCD/openocd/scripts" -f boards/ti/lp_mspm0g3507/support/openocd.cfg "${RPIN[@]}" -c "init; mspm0_board_reset; shutdown"
echo "The blue LED should now blink once a second."
