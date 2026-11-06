#!/bin/bash
# Independent proof that an LP-MSPM0G3507 is blinking: read the LED pin three times, a second apart.
#   led_pin_check.sh <probe-serial> [pin-bit]     default bit 22 = PB22, the blue LED Zephyr's blinky uses
# Prints three DOUT values for GPIOB. A blinking LED shows the bit changing. Halts the chip for a moment each read.
# Needs the OpenOCD installed by mspm0g3507/zephyr/setup-intel-mac.sh, and no other debug session on the probe.
set -euo pipefail
SERIAL="${1:?probe serial number, from xdsdfu -e}"
OCD="${OCD:-$HOME/zephyr-toolchain/xpack-openocd-0.12.0-7}"
CFG="${CFG:-$HOME/zephyrproject/zephyr/boards/ti/lp_mspm0g3507/support/openocd.cfg}"
R='echo "DOUT [mdw 0x400A3280 1]"'
"$OCD/bin/openocd" -s "$OCD/openocd/scripts" -f "$CFG" -c "adapter serial $SERIAL" \
  -c "init; halt; $R; resume; sleep 1000; halt; $R; resume; sleep 1000; halt; $R; resume; shutdown" 2>&1 | grep -E "^DOUT|rror"
