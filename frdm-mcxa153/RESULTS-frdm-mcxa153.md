# NXP FRDM-MCXA153 results: Cortex-M33 against a Cortex-M0+ with the same flash and RAM

**Short answer.** At 96 MHz the MCXA153 runs CoreMark at 282 per second with GCC 12.2 at -O3 (2.94 per MHz), about 1.6 times the LP-MSPM0G3507's GCC figure of 1.82 per MHz (different flags, a cache against wait states) and 1.3 times its TI Arm Clang figure of 2.30. NXP's data sheet claims 381 (3.97 per MHz) without naming a compiler; the best measured here is 74 percent of that. The core has no floating-point unit and no AES block.

That costs motor control: one control step (field-oriented control, the math that drives a motor) in float takes 5.4k cycles. NXP's fixed-point library does the same job in 448 cycles; that is a speed result only, and its output was not compared with the float step.

| Test | Result |
|---|---|
| Changed blink, command line, tools installed | 1.4 minutes of machine time |
| Room-occupancy model (8k), -O3 | 12.6 ms, 20 of 20 correct |
| SDK examples built from a terminal | 96 of 98 |
| SDK examples that ran without a fault | 42 of 43 that can run alone |
| Hardware CRC against a table | 7.1 times |

Contents: setup, CoreMark, core timing, fastest ADC and BitNetMCU, control loop, room-occupancy models, NXP's examples, not run.

Terms: [docs/vocab.md](../docs/vocab.md) and the board's [FAQ](FAQ.md). Start-here guide: [README](README.md). 1 kB = 1,024 bytes. Times are CPU cycles of the 96 MHz clock unless they say otherwise. Words used:

- **FOC** (field-oriented control): the math a motor drive runs every PWM period to turn measured currents into three drive signals.
- **LPCAC**: the chip's small instruction cache in front of flash. **MPU**: the memory protection unit. **`nano` C library**: Arm's small `newlib-nano` C library, whose `printf` lacks some formats. **FRO**: the internal oscillator that clocks the chip.
- **SysTick** and **DWT**: the Cortex-M33's two built-in cycle counters (FreeRTOS owns SysTick, so that image uses DWT). **VCOM**: the board's USB serial port. **SRAMX**: the separate 8 kB RAM region.
- **pyOCD**: an open-source flasher that talks to the board's MCU-Link probe.
- **CMSIS-DSP**: Arm's signal-processing library. **q15**, **q31**, **f32**: 16-bit fixed, 32-bit fixed and single-precision float number formats.
- **Embench IoT**: 19 small benchmark programs. **FreeRTOS**: an open-source real-time operating system. **DMIPS**: Dhrystone's score.
- **eDMA**: the chip's DMA engine, which moves ADC results to RAM without the CPU. **BitNetMCU**: an open-source handwriting-recognition network for tiny chips; **64-wide CNN** is its largest model (a convolutional neural network).
- **-O3**, **-O2**, **-Os**: compiler settings for speed or for size.

Measured 2026-10-11 on an Intel Mac, macOS 15.7.9 (every timing below is counted in CPU cycles on the chip, so the Mac's other work does not move it). Board: FRDM-MCXA153 (MCU-Link firmware V3.128, Cortex-M33 r1p0 as read from the chip), USB power through J15, jumpers as shipped, no wires. Clock: NXP's `BOARD_BootClockFRO96M()`; code in flash with the LPCAC on. Cycles come from SysTick counting CPU cycles (the FreeRTOS image uses the DWT cycle counter because FreeRTOS owns SysTick). CoreMark ran three times per setting with identical counts to 0.0001 percent; every other test ran once (best of 5 passes inside the run, where the row says so). Every number is a row in [`data/results/frdm-mcxa153.csv`](../data/results/frdm-mcxa153.csv).

Software: MCUXpresso SDK 26.09.00 (GitHub route, no login), GCC 12.2.0 (the Zephyr SDK toolchain, the same GCC as this repo's other boards' gcc rows), Arm GNU Toolchain 14.2.Rel1 (GCC 14.2.1, the compiler version NXP lists for the SDK), pyOCD 0.45.1 with the NXP MCXA153 pack 26.09.00, Zephyr 4.4.2 with `hal_nxp`, FreeRTOS-Kernel as shipped in the SDK. NXP's flasher LinkServer was not installed (it needs an administrator password; the smaller open-source flasher was used instead).

## Setup: plug-in to a changed blink

| Path | Edit to proven changed blink | Build | Flash | Notes |
|---|--:|--:|--:|---|
| NXP SDK `led_blinky` at 250 ms, `west build`, pyOCD | 1.4 min | 30 s | 10.8 s | SDK and pyOCD already installed; board identified and backed up first |
| Zephyr 4.4.2 blinky at 250 ms, pyOCD | 2.1 min of machine time (build 124 s, flash 3.6 s) | 124 s | 3.6 s | `hal_nxp` fetched separately: 72 s, 1.3 GB; the edit and proof reads were not stamped to the second |
| MCUXpresso for VS Code | not run | | | the extension installs without a login (30 MB with its dependencies); it was not opened |

Proof each time: the red LED pin (P3_12) read over the debugger, toggling every 0.249 to 0.250 s (19 intervals) against 0.99 to 1.00 s for the stock program.

Install sizes, measured: the SDK for this board 2.3 GB (89 s, no login); pyOCD with its pack about 110 MB; the Arm GNU Toolchain 138 MB download and 1.0 GB installed; `hal_nxp` 1.3 GB. See `mac-setup/README.md`.

## CoreMark

| Compiler and flags | 96 MHz per second | Per MHz |
|---|--:|--:|
| GCC 12.2 `-O3` | 282.18 | 2.939 |
| Arm GNU 14.2 `-O3` | 275.29 | 2.868 |
| GCC 12.2 `-O2` | 263.51 | 2.745 |
| Arm GNU 14.2 `-O2` | 267.52 | 2.787 |
| GCC 12.2 `-Os` | 233.55 | 2.433 |
| Arm GNU 14.2 `-Os` | 219.27 | 2.284 |
| GCC 12.2 `-O3`, 48 MHz clock | 141.22 | 2.942 |
| Zephyr 4.4.2, GCC 12.2, speed setting (`-O2`) | 251.38 | 2.619 |
| Zephyr 4.4.2, GCC 12.2, size setting (`-Os`) | 223.50 | 2.328 |
| Arm GNU 14.2 `-O3`, 48 MHz clock | 137.58 | 2.866 |

Every run passed CoreMark's own check (the two Zephyr rows are single runs timed by the kernel's millisecond clock; the others were three runs and agreed to 0.0001 percent), and a stopwatch on the Mac agreed with the chip's tick count within 4 percent. Same unmodified EEMBC files as the LP-MSPM0G3507's GCC rows (1.82 per MHz at 80 MHz, so the M33 is about 1.6 times faster per MHz) and the TI M33 MSPM33C321A (3.40 per MHz at 160 MHz, 4.38 at 32 MHz, TI Arm Clang -O3). The comparison with the G3507 changes two things at once, the flags and the memory system (the G3507 has two flash wait states at 80 MHz; this chip has a cache), so 1.6 is not the core alone. Per MHz is the same at 48 and 96 MHz, so with the cache on, flash wait states cost under 0.1 percent. NXP's data sheet claims 381 per second (3.97 per MHz) without naming a compiler; the best measured here is 74 percent of that. Suspected, not tested: a different compiler, flags or benchmark build.

## Core timing

| Test | Result | Note |
|---|--:|---|
| Dhrystone 2.1, `-fno-inline`, rules followed | 0.835 DMIPS per MHz (80.2 DMIPS) | Arm GNU 14.2: 0.840 |
| Interrupt latency, pend to first line of the handler | 17 cycles best, 29 worst | includes one SysTick read; MSPM0G3507 29 to 36, MSPM33C321A 32 at 160 MHz |
| GPIO toggle, one register write | 1.12 cycles | 16 back-to-back stores; not checked on a scope |
| GPIO toggle, NXP driver call in a loop | 3.01 cycles | includes the loop branch |
| `sinf` plus `cosf` (software, no FPU) | 2.5k cycles | G3507 9.9k |
| 32-bit unsigned divide | 13.55 cycles | the M33 has a divide instruction; the G3507 takes 262 |
| Hardware CRC-32 over 1 kB against a table | 7.06 times (1.5k against 10.3k cycles) | G3507 13.5 times |
| Hardware AES | not applicable | no AES block ( The data sheet's security list is CRC only) |
| FreeRTOS yield round trip | 265 cycles (2.76 µs) | semaphore wake 524, notify wake 369, queue round trip 1.5k |
| FreeRTOS tasks before the heap runs out | 9 | NXP's example heap of 10 kB; a heap limit, not the chip's |
| Embench IoT, 19 of 19 | pass | split into six images because the SDK linker script gives the program 24 kB of the 32 kB RAM |
| CMSIS-DSP complex FFT 256 | q15 31.6k cycles (0.33 ms), q31 24.3k (0.25 ms), f32 450k (4.7 ms) | built from source for a core without the DSP extension; q31 beat q15, which may be because the q15 code cannot use packed 16-bit multiplies (suspected, not tested; see the FAQ); f32 is software float |

## Fastest ADC, and BitNetMCU's digit recognizer

| Test | Result | Note |
|---|--:|---|
| ADC, back-to-back 12-bit conversions of the chip's internal temperature sensor, eDMA to RAM | 2.53 million samples per second | ADC clock 48 MHz (the 96 MHz oscillator divided by 2), shortest sample time, no averaging, nothing wired. The data sheet says 4 million at 12-bit; whether a faster ADC clock or the DMA limits it was not tested |
| BitNetMCU, 64-wide CNN, 10 handwritten digits | 10 of 10 right, 1.30M cycles (13.6 ms) | the LP-MSPM0G3507 needed 2.64M at 32 MHz |
| BitNetMCU, 16-wide CNN / 12k fully connected / 1k fully connected | 10 of 10 each; 332k / 245k / 44k cycles | same code and models as the TI board; [experiment page](../docs/experiments/2026-10-11-frdm-mcxa153-bitnetmcu.md) |

## Control loop

| Test | Cycles | Microseconds at 96 MHz | Note |
|---|--:|--:|---|
| One FOC step, GCC 12.2 `-O2`, code in flash | 5.4k | 56.6 | the same source file as TI's F2800137 and L1306 boards |
| One FOC step, `-O3` | 5.3k | 55.7 | |
| LP-MSPM0L1306 (Cortex-M0+, soft float) | 12.7k | | 2.3 times slower |
| LP-MSPM33C321A (Cortex-M33 with FPU) | 489 at 32 MHz | | 11 times fewer cycles (cycles, not time) |
| LAUNCHXL-F2800137 (FPU and trig unit) | 400 | | 14 times fewer cycles |

The float step alone would allow about 17 kHz if nothing else ran. NXP's fixed-point RTCESL library (Clarke, Park, two PI controllers, inverse Park, space-vector modulation in Q15) does the same job in 448 cycles (4.7 µs), 12 times fewer; that is a speed result only, because its output was not compared with the float step. The full loop with the ADC trigger and interrupt is not measured.

## Room-occupancy models (plain C, 20 of 20 test vectors each, fastest of 20 vectors, best of 5)

| Model | GCC 12.2 -O3 | GCC 12.2 -Os | Arm GNU 14.2 -O3 |
|---|--:|--:|--:|
| 500 | 1.66 ms | 3.07 ms | 1.68 ms |
| 2k | 3.60 ms | 10.38 ms | 3.51 ms |
| 8k | 12.63 ms | 42.85 ms | 12.56 ms |
| 30k | 45.80 ms | 162.98 ms | 45.48 ms |

All four fit bare metal in one 113 kB image with 16 kB of RAM. The G3507 takes 35.3 ms for the 8k model with TI Arm Clang -O3 at 80 MHz, so this board is 2.8 times faster in time, with a different core, a clock 1.2 times higher, a different compiler and a different harness. Under Zephyr the app gets 24 kB of RAM (the other 8 kB is SRAMX); the 30k model under Zephyr 4.4.2 takes 64.3 kB of flash and 8.7 kB of RAM and passes 20 of 20.

## NXP's examples

96 of the 98 example projects for this board built from a terminal with `west` (the board's folder lists 107 entries: two, `freqme` and `power_mode_switch/low_level`, have no example folder of that path in the SDK tree (the power-mode example is one of the four held for later and was not built); seven hold no project, namely `pwm`, `wwdt` and five `se05x_*`; that leaves 98 projects, of which 96 built). The two that failed fail with both compilers. `safety_iec60730b` stops on a `-Werror` warning inside NXP's middleware, and `freertos_tickless` includes a header (`tickless_api.h`) that is not in the SDK. Of the 43 examples that can run alone, 42 started and ran for 6 seconds with no fault flag read through the debugger afterwards; what each one does was not checked (41 printed on the serial port; `led_blinky` is silent by design). `driver_examples/cdog` resets on purpose; it printed and raised no fault flag, so it is counted clean. `trdc/basic` faults on purpose (it demonstrates the memory protection unit, MPU) and is counted as not clean. 57 were skipped by plan (they need a second board, a secure element, a USB cable in the chip's own port or a motor shield) and 4 are not run because they can drop the debugger or change the flash layout (`freertos_tickless` is one of the four and also did not build, so it is in both counts). The per-example list is not published; the counts are rows `examples_built` and `examples_ran_clean` in the CSV.

## Not run, and why

| Test | Why |
|---|---|
| Fastest clean UART speed, PWM-to-ADC error | need a jumper wire, a resistor and a capacitor on the board's headers; not fitted |
| Control loop with an ADC trigger and interrupt entry time | the program is built (a timer starts an internal-sensor conversion, the ADC interrupt runs the FOC step and writes a compare value); running it left the chip refusing debugger connections, so there is no number |
| Run current per MHz, sleep current | need a series meter; none available |
| MCUXpresso for VS Code first blink, LinkServer | LinkServer's download link redirects to an NXP login; the VS Code extension was installed and removed but not opened. Needs an NXP account |
| Moving TI examples to this board with an agent, and NXP examples to the LP-MSPM0G3507 | needs NXP's 2.3 GB SDK reinstalled; not run |

## How well does an AI agent do blink, CoreMark and a recovery question on the FRDM-MCXA153? (2026-10-12)

On the NXP FRDM-MCXA153, a fresh Claude Sonnet 5.5 agent, three runs per job with this repo's docs and three without, each result checked on this bench (rows `agent_pass` and `agent_minutes` in `data/results/frdm-mcxa153.csv`). 17 of 18 passed. The tools were already installed (NXP's SDK, Arm GNU 14.2 and pyOCD), the jobs named pyOCD and `led_blinky`, and no agent used LinkServer or MCUXpresso for VS Code, so this is not a box-to-blink time for NXP's own tools. The recovery question was answer-only: nothing was flashed or recovered. "Median" is the middle run of three, and "locked out" means the ADC program of this repo left the chip refusing every debugger connection (see the FAQ); it is not NXP's security lock. Blink (NXP's `led_blinky` at 300 ms, LED pin read by script): 6 of 6, median 4.8 minutes with this repo's docs, 5.0 without (a tie: this repo's docs did not help the blink). CoreMark: 6 of 6, 5.1 minutes with this repo's docs (275.1 to 275.3 CoreMark iterations per second), 8.0 without (272.6 to 308.7; the 308.7 run added loop-unrolling flags, so it is not comparable with the others). Question (how to recover a chip whose debug port is locked out): 3 of 3 with this repo's docs in 0.6 minutes, 2 of 3 without in 1.9. No rule breaks that mattered (each transcript was read for another probe, kill commands and writes outside the run folder; one run read `git status` in this repo and one killed its own serial reader). Rows: `agent_*` in [data/results/frdm-mcxa153.csv](../data/results/frdm-mcxa153.csv).
