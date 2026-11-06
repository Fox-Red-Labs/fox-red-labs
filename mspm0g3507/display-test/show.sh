#!/bin/bash
# Put a saved screen on the display:  ./show.sh foxred   (names: ls private/screens/*.h)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
OCD=~/zephyr-toolchain/xpack-openocd-0.12.0-7
CFG=~/zephyrproject/zephyr/boards/ti/lp_mspm0g3507/support/openocd.cfg
"$OCD/bin/openocd" -s "$OCD/openocd/scripts" -c "gdb_port disabled" -c "tcl_port disabled" -c "telnet_port disabled" \
  -f "$CFG" -c "adapter serial ${PROBE:-MG350001}" -c init -c "reset halt" \
  -c "program $HERE/out/screen_$1/screen_$1.out verify" -c "reset run" -c shutdown 2>&1 | grep -E "Verified OK|Error: [^c]" || true
