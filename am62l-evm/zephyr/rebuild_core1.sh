#!/bin/bash
# E242 images on the build host: A = build-sram-core1-uart1, B = build-sram-core1-ring. Needs the GIC patch applied in the workspace
# (patches/gic-skip-distributor.patch, `git apply` in zephyr/). Overlays and apps are read from ~/am62l-zephyr-setup.
set -u
cd ${ZP:-$HOME/zephyr-am62l} && . .venv/bin/activate
export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE=${TC:-$HOME/zephyr-toolchain}/aarch64-zephyr-elf/bin/aarch64-zephyr-elf-
H=~/am62l-zephyr-setup; APP=$H/apps/core1-hello
cd zephyr
git apply --check "$H/patches/gic-skip-distributor.patch" 2>/dev/null && git apply "$H/patches/gic-skip-distributor.patch"
build() { # name overlay conf
  west build -p always -b am62l_evm/am62l3/a53 "$APP" -d ../build-sram-core1-$1 -- -DEXTRA_DTC_OVERLAY_FILE="$H/sram-all.overlay;$H/$2" -DEXTRA_CONF_FILE="$H/bare-el3.conf;$H/core1.conf$3" > ../build-sram-core1-$1.log 2>&1; R=$?
  echo "== build-sram-core1-$1: exit $R"
  grep -E "^Memory region|^ +[A-Za-z_0-9]+: +[0-9]|overflowed|devicetree error|error:|Error" ../build-sram-core1-$1.log | head -12 | cut -c1-240
  [ $R -eq 0 ] && ${TC:-$HOME/zephyr-toolchain}/aarch64-zephyr-elf/bin/aarch64-zephyr-elf-size ../build-sram-core1-$1/zephyr/zephyr.elf
}
if [ "${ONLY:-}" != el1 ]; then   # ONLY=el1 builds just image D
build uart1 core1-uart1.overlay ""
build ring "core1-ring.overlay;$H/core1-ring-map.overlay" ";$H/core1-ring.conf"
# Image C: the same ring console, but code in WKUP SRAM and RAM from 0x70801000, so the SCMI mailbox at 0x70800000 is never written.
# 4 translation tables is too few (Renode: data abort mapping the GIC at 0x01840008), so it is 6, which Renode accepts.
printf 'CONFIG_MAX_XLAT_TABLES=6\n' > $H/xlat.conf
build xip "core1-xip.overlay;$H/core1-ring-map.overlay" ";$H/core1-ring.conf;$H/core1-xip.conf;$H/xlat.conf"
fi
# Image D (build-sram-core1-el1): image C's map, started by Linux through PSCI CPU_ON under TF-A. Non-secure, no EL3 code (core1-ns.conf instead of bare-el3.conf).
west build -p always -b am62l_evm/am62l3/a53 "$APP" -d ../build-sram-core1-el1 -- -DEXTRA_DTC_OVERLAY_FILE="$H/sram-all.overlay;$H/core1-xip.overlay;$H/core1-ring-map.overlay" -DEXTRA_CONF_FILE="$H/core1-ns.conf;$H/core1.conf;$H/core1-ring.conf;$H/core1-xip.conf" > ../build-sram-core1-el1.log 2>&1; R=$?
echo "== build-sram-core1-el1: exit $R"; grep -E "^Memory region|^ +[A-Za-z_0-9]+: +[0-9]|overflowed|devicetree error|error:|Error" ../build-sram-core1-el1.log | head -12 | cut -c1-240
[ $R -eq 0 ] && T=${TC:-$HOME/zephyr-toolchain}/aarch64-zephyr-elf/bin/aarch64-zephyr-elf- && ${T}size ../build-sram-core1-el1/zephyr/zephyr.elf && ${T}readelf -h ../build-sram-core1-el1/zephyr/zephyr.elf | grep Entry && echo "psci/smccc/scmi symbols:" && ${T}nm ../build-sram-core1-el1/zephyr/zephyr.elf | grep -i -E "psci|smccc|scmi" | head; ls -l ../build-sram-core1-el1/zephyr/zephyr.bin
# Image E (build-ddr-core1): Zephyr in DDR 0xE0000000 to 0xEFFFFFFF (Linux runs mem=1536M), ring at 0xEFFFF000, started by PSCI CPU_ON on core 1. No SRAM, no XIP.
west build -p always -b am62l_evm/am62l3/a53 "$APP" -d ../build-ddr-core1 -- -DEXTRA_DTC_OVERLAY_FILE="$H/core1-ddr.overlay" -DEXTRA_CONF_FILE="$H/core1-ns.conf;$H/core1.conf;$H/core1-ring.conf;$H/core1-ddr.conf" > ../build-ddr-core1.log 2>&1; R=$?
echo "== build-ddr-core1: exit $R"; grep -E "^Memory region|^ +[A-Za-z_0-9]+: +[0-9]|overflowed|devicetree error|error:|Error" ../build-ddr-core1.log | head -12 | cut -c1-240
[ $R -eq 0 ] && T=${TC:-$HOME/zephyr-toolchain}/aarch64-zephyr-elf/bin/aarch64-zephyr-elf- && ${T}size ../build-ddr-core1/zephyr/zephyr.elf && ${T}readelf -h ../build-ddr-core1/zephyr/zephyr.elf | grep Entry
