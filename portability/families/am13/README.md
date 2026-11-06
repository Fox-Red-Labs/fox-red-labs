# Porting MSPM0, STM32 and C2000 code to the AM13 family (LP-AM13E230)

MSPM0 DriverLib programs run on the AM13E230 with their app code unchanged, because the AM13 DriverLib carries about 530 redirect macros for MSPM0 names; the SysConfig file is translated module by module and the names the AM13 generator does not emit go in a generated alias header (matrix ids X8, X31). 20 of 43 MSPM0 examples build that way and all 14 run on the board ran with clean fault registers (X31). C2000-style motor control code moved as plain C: one current-loop step goes from trigger to new PWM in 1.33 µs here against 2.58 µs on the F2800137 (X49).

Terms: [docs/vocab.md](../../../docs/vocab.md) and this family's [FAQ](FAQ.md). **DriverLib** is TI's register-level C library; **SysConfig** writes pin and clock code from a `.syscfg` file; **MCPWM** is the AM13's motor PWM unit; **CMPSS** is its comparator subsystem; **PGA** is a programmable-gain amplifier; **TMU** is the trigonometric math unit, reached as custom instructions; **NPU** is the neural processing unit; **FOC** is field-oriented control; **HAL** is a hardware abstraction layer. 1 kB = 1,024 bytes. The generated record: [EVIDENCE.md](EVIDENCE.md). The assistant's copy: [AGENTS.md](AGENTS.md).

Contents: the part and its neighbors, a ten-minute path, the one-command build, the driver and function map, how to port into the family, moving inside it, moving to and from other families, what was measured, errors, sources.

## The board (one part held)

| Part | Kit | Core and clock | Flash | RAM | On the board | Source |
|---|---|---|---|---|---|---|
| AM13E23019 | LP-AM13E230, revision E2 (`MCU178E2` on the back; E1 has different pins) | Cortex-M33, 200 MHz, FPU, TMU, TinyEngine NPU | 512 kB | 128 kB | isolated debug probe, CAN-FD transceiver, 5 V encoder header, red and green LEDs, 40-pin BoosterPack headers, no motor driver | `am13e230/AGENTS.md`, `am13e230/README.md` |

Board folder: [`am13e230/`](../../../am13e230/AGENTS.md) (`README.md`, `AGENTS.md`, `FAQ.md`). No other AM13 part is used here. The silicon on the kit is prototype XAM13E23019 (`am13e230/README.md`).

### Part-to-part map: the AM13E230 against its nearest neighbors

| | AM13E230 | MSPM0G3519 | MSPM33C321A | F2800137 |
|---|---|---|---|---|
| Core, clock | Cortex-M33, 200 MHz, FPU, TMU, NPU | Cortex-M0+, 80 MHz | Cortex-M33, 160 MHz, FPU | C28x, 120 MHz |
| Flash, RAM | 512 kB, 128 kB | 512 kB, 128 kB | 1 MB, 256 kB | 256 kB, 36 kB |
| Motor and analog blocks | five motor PWM modules (MCPWM), three 12-bit ADCs at 7.81 million samples a second, PGAs, fast comparators (CMPSS) | two 4 Msps ADCs, 12-bit DAC | HSADC | - |
| ADC model | sequencer and SOC: force the sequencer, wait for `DL_ADC_getSOCStatus`, then `DL_ADC_isBusy` | ADC12 | HSADC | - |
| PWM in SysConfig | no timer-PWM module: copy the MSPM0 generated init and drop `isTimerWithFourCC` | timer PWM module | timer PWM module | - |
| SysConfig module ids | `/drivers/gpio`, `uart`, `systick`, `timer` | `/ti/driverlib/GPIO`, `UART`, `SYSTICK`, `TIMER` | `/ti/driverlib/...` | - |
| Serial block names | `UC0`, `UC1`, `UC4` (`UC4_INT_IRQn` for `UART_0_INST`) | UART0 | UART | - |
| Generator slip | writes the bare word `RX` into C | - | - | - |
| Boot settings | NONMAIN at 0x60100000, 8 kB, never written | NONMAIN | boot configuration at 0x80101800 | - |
| Start-up of a new image | DSLite with `-r 1 -u -t 60` | nRST pulse after DSLite | DSLite with `-r 1 -u -t 60` | - |
| Wake from STANDBY | the SDK names a GPIO as the wake source; a watchdog-only wake never woke | - | - | - |
| Math unit | TMU must be on before use, or a usage fault (CFSR 0x80000) | math accelerator | none | TMU |

Sources: `am13e230/AGENTS.md`, `am13e230/README.md`, matrix ids X8, X31, X38, X49; `portability/everywhere/PORTING.md` sections 2 and 3; `portability/three-ways/README.md` (ADC model, DSLite flags); `mspm0g3519/AGENTS.md`; `mspm33c321a/AGENTS.md`; `data/boards.csv` (F2800137).

## Ten-minute path

Port one MSPM0 example to the LP-AM13E230 and run it. The commands are the ones the runs used.

1. Terminal, no board. Translate and build one example; the porter writes `board_pins.h` with the interrupt names and the `RX` alias:

   ```bash
   python3 portability/everywhere/crossport_mspm0_am13.py <work dir> <results.jsonl> --only gpio_toggle_output
   ```

   You should see one result line with the dropped SysConfig lines and the build outcome.
2. Terminal, board plugged in. Back up main flash (0x00000000, 0x20000 words) and read NONMAIN (0x60100000, 8 kB), each twice, with the `DSLite memory` form in `am13e230/AGENTS.md`.
3. Terminal. `python3 tools/preflight_image.py am13e230 <image.out>`.
4. Terminal. Flash and start: `DSLite flash -c <ccxml> -e -f -v -r 1 -u -t 60 <image>.out`. `Failed: Target is not halted` after `Program verification successful` is expected (`am13e230/AGENTS.md`).
5. Terminal. `am13e230/tools/led_check.sh <serial>` reads PA5 six times; changing values mean a blink. With `--run`, the porter flashes each built image itself, waits 4 s, reads CFSR, HFSR and the GPIO outputs, captures the backchannel UART, skips risky, low-power and flash-writing examples, and writes TI's 250 ms blink back at the end; it does not restore your backup, so restore it yourself.

## One-command build

```bash
portability/three-ways/driverlib/build_am13.sh
```

It builds the shared 81-line application with the AM13 DriverLib (`glue_am13.c`, 38 lines; `am13e230.syscfg`, 35 lines): `9,376` B of flash and `9,205` B of RAM, most of the RAM the SDK linker file's stack and heap; 14.3 s on this Mac. It does not flash (`portability/three-ways/README.md`).

## Driver and function map

Columns: the AM13 SDK's own DriverLib, MSPM0 DriverLib code carried through the redirects, TI's STM32G4 HAL layer in the AM13 SDK, plain C from C2000, and the other layers. Not run cells are quiet text.

| Function | AM13 DriverLib (SDK examples) | MSPM0 DriverLib code carried over | STM32G4 HAL layer | Plain C from C2000 | TI Drivers | Zephyr (main) |
|---|---|---|---|---|---|---|
| GPIO | Works: blink on PA5, read by `led_check.sh` ([board](../../../am13e230/AGENTS.md)) | Works: the GPIO program with 53 app lines unchanged (X8); LED-rotating examples toggled PA5 to PA8 (X31) | Works: ST's GPIO example with 3 lines added to ST's `main.c` (X9) | _not used_ | Not for this chip: none in the SDK ([`portability/README.md`](../../README.md)) | Could work: no GPIO driver; `am13e230/zephyr/led-proof` writes the registers |
| UART | Works: clean to 6.25 Mbaud, UC0 to UC1 (`data/results/am13e230.csv`) | Works: the UART print program unchanged (X8) | Works: ST's UART example with 7 lines added to ST's `main.c`; the two TI-side shims are 13 and 50 lines (X9) | _not used_ | Not for this chip | Could work: no serial driver |
| I2C | - | - | _not in the layer_ (X9) | _not used_ | Not for this chip | - |
| SPI | - | - | Could work: TI's SPI layer example runs to its wait and stops there (X9) | _not used_ | Not for this chip | - |
| ADC | Works: shared app through the sequencer (X53); microphone capture, 2k samples at 8 kHz | - | _not in the layer_ (X9) | Works: PWM-triggered conversion in the current-loop step (X49) | Not for this chip | - |
| PWM, timers and motor PWM | Works: TI's Hall FOC drives a motor closed loop through a DRV8376EVM, unloaded ([FOC](../../../am13e230/foc-drv8376/README.md)) | Works: timer PWM read 74.9 and 49.9 percent against 75 and 50, with the MSPM0 generated init copied (X8) | _not in the layer_: ST's timer examples fail at the first compile, 90 and 87 errors (X9) | Works: trigger to new PWM value 1.33 µs (X49) | Not for this chip | - |
| DMA | Works: STM32 UART DMA port passes five of six checks ([STM32 port](../../../docs/experiments/2026-10-19-stm32-usart-dma-port-matrix.md)) | - | - | _not used_ | Not for this chip | - |
| Interrupts | Works: interrupt entry 0.365 µs (X49) | Works through the alias header (`GPIO0_INT_IRQn`, `UC4_INT_IRQn`, `TIMGn_0_INT_IRQn`) (X31) | - | Works: the PWM-triggered ADC interrupt (X49) | Not for this chip | - |
| Flash | Should work: flash write and persistent-counter examples built, not run ([chip map](../../../docs/guides/chip-capability-map.md)) | - | - | _not used_ | Not for this chip | - |
| Clocks | Works: 200 MHz; the UART clock at 100 MHz in the STM32 port | Works with the module-by-module SysConfig translation (X31) | Could work: no RCC; `HAL_Delay` and every timeout count CPU cycles, not milliseconds (X9) | _not used_ | Not for this chip | - |
| Low power | Works: SLEEP with a watchdog interval interrupt, 5 ticks at 1.0 s; STANDBY0 with only a watchdog wake never woke ([chip map](../../../docs/guides/chip-capability-map.md)) | - | _not in the layer_ (no PWR) | _not used_ | Not for this chip | - |
| Math and AI units | Works: TMU, one FOC step 117 cycles; NPU 4.7 to 8.8 times faster than TI's CPU build on the occupancy models | _not used_ | _not used_ | Works: sine and cosine on the TMU (X49) | Not for this chip | - |
| RTOS | Works: FreeRTOS, task switch 1.49 µs at 200 MHz (`data/support-matrix.json`) | _bare metal_ | _bare metal_ | _bare metal_ | Not for this chip | Works: `hello_world` reaches main ([Zephyr](../../../docs/experiments/2026-10-09-am13e230-zephyr-upstream.md)) |

Sources: matrix ids X8, X9, X31, X49, X53; `am13e230/AGENTS.md`; `docs/guides/chip-capability-map.md`; `site/data/ports.json` (ports `stm32-usart-dma-rx-tx`, `ti-hall-foc-drv8376`, `zephyr-am13e230`); `data/support-matrix.json`.

## How to port into this family

1. **Pick the route by the source code.** MSPM0 DriverLib code: the AM13 DriverLib redirects (X8, X31). STM32G4 HAL code: TI's layer covers GPIO and UART, with 65 of the 93 functions its headers declare and no RCC, PWR, timer, ADC or I2C (X9). C2000 or plain C control code: move it as C and put the trigonometry on the TMU (X49). TI Drivers code: there is none in the AM13 SDK, so move it to DriverLib.
2. **Translate the `.syscfg` module by module.** MSPM0 `/ti/driverlib/GPIO` is `/drivers/gpio` on the AM13; the table is in the porter. Each line SysConfig rejects is commented out with `// PORT-DROPPED:` and listed with SysConfig's message (`portability/everywhere/PORTING.md` section 2).
3. **Force-include the alias header.** `crossport_mspm0_am13.py` writes `board_pins.h` per example: the group `_PORT` macro, `_INT_IRQN` and `_IRQHandler` names, and `#define RX DL_UART_INTERRUPT_RX` for the generator slip. Renames and small inline helpers only, no logic (`portability/everywhere/PORTING.md` section 3).
4. **Redo pins per program.** SysConfig and pin lines were redone for each MSPM0 program: 19, 32 and 7 lines (X8). Check the board revision: E1 routes MCPWM4 1B, 2A and 3B differently from E2 (`am13e230/AGENTS.md`).
5. **Turn the TMU on before any code that uses it.** Define `_system_pre_init` to set `SCB->CPACR`; a line at the top of `main()` is too late (`am13e230/AGENTS.md`).
6. **Keep a GPIO wake source and an awake window in any sleep image.** A STANDBY0 image with only the watchdog as wake source locked the debugger out (`am13e230/AGENTS.md`, trap 9).
7. **Back up, run, restore.** `tools/board_sweep/run_all.py <BOARD> <builds.jsonl> <out.jsonl>` backs up, runs each image, reads CFSR, HFSR and the GPIO outputs, and restores; it refuses key, flash-protection and NONMAIN examples unless asked by name (`portability/everywhere/PORTING.md` section 5).

## Moving inside the family

Only one AM13 part is held, so no move between AM13 parts has run. The revision matters inside one part: E1 and E2 boards route three MCPWM4 outputs differently (`am13e230/AGENTS.md`).

## Moving to and from the other families

| To or from | Route | Measured | Matrix ids and files |
|---|---|---|---|
| MSPM0 to AM13, three programs | 13-line shim header; SysConfig redone per program | 3 of 3 ran with 53 app lines unchanged | X8, [experiment](../../../docs/experiments/2026-10-09-am13e230-mspm0-driverlib.md) |
| MSPM0G3519 to AM13, configuration port | `portability/everywhere/crossport_mspm0_am13.py` | 43 of 43 SysConfig files translate, 20 build, 14 ran clean | X31 |
| STM32G4 HAL to AM13 | TI's layer in the AM13 SDK | GPIO and UART ran; timers do not compile | X9, [experiment](../../../docs/experiments/2026-10-09-am13e230-stm32g4-hal-layer.md) |
| F2800137 to AM13, control loop | Plain C, TMU for sine and cosine | 1.33 µs trigger to PWM; one FOC step 117 cycles | X49 |
| MSP430FR2433 to AM13 | Examples rewritten | 3 of 3 build; the deep-sleep test never woke | X38, `ports/msp430-to-mspm0/README.md` |
| AM13 to SK-TDA4VM, radar parser | Plain C | 231 frames identical to the Mac reference | X34 |
| One app across families | Shared `app.c`, AM13 DriverLib glue | 38 glue lines, 35 config lines; serial check passed | X53 |
| MSPM0 guide | | | [MSPM0 family guide](../mspm0/README.md) |
| MSPM33 guide | | | [MSPM33 family guide](../mspm33/README.md) |

## What was measured

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| MSPM0 examples translated and built | 43 translate, 20 build | AM13E2x SDK 26.01.00.03, SysConfig 1.28.0 | measured | `portability/matrix.csv` X31 |
| Those run on the LP-AM13E230 | 14 of 14 clean fault registers | the other 6 skipped (flash, standby, sleep) | measured | X31 |
| MSPM0 timer PWM through the redirects | 74.9 and 49.9 percent against 75 and 50 | app code unchanged | measured | X8 |
| Current-loop step, trigger to new PWM | 1.33 µs at 200 MHz, against 2.58 µs on the F2800137 | mean of 1,000 passes | measured | X49 |
| AM13 SDK examples | 100 of 100 build; 80 flashed and run, 79 clean | TI Arm Clang 5.1.1 | measured | `am13e230/AGENTS.md` |
| UART loop | clean to 6.25 Mbaud | UC0 to UC1 | measured | `am13e230/README.md`, `data/results/am13e230.csv` |

## If you see this

- If you see `error: use of undeclared identifier 'RX'`, the AM13 SysConfig generator wrote the bare word into C: add `#define RX DL_UART_INTERRUPT_RX` to the force-included alias header (`portability/everywhere/PORTING.md` section 3).
- If you see a usage fault with `CFSR 0x80000`, the code used the TMU while it was off: turn it on in `_system_pre_init` (`am13e230/AGENTS.md`).
- If you see `'__device_dts_ord_DT_N_ALIAS_led0_P_gpios_IDX_0_PH_ORD' undeclared here` from `west build -b lp_am13e230 samples/basic/blinky`, Zephyr's board has no LED, GPIO or serial driver: use `am13e230/zephyr/led-proof` or TI's SDK example (`am13e230/AGENTS.md`).

The rest, with causes: [FAQ.md](FAQ.md).

## Where these facts come from

AM13E2x SDK 26.01.00.03.STS, MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0, DSLite from CCStudio 21.0.1, probe firmware 3.0.0.41, on a revision E2 board and an Intel Mac with macOS 15.7.9; runs dated 2026-10-08 to 2026-10-25 in the files named. Data: `portability/matrix.csv`, `data/results/am13e230.csv`, `site/data/ports.json`, `data/support-matrix.json`. The generated record: `portability/families/am13/EVIDENCE.md`. Checked 2026-10-25.
