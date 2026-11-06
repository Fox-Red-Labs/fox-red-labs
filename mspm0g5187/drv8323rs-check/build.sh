#!/bin/bash
# Build the DRV8323RS cross-check for the LP-MSPM0G5187: TI's spi_controller_multibyte_fifo_poll folder with our .c and .syscfg.
#   ./build.sh            build        ./build.sh run [wait-s]   flash (probe MG020001), wait, read struct ck over OpenOCD
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."; E=spi_controller_multibyte_fifo_poll
W=$PWD/out/mspm0g5187/drv8323rs-check; B=$W/$E/ticlang/$E.out
if [ "${1:-}" != "run" ]; then
  rm -rf "$W"; mkdir -p "$W"
  cp -R /Applications/ti/mspm0_sdk_2_11_00_07/examples/nortos/LP_MSPM0G5187/driverlib/$E "$W/"
  cp "$HERE/check.c" "$W/$E/$E.c"; cp "$HERE/check.syscfg" "$W/$E/$E.syscfg"
  rm -f "$W/$E/ti_msp_dl_config.c" "$W/$E/ti_msp_dl_config.h"
  "$REPO/mspm0g3507/mcu-bench/sdk-common/sdkmake.sh" "$W/$E/ticlang" 2>&1 | tail -15
  exit 0
fi
WAIT=${2:-4}
OCD=$HOME/zephyr-toolchain/xpack-openocd-0.12.0-7; CFG=$HOME/zephyrproject/zephyr/boards/ti/lp_mspm0g3507/support/openocd.cfg
A=$($HOME/zephyr-toolchain/arm-zephyr-eabi/bin/arm-zephyr-eabi-nm "$B" | awk '$3=="ck"{print $1}')
/Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c "$REPO/mspm0g5187/MSPM0G5187_MG020001.ccxml" -e -f -v -u "$B" 2>&1 | grep -E "Success|Error|Fail" | tail -1
"$OCD/bin/openocd" -s "$OCD/openocd/scripts" -f "$CFG" -c "adapter serial ${PROBE:?set PROBE to the probe serial}" -c "gdb_port disabled; telnet_port disabled; tcl_port disabled" -c init -c "sleep $((WAIT*1000))" -c halt -c "mdw 0x$A 18" -c resume -c shutdown 2>&1 | grep -E "^0x"
