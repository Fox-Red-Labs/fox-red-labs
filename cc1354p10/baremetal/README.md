# How fast is the LP-CC1354P10-1 (Cortex-M33) on bare metal with TI Arm Clang?

EEMBC CoreMark scores **199.92 at -O3 (4.17 per MHz at 48 MHz)** and 94.70 at -Oz, with TI Arm Clang 5.1.1 and no operating system, measured on the board on 2026-10-06, three runs each. Interrupt latency from pend to handler is 23 to 24 CPU cycles at -O3. A GPIO register toggle takes 1.51 cycles per loop pass.

Terms: [docs/vocab.md](../../docs/vocab.md). 1 kB = 1,024 bytes. Built and run from an Intel Mac (macOS 15.7.9) with SimpleLink Low Power F2 SDK 8.33.00.16 and SysConfig 1.26.3, loaded with CCStudio 21.0.1 scripting (this board does not start a DSLite-flashed program until a power cycle).

## Results, measured 2026-10-06

| | -O3 | -Oz |
|---|--:|--:|
| CoreMark, iterations per second (median of 3) | **199.92** | 94.70 |
| CoreMark per MHz | **4.17** | 1.97 |
| GPIO toggle, TI driver call, cycles | 15.08 | 19.01 |
| GPIO toggle, register write in a loop, cycles | 1.51 | 8.01 |
| Interrupt latency, pend to handler, direct vector, cycles (includes one SysTick read) | 23 to 24 | 25 to 29 |
| Flash used (whole benchmark image) | 43.3 kB | 30.8 kB |

The three -O3 runs gave 199.9208 to 199.9210; the three -Oz runs 94.7041 to 94.7063. Every run passed CoreMark's own check ("Correct operation validated", crcfinal 0x4983). Clock read back as 48 MHz (`ClockP_getCpuFreq`). Cache on (VIMS CTL 0x20000005). Mac load average about 2.5 on 12 cores during the runs.

How it compares, same method and compiler:

- **Against Arm's figure**: Arm publishes 4.02 CoreMark per MHz for the Cortex-M33. 4.17 here is slightly above it.
- **Against the CC2340R5** (Cortex-M0+, also 48 MHz): 111.09 at -O3, 2.31 per MHz (`cc2340r5/baremetal/README.md`). The M33 does 1.8 times the work per clock.
- **Against the MSPM0G3507** (Cortex-M0+, 80 MHz): 183.61 (`mspm0g3507` rows in `data/results/mspm0g3507.csv`). The CC1354P10 beats it at 60% of the clock.

## Run it

    ./run.sh O3      # or Oz: builds, loads with CCStudio scripting, prints the serial report

`run.sh` checks that the board printed the flags that were built and stops with "WRONG IMAGE" otherwise: on 2026-10-06 one "-Oz" run was really the previous -O3 image still running after a load that did not take, and its numbers looked valid.

## The port from the CC2340R5 bench

Same `bench.c`, `bench_timer.h` and unmodified EEMBC CoreMark files as `cc2340r5/baremetal/`, with:

- `PowerCC26XX.h` and `PowerCC26XX_DISALLOW_STANDBY`/`_IDLE` instead of the CC23 power names.
- `cmsis/cc26x4.h` plus `cmsis/core/core_cm33.h`: the F2 SDK's device header does not pull in the Arm core header (the F3 SDK's does).
- Spare interrupt for the latency test: I2S (unused here) instead of the CC23's LRFD IRQ0.
- Cache report: VIMS CTL instead of the CC23's wait-state and cache registers.
- Makefile and SysConfig from TI's `examples/nortos/LP_EM_CC1354P10_1/drivers/empty` plus the UART2 lines from `uart2echo`; no CCFG CRC step (F2 parts have none).
