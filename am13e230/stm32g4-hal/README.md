# STM32G4 HAL code on the TI LP-AM13E230: build and run through TI's compatibility layer

Short answer: `./run_all.sh` builds TI's five STM32G4 HAL examples and two of ST's own examples against TI's compatibility layer; `./build.sh` builds any one program. What ran, what did not, and where the layer stops: [the experiment page](../../docs/experiments/2026-10-09-am13e230-stm32g4-hal-layer.md).

Board: [LP-AM13E230 README](../README.md), [FAQ](../FAQ.md). Checked 2026-10-09 on macOS 15.7 with the AM13E2x SDK 26.01.00.03, TI Arm Clang 5.1.1.LTS and SysConfig 1.28.0.

| File | What it is |
|---|---|
| `build.sh <name> <syscfg> <sources> [-I...] [-D...] [--flash]` | Builds one program with the layer's prebuilt library; same flags as `../tools/build_example.sh` |
| `run_all.sh` | The five TI examples, the two ST ports, then the timer compile test |
| `st-gpio-iotoggle/`, `st-uart-printf/` | The edits to ST's `main.c` (`main.diff`) and the TI-side files ST's example needs (`main.h`, `stm32g4xx_hal_msp.c`, `retarget.c`) |
| `st-tim-pwm/main.h`, `tim_try.sh` | Stand-in header and script that count the compiler errors of ST's two timer examples |

ST's `main.c` is not stored here. `run_all.sh` takes it from a clone of STM32CubeU0 v1.3.0 (set `ST` to the clone's `Projects/NUCLEO-U083RC/Examples` folder) and applies `main.diff`. ST's code is BSD 3-clause licensed; the license text is in the clone.

Proofs used on the board: the backchannel serial port (`../tools/uart_listen.py`, `PARITY=odd` for ST's UART example), a register read of the output pin (`../tools/pin_check.sh`) and a RAM read (`../tools/run_image.py`).

If you see `fatal error: 'Uart_Hal.h' file not found`, add `-I$SDK/source/hal/am13e230x/include` and `-I$SDK/ti_sdk_config/am13e230x/default/Hal_Cfg`; `build.sh` already does. **Tool or docs.**

If ST's `HAL_Delay(100)` returns at once, it counts CPU cycles here. Multiply by `CPUCLK_FREQ_HZ / 1000`. **Tool or docs.**

What is open: the two SPI examples against each other, and an unmodified STM32CubeG4 example.
