# TI LP-AM263P LaunchPad: CoreMark, motor-control step, flash, four cores and what did not run

**Short answer.** CoreMark runs at 1,464 per second on one Cortex-R5F at 400 MHz with TI Arm Clang -O3 (3.66 per MHz) from on-chip RAM with the caches on, and at 1,498 (3.74 per MHz) with CoreMark's own code in the tightly coupled memory; TI's own CoreMark example gives 1,464.5. One motor-control step takes 160 cycles with the TMU (0.40 microseconds), about 4 times faster in time than the C2000 F2800137 and 1.5 times faster than the AM13E230. From the external flash with TI's 64 kB cache, CoreMark matches RAM; all four cores run CoreMark at once with each within 0.1 percent of alone; lockstep costs no speed; and Zephyr 4.4.2 boots with this repo's board files. A current-loop pass from PWM trigger to new PWM value takes 1.24 microseconds with the TMU. Of TI's 284 example projects for the board, 249 build from the Mac terminal and 118 of the 208 loaded print a pass (`examples_built` and `examples_ran_clean` rows). The ADC rate, the UART loop and power did not run: the list is at the end.

Terms: 1 kB = 1,024 bytes. **OCSRAM** is the on-chip RAM the SDK calls OCRAM (the data sheet lists 3 MB; a program using above 2 MB did not start here); **TCM** is the core's tightly coupled memory. Every row, with its conditions, is in `data/results/am263p.csv`. Start-here guide: [README](README.md). Problems and fixes: [FAQ](FAQ.md).

Measured 2026-10-09 on an Intel Mac (macOS 15.7.9, load average 4 to 6; cycles are counted on the chip). Board: LP-AM263P Rev A (PROC171A), AM263P4, probe firmware 3.0.0.29, MCU+ SDK 26.01.00.08, TI Arm Clang 5.1.1.LTS, every program loaded into RAM by DSLite and run from there (no flash was written). Core R5FSS0-0 only, both L1 caches on, SDK code and CoreMark's data as the tables say.

**The clock.** The SDK reports 400,000,000 Hz for the core. Check against the Mac's clock: a program printed one line every 1.0 s of that clock for 31 lines, the Mac time-stamped each line as it arrived, and a straight-line fit gives 399.9 MHz. The two end lines alone give 401.8 MHz, because lines arrive up to 0.1 s early or late over USB; the fit uses all 31. CoreMark's own timed run of 13.66 s read 13.81 s on the Mac (1 percent, the same USB jitter).

## Core

| Test | Result | Note |
|---|---|---|
| CoreMark, this repo's port, -O3 / -O2 / -Os / -Oz, code and stack in OCSRAM | 1,464.2 / 1,026.0 / 877.5 / 850.0 per second | Unmodified EEMBC files, validated by CoreMark's CRCs, 3 runs each; the runs differ by under 0.002 percent |
| Same, CoreMark's code in TCM, stack in TCM | 1,497.8 / 1,039.8 / 870.4 / 854.2 | -Os is 0.8 percent slower in TCM than from OCSRAM; unexplained |
| CoreMark, TI's own example (`coremark_benchmark`, unchanged, -O3) | 1,464.5 per second | 15k iterations; within 0.02 percent of this repo's -O3 |
| CoreMark per MHz, -O3, OCSRAM / TCM | 3.661 / 3.744 | TI's page shows 3.769 on the LaunchPad |
| Dhrystone, DMIPS per MHz | 1.497 (-O3, no inlining), 1.445 (-Os), 1.883 (-O3 with inlining, not rules-compliant), 1.941 (TI's example) | TI's page shows 1.961 |
| sine plus cosine pair, TMU / TI mathlib / C library | 26.9 / 77.8 / 300.3 cycles | 11 times fewer cycles with the TMU than the C library |
| Float divide (FPU) | 15.9 cycles | The SDK's TMU has no divide |
| GPIO pin write, register and SDK driver | 10.0 cycles each | A store into the GPIO block, 25 ns |
| Interrupt, software-pended, to the first line of a C handler: IRQ / FIQ | 195 / 144 cycles (0.49 / 0.36 us) | Through TI's interrupt dispatcher; the external-edge version needs a wire |
| FreeRTOS task switch, TI's example: semaphore / notification | 6.06 / 5.13 us | TI's page shows 0.763 and 0.558; see below |

## Control

| Test | Result | Note |
|---|---|---|
| One FOC step, TMU / TI mathlib / C library | 160.3 / 247.4 / 412.0 cycles (0.40 / 0.62 / 1.03 us) | Same `foc.c` as the F2800137, mean of 1,000 steps from OCSRAM; worst 833 / 263 / 789 |
| TI's TMU park transform example | 98 cycles TMU, 249 TI mathlib, 625 C library | TI's own program, on the same board |
| Current loop, trigger to new PWM value at 50 kHz (ADC conversion, interrupt, FOC step, compare write), TMU / TI mathlib / C library | 1.24 / 1.45 / 2.06 us | 2026-10-11, code in OCSRAM, 1,000 passes x 3 runs, no missed period; 0.63 us of it is conversion plus interrupt entry; [comparison with the AM13E230 and F2800137](../docs/experiments/2026-10-11-current-loop-three-mcus.md) |
| Same loop with the R5F cores in lockstep (split mode in the row above) | 1.236 / 1.455 / 2.068 us | Split mode: 1.235 / 1.454 / 2.060. The C-library build is about 3 cycles slower in lockstep; the others match to a cycle. [Lockstep and a loaded neighbour](../docs/experiments/2026-10-11-am263p-lockstep-and-loaded-loop.md) |
| Same loop (TMU, 200,000 passes, split mode) with CoreMark, or a 64 kB memory copy, on the other three cores | 1.234 us, worst 2.11 us | Identical to the cycle with the other cores idle, 3 runs each; the copy load was 179 MB/s across the three cores with the library `memcpy` and 779 MB/s with this repo's word loop, each core in its own block of on-chip RAM |
| Same, the three cores copying in the RAM next to core 0's block (899 MB/s) | 1.234 us, worst 2.51 to 2.70 us | Mean and best unchanged; the worst pass is the first after the load, with cold caches |
| Same loop with its handler, data and stacks in TCM, alone or beside those copies | 1.194 us, worst 1.26 us | The first pass is no longer the slowest; spread (worst minus best) 0.09 to 0.10 us; the copies do not move the loop |
| Memory copy, 64 kB block in on-chip RAM, per core: C library `memcpy` / this repo's 8-byte loop | 58.6 to 61.4 / 235 to 297 MB/s | The library copy in this build is a byte loop; the same with the control loop running beside |

## From flash (execute in place)

Measured 2026-10-10 and 11. The flash ran at 16.7 MHz (its high-speed read tuning failed), so these are slower than TI's pages; details on the [execute-in-place page](../docs/experiments/2026-10-10-am263p-xip-ladder.md).

| Test | RAM | Flash, no extra cache | Flash, 64 kB L2 cache |
|---|---|---|---|
| CoreMark, TI's example, -O3 | 1,464.5 | 1,394.5 | 1,464.5 |
| TI's OptiFlash memcpy benchmark, 0 B row | 14.60 ms | 126.55 ms | 34.57 ms (13.26 ms with smart placement) |
| TI's 12 AI examples, one inference | baseline | same to 7.8 times as long | 11 within 2 percent of RAM, one 6.9 times as long |

## All four cores, and other operating systems

Measured 2026-10-11; details on the [four-cores page](../docs/experiments/2026-10-11-am263p-four-cores-mixed-os.md).

| Test | Result | Note |
|---|---|---|
| CoreMark on all four cores at once | 1,462.1 / 1,464.1 / 1,463.7 / 1,464.1, sum 5,854.0 | Each within 0.1 percent of its score alone (1,463.4 / 1,464.1 / 1,463.7 / 1,463.5) |
| Message round trip, TI's IPC, FreeRTOS core to bare-metal cores | 3.49 us with one partner, 3.81 us each with three | 1,000,000 round trips per partner |
| Same with ThreadX on core 0 | 3.36 / 3.87 us | |
| Zephyr 4.4.2 | Boots and prints on R5FSS0-0; runs beside TI's FreeRTOS on R5FSS0-1 | This repo's out-of-tree board files in `zephyr/`; no timer driver |
| CoreMark with the cores in lockstep, TI's example | 1,464.52 | Split mode 1,464.53. Reached from the debugger with `tools/lockstep.js`; TI's lockstep connection file did not get there in these tries ([FAQ](FAQ.md#lockstep)) |

## What was expected, and where the numbers disagree

Expectations were written before the runs.

- **CoreMark 1,508 per second (3.769 per MHz): measured 1,464 from OCSRAM, 2.9 percent lower.** Moving CoreMark's code and stack into TCM gives 1,498, 0.7 percent below TI's page, so code placement explains most of the gap. TI's page does not say where its program ran; suspected: TCM. TI's own example, built by TI's makefile, gives 1,464.5, so the difference is not in the port. A first build read 223 per second because the build script passed `-o3` (no optimization); the capital-O build is what the table shows.
- **Dhrystone 1.96 expected, the port's 1.50.** The expectation said the port's build forbids inlining and would be lower. The same sources with inlining allowed read 1.88 and TI's example reads 1.94, so the compiled code is fine and the gap is the inlining rule.
- **TMU sine and cosine 15 to 40 cycles expected: 26.9.** TI's `tmu_operations` example prints 35.0 for its sincos test and 98 cycles for a whole park transform.
- **TI software mathlib about 71 cycles expected: 77.8.** TI's mathlib benchmark prints 79.0 for the same function.
- **C library about 590 cycles expected: 300.3.** The 590 is TI's page for the double-precision `sin` and `cos`; the row here calls the float `sinf` and `cosf`. Suspected: float against double; the double version was not timed. TI's benchmark on this board prints 276 plus 278 for its double calls, in line with its page.
- **FOC step with the C library 650 to 800 cycles expected: 412.** Same cause. Second check: 300.3 cycles for the sine and cosine plus the rest of the step (about 110) adds up to 412. With the TMU and TI's mathlib the step is inside the expected ranges (150 to 250 and 190 to 260).
- **Against the F2800137** (194.1 cycles, 1.62 us from RAM): 4.0 times faster in time, inside the expected 2.6 to 4.3. **Against the AM13E230** (117 cycles, 0.585 us): 1.46 times faster in time, at the top of the expected 0.7 to 1.5.
- **Interrupt entry 40 to 120 cycles expected: 195 (IRQ), 144 (FIQ).** The expectation was for an external edge on a bare vectored interrupt; this row pends the interrupt in software and goes through TI's dispatcher, which saves registers before the handler. Suspected: the dispatcher; an edge or a hand-written vector was not timed.
- **Task switch 8 to 9 times slower than TI's page: unexplained.** Same example, same chip, same compiler flag, same memory placement (OCSRAM, caches on). The Mac saw 12.1 s for the first loop, matching the printed 12,127,866 us, so the timer is right. No separate harness was built.
- **One outlier in the FOC step.** The TMU build's worst step is 833 cycles with a best of 156 and a mean of 160, the same in all three runs; about six of 1,000 steps are slow. Cause not found (suspected: a cache or branch-predictor miss, not a TMU stall).

## What did not run

| Test | Why |
|---|---|
| CoreMark from flash with TI's smart placement | Smart placement was run only on TI's memcpy benchmark |
| Interrupt latency from an external edge | Needs a jumper wire from a pin to an interrupt input |
| ADC rate, DAC to ADC error, UART loop speed | The first needs a new ADC plus DMA program; the last two need jumper wires |
| Power loop and the control loop's rate sweep | Not reached |
| TMU in the other cores, AES and SHA with the on-chip crypto block, post-quantum rows | Not reached |
| Embench, FFT, CRC | Not reached |
| Power, energy per CoreMark | Needs the USB-C power meter |
| Zephyr with a timer, IPC, or on cores 2 and 3 | The port has no driver for the AM263P's RTI timers |

## How these numbers were made

CoreMark and Dhrystone use the unmodified source files and a small port: the cycle counter of the R5F's performance monitor, results printed once to the board's console (the first XDS110 serial port, 115200 baud) and also left in a RAM structure. Programs are built with `tools/build_bench.sh` against the SDK's SysConfig output for TI's CoreMark example (400 MHz, UART0 console) and loaded with `tools/capload.py`, which opens the serial port before the load. Mean, best and worst cycle counts come from the same program; the three runs of each row are rows in `data/results/am263p.csv`. The RAM structure was not read back by the debugger.
