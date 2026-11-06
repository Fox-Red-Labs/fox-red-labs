#!/bin/bash
# Builds upstream Zephyr main for the LP-AM13E230 on an Intel Mac, no board needed.
#   ./build.sh            sets up the workspace once (about 90 s, 830 MB), then builds hello_world, blinky and led-proof
#   ./build.sh flash      also needs the board: flashes led-proof with OpenOCD built from source (see README.md)
# Everything lands in $RUN (default out/zephyr-am13), never in the repo.
# Toolchain: the GCC 12.2 from Zephyr SDK 0.17.4 that the other Zephyr pages here use, selected with ZEPHYR_TOOLCHAIN_VARIANT=cross-compile.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
RUN="${RUN:-$PWD/out/zephyr-am13}"
mkdir -p "$RUN"; cd "$RUN" || exit 1
source "$HOME/zephyrproject/.venv/bin/activate"
export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE="$HOME/zephyr-toolchain/arm-zephyr-eabi/bin/arm-zephyr-eabi-"
export ZEPHYR_BASE="$RUN/zephyr-main"
if [ ! -d zephyr-main/.git ]; then
  git clone --depth 1 --filter=blob:none https://github.com/zephyrproject-rtos/zephyr zephyr-main || exit 1
  west init -l zephyr-main && west update hal_ti cmsis cmsis_6 --narrow -o=--depth=1 || exit 1
fi
[ -f zephyr-main/boards/ti/lp_am13e230/board.yml ] || { echo "This Zephyr checkout has no lp_am13e230 board"; exit 1; }
t() { local log=$1; shift; local s=$(date +%s); "$@" > "$log" 2>&1; local rc=$?; echo "  -> rc=$rc, $(( $(date +%s) - s )) s"; return $rc; }
echo "hello_world"; t hello.log west build -p always -b lp_am13e230 zephyr-main/samples/hello_world -d build-hello
echo "led-proof";   t led.log west build -p always -b lp_am13e230 "$HERE/led-proof" -d build-led
echo "blinky (expected to fail: the board has no led0 alias and no GPIO driver)"
t blinky.log west build -p always -b lp_am13e230 zephyr-main/samples/basic/blinky -d build-blinky
for b in hello led; do echo "== $b"; grep -E "FLASH:|  RAM:" $b.log; done
if [ "${1:-}" = flash ]; then
  OCD="$RUN/openocd"
  [ -x "$OCD/src/openocd" ] || { echo "No OpenOCD at $OCD/src/openocd; see README.md"; exit 1; }
  west flash -d build-led --runner openocd --openocd "$OCD/src/openocd" --openocd-search "$OCD/tcl" \
    --cmd-pre-init "adapter serial ${SERIAL:?set SERIAL to the probe serial}"
fi
