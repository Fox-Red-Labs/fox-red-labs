# Shared by the digit recipes' scripts (touch/build.sh, touch/flash.sh, mspm0/build.sh, mspm0/run.sh). Source it; do not run it.
# One set of variable names everywhere. Set any of them in your terminal; paths may contain spaces.
#   SDK      MSPM0 SDK folder                  (default: the one in the TI install folder, or FRL_MSPM0_SDK)
#   CC       TI Arm Clang folder               (default: standalone or inside CCStudio, or FRL_TICLANG)
#   NM       the symbol tool run.sh uses       (default $CC/bin/tiarmnm, which ships with TI Arm Clang)
#   OCD      xPack OpenOCD folder              (default ~/zephyr-toolchain/xpack-openocd-0.12.0-7)
#   OCD_CFG  OpenOCD connection file           (default openocd-mspm0.cfg beside this file)
#   PROBE    your LaunchPad kit's XDS110 serial    (no default: flashing refuses to guess)
BR_HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Defaults found the way frl finds them (tools/frl/toolpath.py): the TI install folders on this computer, or FRL_MSPM0_SDK and
# FRL_TICLANG; the standalone compiler counts as well as the copy inside CCStudio. SDK and CC set in the terminal still win.
BR_TP="python3 $BR_HERE/../../tools/frl/toolpath.py"
SDK="${SDK:-${FRL_MSPM0_SDK:-$($BR_TP mspm0_sdk --path 2>/dev/null)}}"
CC="${CC:-${FRL_TICLANG:-$($BR_TP ticlang --version 5.1.1.LTS --path 2>/dev/null)}}"
NM="${NM:-$CC/bin/tiarmnm}"
OCD="${OCD:-$HOME/zephyr-toolchain/xpack-openocd-0.12.0-7}"
OCD_CFG="${OCD_CFG:-$BR_HERE/openocd-mspm0.cfg}"
BR_MISSING=()

br_need_path() {  # label, path, how to fix
  [ -e "$2" ] || BR_MISSING+=("$1: not found at \"$2\". $3")
}
br_need_cmd() {   # label, command, how to fix
  command -v "$2" >/dev/null 2>&1 || BR_MISSING+=("$1: \"$2\" is not installed. $3")
}
br_need_exec() {  # label, file, how to fix
  [ -x "$2" ] || BR_MISSING+=("$1: not an executable file at \"$2\". $3")
}
br_need_probe() {
  if [ -z "${PROBE:-}" ]; then
    BR_MISSING+=("PROBE: not set. This is your LaunchPad kit's XDS110 serial number, and the scripts will not guess it, so they never touch a board that is not yours. Plug the board in and run  ls /dev/cu.usbmodem*  : each board lists two ports, and its serial is the text after \"usbmodem\" without the last digit (usbmodemAB1234561 and usbmodemAB1234564 mean AB123456). Or run  system_profiler SPUSBDataType | grep -A6 XDS110  and read the \"Serial Number\" line. With several boards plugged in, unplug the others or pick the right line. Then:  export PROBE=AB123456")
  elif ! [[ "$PROBE" =~ ^[A-Za-z0-9_-]+$ ]]; then
    BR_MISSING+=("PROBE: \"$PROBE\" is not a serial number (letters and digits only). Run  ls /dev/cu.usbmodem*  and drop the \"usbmodem\" and the last digit.")
  fi
}
br_report() {     # prints what is missing and exits 2; when nothing is, prints OK (not with the argument quiet)
  if [ ${#BR_MISSING[@]} -gt 0 ]; then
    echo "Not ready: ${#BR_MISSING[@]} thing(s) missing. Nothing was built, flashed or reset."
    local m; for m in "${BR_MISSING[@]}"; do echo "  - $m"; done
    exit 2
  fi
  [ "${1:-}" = quiet ] || echo "check OK: every tool and setting this script needs was found. Nothing was built, flashed or reset."
}
