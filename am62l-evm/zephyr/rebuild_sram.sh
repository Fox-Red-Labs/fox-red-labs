#!/bin/bash
# Rebuild the two SRAM images on the build host with the overlays in ~/am62l-zephyr-setup (fast loop; setup.sh is the from-nothing path).
set -u
cd ${ZP:-$HOME/zephyr-am62l} && . .venv/bin/activate
export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE=${TC:-$HOME/zephyr-toolchain}/aarch64-zephyr-elf/bin/aarch64-zephyr-elf-
H=~/am62l-zephyr-setup; APP=${APP:-samples/hello_world}
cd zephyr
for v in main all; do
  west build -p always -b am62l_evm/am62l3/a53 "$APP" -d ../build-sram-$v -- -DEXTRA_DTC_OVERLAY_FILE="$H/sram-$v.overlay" -DEXTRA_CONF_FILE="$H/bare-el3.conf" > ../build-sram-$v.log 2>&1; R=$?
  echo "== build-sram-$v: exit $R"
  grep -E "^Memory region|^ +[A-Za-z_0-9]+: +[0-9]|overflowed|devicetree error|error:|Error" ../build-sram-$v.log | head -12 | cut -c1-240
  [ $R -eq 0 ] && ${TC:-$HOME/zephyr-toolchain}/aarch64-zephyr-elf/bin/aarch64-zephyr-elf-size ../build-sram-$v/zephyr/zephyr.elf
done
