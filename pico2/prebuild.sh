#!/bin/bash
# Build everything for the Raspberry Pi Pico 2 that needs no board: blink, CoreMark (unmodified EEMBC files,
# third-party/coremark/) on both cores, the four occupancy models, math and Dhrystone (bench/), the pico-examples
# build pass for the sweep, and Zephyr's blinky. Output: out/pico2/
#
#   ./prebuild.sh          refuse to start if the Mac is busy (1-minute load at or above the core count)
#   WAIT=1 ./prebuild.sh   wait for a quiet Mac instead
#
# Installs nothing. When a tool is missing it prints the install steps, with the versions and sizes read on
# the vendors' pages on 2026-10-07, and exits 2. Never touches a board.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/.." && pwd)"
OUT="${OUT:-$PWD/out/pico2}"
PICO_ROOT="${PICO_ROOT:-$HOME/pico}"                       # where the install steps below put things
export PICO_SDK_PATH="${PICO_SDK_PATH:-$PICO_ROOT/pico-sdk}"
EXAMPLES="${PICO_EXAMPLES_PATH:-$PICO_ROOT/pico-examples}"
PICOTOOL_DIR="${picotool_DIR:-$PICO_ROOT/picotool}"           # folder holding picotool and picotoolConfig.cmake
RISCV_TC="${RISCV_TC:-$PICO_ROOT/riscv-toolchain}"            # riscv32-pico-elf toolchain (bin/ inside)
ARM_TC="${ARM_TC:-$HOME/zephyr-toolchain/arm-zephyr-eabi}"    # GCC 12.2.0 from Zephyr SDK 0.17.4, already here
CMAKE="${CMAKE:-$HOME/zephyrproject/.venv/bin/cmake}"
ZEPHYR="$HOME/zephyrproject"

# 1. Quiet Mac, or nothing runs
CORES=$(sysctl -n hw.ncpu)
load1() { sysctl -n vm.loadavg | awk '{print int($2)}'; }
if [ "${WAIT:-0}" = 1 ]; then
	while [ "$(load1)" -ge "$CORES" ]; do sleep 30; done
elif [ "$(load1)" -ge "$CORES" ]; then
	echo "prebuild: Mac is busy (1-minute load $(load1), $CORES cores). Not starting. WAIT=1 to wait." >&2
	exit 1
fi

# 2. What is missing
missing=""
[ -f "$PICO_SDK_PATH/pico_sdk_init.cmake" ] || missing="$missing sdk"
[ -x "$PICOTOOL_DIR/picotool" ] || missing="$missing picotool"
[ -x "$ARM_TC/bin/arm-zephyr-eabi-gcc" ] || missing="$missing armgcc"
[ -x "$CMAKE" ] || missing="$missing cmake"
command -v ninja >/dev/null || missing="$missing ninja"
if [ -n "$missing" ]; then
	cat <<EOF
prebuild: missing:$missing. Nothing built. Install steps (read 2026-10-07; recheck the versions first):

  pico-sdk 2.3.1 (released 2026-09-04), about 10 MB plus submodules (TinyUSB 53 MB):
    git clone -b 2.3.1 https://github.com/raspberrypi/pico-sdk.git $PICO_ROOT/pico-sdk
    git -C $PICO_ROOT/pico-sdk submodule update --init
  pico-examples for the sweep (tag sdk-2.3.1, about 11 MB):
    git clone -b sdk-2.3.1 https://github.com/raspberrypi/pico-examples.git $PICO_ROOT/pico-examples
  picotool 2.3.1 (released 2026-09-05), prebuilt for macOS: picotool-2.3.1-mac.zip, 1,894,547 bytes, from
    https://github.com/raspberrypi/pico-sdk-tools/releases/tag/v2.3.1-0  -> unzip into $PICO_ROOT/picotool
    (without it, pico-sdk downloads and builds picotool itself during the first build)
  RISC-V toolchain for the Hazard3 cores (triple riscv32-pico-elf): riscv-toolchain-16-mac.zip, 234,615,088 bytes,
    same release page -> unzip into $PICO_ROOT/riscv-toolchain. Optional: only the RISC-V rows need it.
  Arm compiler: none to install. This script uses $ARM_TC (GCC 12.2.0) through
    pico-sdk's PICO_GCC_TRIPLE=arm-zephyr-eabi. Arm stopped shipping Intel-Mac builds of the Arm GNU Toolchain
    after 14.2.Rel1, and the Raspberry Pi VS Code extension's list (0.22.0 data) offers 15.2.Rel1 with no
    Intel-Mac file, so the extension's default does not fit this Mac.
  CMake: $CMAKE (Zephyr venv). Ninja: /usr/local/bin/ninja.
  Zephyr rows: cd $ZEPHYR && .venv/bin/west update hal_rpi_pico   (8 MB on GitHub)

  Disk: about 0.6 GB for all of the above, unzipped. Free now: $(df -h "$HOME" | awk 'NR==2{print $4}').
EOF
	exit 2
fi

mkdir -p "$OUT"; LOG="$OUT/build.log"; : > "$LOG"
echo "prebuild pico2: $(date '+%Y-%m-%d %H:%M:%S'), load $(uptime | sed 's/.*load averages*: //')" | tee -a "$LOG"
t0=$(date +%s)
export picotool_DIR="$PICOTOOL_DIR"

# 3. Bench images, Arm Cortex-M33 (our GCC 12.2 through PICO_GCC_TRIPLE)
PICO_GCC_TRIPLE=arm-zephyr-eabi PICO_TOOLCHAIN_PATH="$ARM_TC" "$CMAKE" -S "$HERE/bench" -B "$OUT/bench-m33" -G Ninja \
	-DPICO_BOARD=pico2 -DPICO_PLATFORM=rp2350-arm-s -DCMAKE_BUILD_TYPE=Release >>"$LOG" 2>&1 \
	&& ninja -C "$OUT/bench-m33" -k 0 >>"$LOG" 2>&1
echo "bench m33: exit $?" | tee -a "$LOG"

# 4. Same images, Hazard3 RISC-V, if its toolchain is here
if [ -d "$RISCV_TC/bin" ]; then
	PICO_TOOLCHAIN_PATH="$RISCV_TC" "$CMAKE" -S "$HERE/bench" -B "$OUT/bench-riscv" -G Ninja \
		-DPICO_BOARD=pico2 -DPICO_PLATFORM=rp2350-riscv -DCMAKE_BUILD_TYPE=Release >>"$LOG" 2>&1 \
		&& ninja -C "$OUT/bench-riscv" -k 0 >>"$LOG" 2>&1
	echo "bench riscv: exit $?" | tee -a "$LOG"
else
	echo "bench riscv: skipped, no toolchain at $RISCV_TC" | tee -a "$LOG"
fi

# 5. Sweep build pass: every pico-examples target for the Pico 2, Arm then RISC-V. OTP, encrypted-boot and
#    bootloader examples are built (building is harmless) but are on the never-run list in the plan.
if [ -d "$EXAMPLES" ]; then
	PICO_GCC_TRIPLE=arm-zephyr-eabi PICO_TOOLCHAIN_PATH="$ARM_TC" "$CMAKE" -S "$EXAMPLES" -B "$OUT/examples-m33" -G Ninja \
		-DPICO_BOARD=pico2 -DPICO_PLATFORM=rp2350-arm-s -DCMAKE_BUILD_TYPE=Release >>"$LOG" 2>&1
	ninja -C "$OUT/examples-m33" -k 0 >"$OUT/examples-m33.log" 2>&1
	echo "examples m33: $(find "$OUT/examples-m33" -name '*.uf2' | wc -l | tr -d ' ') UF2 files" | tee -a "$LOG"
	if [ -d "$RISCV_TC/bin" ]; then
		PICO_TOOLCHAIN_PATH="$RISCV_TC" "$CMAKE" -S "$EXAMPLES" -B "$OUT/examples-riscv" -G Ninja \
			-DPICO_BOARD=pico2 -DPICO_PLATFORM=rp2350-riscv -DCMAKE_BUILD_TYPE=Release >>"$LOG" 2>&1
		ninja -C "$OUT/examples-riscv" -k 0 >"$OUT/examples-riscv.log" 2>&1
		echo "examples riscv: $(find "$OUT/examples-riscv" -name '*.uf2' | wc -l | tr -d ' ') UF2 files" | tee -a "$LOG"
	fi
else
	echo "examples: skipped, no pico-examples at $EXAMPLES" | tee -a "$LOG"
fi

# 6. Zephyr blinky, both cores, if the HAL module is here (do not run west update from this script)
if [ -d "$ZEPHYR/modules/hal/rpi_pico" ]; then
	for b in rpi_pico2/rp2350a/m33 rpi_pico2/rp2350a/hazard3; do
		d="$OUT/zephyr-blinky-$(basename $b)"
		(cd "$ZEPHYR" && .venv/bin/west build -p always -b "$b" zephyr/samples/basic/blinky -d "$d") >>"$LOG" 2>&1
		echo "zephyr blinky $b: exit $?" | tee -a "$LOG"
	done
else
	echo "zephyr: skipped, hal_rpi_pico not cloned (west update hal_rpi_pico)" | tee -a "$LOG"
fi

# 7. Sizes of every bench image, for blink_flash_bytes, blink_ram_bytes and the fit rows
SZ="$ARM_TC/bin/arm-zephyr-eabi-size"
{
	echo "image,core,text,data,bss"
	for e in "$OUT"/bench-m33/*.elf; do [ -f "$e" ] && "$SZ" "$e" | awk -v n="$(basename "$e" .elf)" 'NR==2{print n",m33,"$1","$2","$3}'; done
	for e in "$OUT"/bench-riscv/*.elf; do [ -f "$e" ] && "$RISCV_TC/bin/riscv32-pico-elf-size" "$e" 2>/dev/null | awk -v n="$(basename "$e" .elf)" 'NR==2{print n",riscv,"$1","$2","$3}'; done
} > "$OUT/sizes.csv"
echo "prebuild pico2: done in $(( $(date +%s) - t0 )) s; sizes in $OUT/sizes.csv; log $LOG" | tee -a "$LOG"
