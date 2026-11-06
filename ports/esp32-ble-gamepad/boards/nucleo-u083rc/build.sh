#!/bin/bash
# usage: ./build.sh [extra cflags]   -> fw.elf fw.map
set -e
cd "$(dirname "$0")"
CC=~/zephyrproject/modules/hal; TC=~/zephyr-toolchain/arm-zephyr-eabi/bin/arm-zephyr-eabi
$TC-gcc -mcpu=cortex-m0plus -mthumb -Os -ffunction-sections -fdata-sections -fno-tree-loop-distribute-patterns -Wall "$@" \
  -DSTM32U083xx -I. -I$CC/stm32/stm32cube/stm32u0xx/soc -I$CC/cmsis/CMSIS/Core/Include \
  -nostartfiles -nostdlib -Wl,--gc-sections -Wl,-Map=fw.map -T u083rc.ld main.c gamepad_core.c board_u083rc.c -lgcc -o fw.elf
$TC-size fw.elf
