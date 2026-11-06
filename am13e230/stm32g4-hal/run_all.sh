#!/bin/bash
# Build the five LaunchPad examples TI ships for its STM32G4 HAL compatibility layer, and the two ST examples ported
# with the edits in st-*/main.diff, against the layer. Build only; the proofs are in ../../docs/experiments/2026-10-09-am13e230-stm32g4-hal-layer.md.
#   ./run_all.sh            ST originals are read from ~/vendor/STM32CubeU0 (git clone of STM32CubeU0, tag v1.3.0)
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; cd "$HERE"
E=/Applications/ti/am13e230x_sdk_26_01_00_03/examples/compatibility/stm32g4_hal; L=am13e230x_lp/m33_nortos
ST=${ST:-$HOME/vendor/STM32CubeU0/Projects/NUCLEO-U083RC/Examples}
./build.sh toggle $E/GPIO_IOToggle/$L/example.syscfg $E/GPIO_IOToggle/main.c
./build.sh dma  $E/DMA_FLASHToRAM/$L/example.syscfg $E/DMA_FLASHToRAM/main.c $E/DMA_FLASHToRAM/BSP/stm32g4xx_nucleo.c -I$E/DMA_FLASHToRAM -I$E/DMA_FLASHToRAM/BSP
./build.sh spim $E/SPI/SPI_Poll_Master/$L/example.syscfg $E/SPI/SPI_Poll_Master/main.c $E/SPI/SPI_Poll_Master/stm32g4xx_hal_msp.c $E/SPI/SPI_Poll_Master/BSP/stm32g4xx_nucleo.c -I$E/SPI/SPI_Poll_Master -I$E/SPI/SPI_Poll_Master/BSP
./build.sh spis $E/SPI/SPI_Poll_Slave/$L/example.syscfg $E/SPI/SPI_Poll_Slave/main.c $E/SPI/SPI_Poll_Slave/stm32g4xx_hal_msp.c $E/SPI/SPI_Poll_Slave/BSP/stm32g4xx_nucleo.c -I$E/SPI/SPI_Poll_Slave -I$E/SPI/SPI_Poll_Slave/BSP
./build.sh uart $E/UART/UART_Printf/$L/example.syscfg $E/UART/UART_Printf/main.c $E/UART/UART_Printf/stm32g4xx_hal_msp.c -I$E/UART/UART_Printf
# ST's own examples: ST's main.c is kept; main.h, the MSP file and the retarget file are TI-side and in this folder.
W=$(mktemp -d "${TMPDIR:-/tmp}/frl-st-port.XXXXXX")   # private work dir, not a fixed path another user could pre-create
cp $ST/GPIO/GPIO_IOToggle/Src/main.c "$W/gpio_main.c" && patch -s "$W/gpio_main.c" < st-gpio-iotoggle/main.diff && cp "$W/gpio_main.c" st-gpio-iotoggle/main.c
cp $ST/UART/UART_Printf/Src/main.c "$W/uart_main.c" && patch -s "$W/uart_main.c" < st-uart-printf/main.diff && cp "$W/uart_main.c" st-uart-printf/main.c
./build.sh stgpio $E/GPIO_IOToggle/$L/example.syscfg st-gpio-iotoggle/main.c -Ist-gpio-iotoggle
./build.sh stuart $E/UART/UART_Printf/$L/example.syscfg st-uart-printf/main.c st-uart-printf/stm32g4xx_hal_msp.c st-uart-printf/retarget.c -Ist-uart-printf
# ST's timer examples: compile only, to count what the layer lacks (it has no timer).
./tim_try.sh
