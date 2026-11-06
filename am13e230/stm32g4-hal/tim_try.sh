#!/bin/bash
# Compile ST's HAL timer example (TIM_DMA, PWM through HAL_TIM_PWM_Start_DMA) and ST's LL PWM example against the layer's headers,
# with a stand-in main.h, and count the compiler errors and distinct missing names.
SDK=/Applications/ti/am13e230x_sdk_26_01_00_03; HERE="$(cd "$(dirname "$0")" && pwd)"
ST=${ST:-$HOME/vendor/STM32CubeU0/Projects/NUCLEO-U083RC}
for f in Examples/TIM/TIM_DMA/Src/main.c Examples_LL/TIM/TIM_PWMOutput_Init/Src/main.c; do
  out=$(/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin/tiarmclang -c "$ST/$f" -o /dev/null -mcpu=cortex-m33 -mthumb -I"$HERE/st-tim-pwm" \
    -I$SDK/source/compiler/m33_ti_arm_clang -I$SDK/source/device/am13e230x/include -I$SDK/source/device/am13e230x/include/hw -I$SDK/source/arch/include \
    -I$SDK/source/arch/m33/include -I$SDK/source/cmsis/Core/Include -I$SDK/source/driverlib/am13e230x -I$SDK/source/compatibility/stm32g4_hal/STM32G4xx_HAL_Driver/Inc \
    -I$SDK/source/hal/am13e230x/include -I$SDK/ti_sdk_config/am13e230x/default/Hal_Cfg -I$SDK/ti_sdk_config/am13e230x/default/device_support/include \
    -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg -I$SDK/ti_sdk_config/am13e230x/default/arch_cfg/m33 -I$SDK/source/utils/log/include \
    -I$SDK/ti_sdk_config/am13e230x/default/utils_cfg -I$SDK/source -ferror-limit=0 2>&1 | grep "error")
  echo "$f: $(echo "$out" | wc -l | tr -d ' ') errors, $(echo "$out" | grep -oE "(undeclared identifier|unknown type name) '[A-Za-z0-9_]+'" | sort -u | wc -l | tr -d ' ') distinct missing names"
done
