#!/bin/bash
# Build the five images and write results/build-log.txt (sizes). Needs arm-none-eabi-gcc on PATH
# (or set GCC_BIN), the MSPM0 SDK (MSPM0_SDK, default /Applications/ti/mspm0_sdk_2_11_00_07) and Python 3.
# Run from anywhere:  ports/micropython/build_all.sh
set -e
export PYTHONDONTWRITEBYTECODE=1
cd "$(dirname "$0")"
[ -n "$GCC_BIN" ] && export PATH="$GCC_BIN:$PATH"
mkdir -p results
LOG=results/build-log.txt
{
  echo "# MicroPython for MSPM0, build sizes $(date +%Y-%m-%d)"
  arm-none-eabi-gcc --version | head -1
  echo "upstream: micropython v1.29.0 (0fd6c573e), MSPM0 SDK ${MSPM0_SDK:-/Applications/ti/mspm0_sdk_2_11_00_07}"
  echo "columns: image, text (code and constants in flash), data (initialised, also in flash), bss, flash=text+data, static RAM=data+bss, stack, heap, RAM total"
} > $LOG
row() { # row <name> <board> <variant> <flash_len> <ram_len> <make args...>
  local name=$1 board=$2 variant=$3 flash=$4 ram=$5; shift 5
  rm -rf build-$name
  make BOARD=$board VARIANT=$variant BUILD=build-$name "$@" -j8 > results/make-$name.log 2>&1 || { echo "$name: BUILD FAILED, see results/make-$name.log" >> $LOG; return 0; }
  read t d b _ < <(arm-none-eabi-size build-$name/firmware.elf | tail -1)
  hs=$(arm-none-eabi-nm build-$name/firmware.elf | awk '$3=="__heap_start__"{print $1}')
  sf=$(arm-none-eabi-nm build-$name/firmware.elf | awk '$3=="__stack_floor__"{print $1}')
  st=$(arm-none-eabi-nm build-$name/firmware.elf | awk '$3=="__StackTop"{print $1}')
  stack=$((16#$st - 16#$sf)); heap=$((16#$sf - 16#$hs))
  for a in "$@"; do case $a in HEAP_MAX=*) heap=${a#HEAP_MAX=};; esac; done
  echo "$name flash=$((t+d)) text=$t data=$d bss=$b static_ram=$((d+b)) stack=$stack heap=$heap ram_total=$((d+b+stack+heap)) flash_limit=$flash ram_limit=$ram" >> $LOG
}
row g3519-full g3519 full 524288 131072
row g3519-min  g3519 min  524288 131072
row g3519-min-heap16k g3519 min 524288 131072 HEAP_MAX=16384
row g3507-min  g3507 min  131072 32768
# The full build does not fit the G3507; link it into a 256 kB window just to measure the overshoot.
row g3507-full-oversize g3507 full 131072 32768 FLASH_LEN=0x40000
cat $LOG
