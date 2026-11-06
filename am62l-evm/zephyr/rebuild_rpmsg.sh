#!/bin/bash
# Image F (build-ddr-core1-rpmsg): the Linux link, apps/core1-rpmsg, linked in DDR at 0xE0000000 like image E, plus the 1 MB shared window at
# 0xEFF00000 (core1-shm.overlay). Needs the GIC patch applied (see rebuild_core1.sh) and the repo's am62l-evm/rpmsg folder copied to
# ~/am62l-zephyr-setup/rpmsg. Run on the build host.
set -u
cd ${ZP:-$HOME/zephyr-am62l} && . .venv/bin/activate
export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE=${TC:-$HOME/zephyr-toolchain}/aarch64-zephyr-elf/bin/aarch64-zephyr-elf-
H=~/am62l-zephyr-setup; APP=$H/apps/core1-rpmsg
cd zephyr
west build -p always -b am62l_evm/am62l3/a53 "$APP" -d ../build-ddr-core1-rpmsg -- -DEXTRA_DTC_OVERLAY_FILE="$H/core1-ddr.overlay;$H/core1-shm.overlay" -DEXTRA_CONF_FILE="$H/core1-ns.conf;$H/core1.conf;$H/core1-ring.conf;$H/core1-ddr.conf${EXTRA:-}" > ../build-ddr-core1-rpmsg.log 2>&1; R=$?
echo "== build-ddr-core1-rpmsg: exit $R"; grep -E "^Memory region|^ +[A-Za-z_0-9]+: +[0-9]|overflowed|devicetree error|error:|Error|warning:" ../build-ddr-core1-rpmsg.log | head -12 | cut -c1-240
T=${TC:-$HOME/zephyr-toolchain}/aarch64-zephyr-elf/bin/aarch64-zephyr-elf-
[ $R -eq 0 ] && ${T}size ../build-ddr-core1-rpmsg/zephyr/zephyr.elf && ${T}readelf -h ../build-ddr-core1-rpmsg/zephyr/zephyr.elf | grep Entry && ls -l ../build-ddr-core1-rpmsg/zephyr/zephyr.bin && shasum -a 256 ../build-ddr-core1-rpmsg/zephyr/zephyr.bin
