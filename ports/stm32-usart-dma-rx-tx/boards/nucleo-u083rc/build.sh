#!/bin/bash
# Build the NUCLEO-U083RC port of MaJerle/stm32-usart-uart-dma-rx-tx (ringbuff_tx example) with STM32CubeCLT's GCC.
#   ./build.sh [outdir]      default out/port-it-prove-it/stm32/build/nucleo-u083rc
# Needs STM32CubeU0 in $VENDOR/STM32CubeU0 (see nucleo-u083rc/prebuild.sh --fetch-sources).
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd); TOP=$HERE/../..
OUT=${1:-$PWD/out/port-it-prove-it/stm32/build/nucleo-u083rc}; mkdir -p "$OUT"
CUBE=${VENDOR:-$HOME/vendor}/STM32CubeU0
GCC=$(ls -d /opt/ST/STM32CubeCLT*/GNU-tools-for-STM32/bin | tail -1)/arm-none-eabi
D=$CUBE/Drivers/STM32U0xx_HAL_Driver
CF="${EXTRA:-} -mthumb -mcpu=cortex-m0plus -Os -g -ffunction-sections -fdata-sections -Wall -DUSE_FULL_LL_DRIVER -DSTM32U083xx -I$HERE/inc -I$D/Inc -I$CUBE/Drivers/CMSIS/Device/ST/STM32U0xx/Include -I$CUBE/Drivers/CMSIS/Include -I$TOP/common/lwrb/src/include"
SRC="$HERE/src/main.c $HERE/src/stm32u0xx_it.c $HERE/src/system_stm32u0xx.c $HERE/src/syscalls.c $HERE/src/sysmem.c $TOP/common/lwrb/src/lwrb/lwrb.c"
for m in dma gpio rcc usart utils pwr exti; do SRC="$SRC $D/Src/stm32u0xx_ll_$m.c"; done
OBJ=""
for f in $SRC $HERE/startup/startup_stm32u083rctx.s; do o=$OUT/$(basename "$f").o; "$GCC-gcc" $CF -c "$f" -o "$o"; OBJ="$OBJ $o"; done
"$GCC-gcc" -mthumb -mcpu=cortex-m0plus -T"$HERE/linker/STM32U083RCTX_FLASH.ld" --specs=nosys.specs -Wl,--gc-sections -Wl,-Map=$OUT/app.map $OBJ -o "$OUT/app.elf"
"$GCC-size" "$OUT/app.elf"; "$GCC-objcopy" -O binary "$OUT/app.elf" "$OUT/app.bin"
