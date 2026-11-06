#!/bin/bash
# Reset and run the LP-MSPM0G5187 (probe MG020001) with OpenOCD, as the bench scripts do.
OCD=$HOME/zephyr-toolchain/xpack-openocd-0.12.0-7; CFG=$HOME/zephyrproject/zephyr/boards/ti/lp_mspm0g3507/support/openocd.cfg
"$OCD/bin/openocd" -s "$OCD/openocd/scripts" -f "$CFG" -c "adapter serial ${PROBE:?set PROBE to the probe serial}" -c "gdb_port disabled; telnet_port disabled; tcl_port disabled" -c init -c "reset run" -c shutdown > /dev/null 2>&1
