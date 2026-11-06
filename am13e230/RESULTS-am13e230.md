# TI LP-AM13E230 LaunchPad: CoreMark, motor-control step, neural accelerator and what did not run

**Short answer.** CoreMark runs at 872.4 per second at 200 MHz with TI Arm Clang -O3 (4.36 per MHz), matching the data sheet's 870, and TI's own CoreMark files give 871.6. One motor-control step takes 117 cycles with the TMU (0.585 microseconds), 3.1 times faster in time than the C2000 F2800137. The four room-occupancy models fit and take 0.5 to 18.7 ms in plain C. The neural accelerator runs them 4.7 to 8.8 times faster than the same TI-compiled model on the CPU at the same clock (2.4 times on the largest, where one layer stays on the CPU). The whole trigger, ADC, step and PWM-write loop takes 1.33 microseconds with the TMU and 4.74 in plain software. Power did not run. Later runs (2026-10-09) are in the table at the end of this page.

1 kB = 1,024 bytes. Every row, with its conditions, is in `data/results/am13e230.csv`. Start-here guide: [README](README.md). Problems and fixes: [FAQ](FAQ.md).

Measured 2026-10-08 on an Intel Mac (macOS 15.7.9, load average 5 to 25; cycles are counted on the chip). Board: LP-AM13E230 Rev E2, prototype silicon, probe firmware 3.0.0.41, AM13E2x SDK 26.01.00.03.STS, TI Arm Clang 5.1.1.LTS, code in flash with 3 wait states and the cache on. The clock was checked against the Mac's clock: 201.4 MHz at 200, 99.4 at 100 and 49.6 at 50.

## Core

| Test | Result | Note |
|---|---|---|
| CoreMark, this repo's port, -O3 / -O2 / -Os / -Oz | 872.4 / 612.7 / 477.1 / 438.1 per second | Unmodified EEMBC files, validated, 3 runs each |
| CoreMark, TI's port, same four flags | 871.6 / 612.5 / 476.7 / 438.3 | Within 0.1 percent of this repo's port |
| CoreMark per MHz at 200, 100 and 50 MHz | 4.36, 4.42, 4.42 | 1 wait state below 100 MHz |
| sinf plus cosf, TMU / C library | 11.6 / 512.7 cycles | 44 times fewer cycles with the TMU |
| Divide, TMU / FPU | 7.9 / 15.1 cycles | |

## Control

| Test | Result | Note |
|---|---|---|
| One FOC step, TMU / C library | 117.0 / 655.6 cycles (0.585 / 3.28 us) | Same `foc.c` as the F2800137; see [the experiment](../docs/experiments/2026-10-08-am13e230-foc-step.md) |
| TI's ACI chain, FOC-only ISR | 555 cycles, PASS | Full chain 637 |

## AI, plain C, 20 of 20 test vectors on every build

| Model | -O3 | -Os |
|---|---|---|
| occ_cnn_500 | 0.489 ms | 0.681 ms |
| occ_cnn_2k | 1.250 ms | 1.859 ms |
| occ_cnn_8k | 5.055 ms | 7.608 ms |
| occ_cnn_30k | 18.71 ms | 28.63 ms |

## Analog

The DAC read back by the ADC through the J3 wire reads high by 4.4 to 9.4 percent at the four mid-scale points (the plan expected about one DAC step, 0.1 percent), the same on two runs; TI's example reports 3 errors of 5 at plus or minus 5 percent. Cause unexplained. The UART loop (J1 wires, UC0 to UC1) is clean at every speed tried, up to 6.25 Mbaud, 128 bytes per pass. The first two runs failed on 2026-10-08 because a wire was open; it was fixed at the bench and both wires were proved closed by driving each pin as GPIO.

## NPU against CPU, same model, same clock

**Expectation, written before the run (2026-10-08).** The TinyEngine NPU is the same block as on the MSPM0G5187, so the pattern should match: a large gain on the mid-size models, a smaller one on the 500-parameter model (set-up per layer dominates) and on the 30k model (NNC kept one convolution on the CPU on the G5187). The M33 has an FPU and DSP instructions, so its CPU build should be faster per multiply-add than the G5187's M0+ (about 23 to 35 cycles per multiply-add there), by a factor of three or more.

**Method.** The same four models (`model_tinpu_int8.onnx`), compiled by TI's Neural Network Compiler 2.1.2 on GitHub's Linux machines (TI ships no Intel-Mac build) with TI Arm Clang 5.1.1.LTS and TI's documented Cortex-M33 options, once for the NPU (`--target="c, ti-npu"`) and once for the Cortex-M33 alone (`"c, ti-npu type=soft"`). Both run on the board from flash at 200 MHz with the same harness: 20 test vectors checked, then one `tvmgen_default_run` call timed with the DWT cycle counter, best of 5 per vector, fastest vector quoted. Three runs per build, each a full erase, flash and run. Every run matched the expected class on 20 of 20 vectors. The fastest-vector cycle count was identical across the three runs on 7 of 8 builds; the 500-model NPU build differed by 11 cycles on one run (21,333 against 21,344), under 0.1 percent.

| Model | Multiply-adds | CPU (M33) | NPU | Speed-up | CPU cycles per multiply-add | NPU cycles per multiply-add | Runs |
|---|--:|--:|--:|--:|--:|--:|--:|
| occ_cnn_500 | 11,328 | 0.499 ms | 0.107 ms | 4.7x | 8.81 | 1.88 | 3 |
| occ_cnn_2k | 33,856 | 1.376 ms | 0.205 ms | 6.7x | 8.13 | 1.21 | 3 |
| occ_cnn_8k | 157,824 | 5.544 ms | 0.630 ms | 8.8x | 7.03 | 0.80 | 3 |
| occ_cnn_30k | 610,560 | 22.91 ms | 9.535 ms | 2.4x | 7.50 | 3.12 | 3 |

Against the plain-C `-O3` builds in the table above (0.489, 1.250, 5.055 and 18.71 ms, quantize step included), the NPU is 4.6, 6.1, 8.0 and 2.0 times faster. TI's CPU build is 2, 10, 10 and 22 percent slower than plain C `-O3` on the four models, so use the same-compiler pair above for "what the NPU adds".

**What came out.**

- Both parts of the expectation held. The M33 CPU build costs 7 to 9 cycles per multiply-add, three to five times fewer than the G5187's M0+ (21 to 35), and the NPU gain grows from 4.7 times on the smallest model to 8.8 times on the 8k model, then falls on the 30k model. Per cycle the AM13E230's NPU is faster than the G5187's: 126k cycles for the 8k model against 169k.
- In time the gap to the G5187 is wider because of the clock: the 8k model takes 0.630 ms here at 200 MHz and 2.116 ms there at 80 MHz.
- The 30k model gains only 2.4 times. NNC's compile log says `GCONV layer NOT offloaded to TI-NPU` (the weights could overflow the NPU's 18-bit accumulator), the same warning as for the G5187, so that layer runs on the CPU.
- **Not working:** the Cortex-M33 CDE build (`type=m33cde`, custom instructions on the CPU). NNC 2.1.2's generated code does not compile with TI Arm Clang 5.1.1 (an incompatible-pointer-types error in `lib2.c`); with that error turned into a warning it builds but the board takes an instruction bus error before the first result (CFSR 0x100, HFSR FORCED). Not chased; rows are `failed`.
- **Energy per inference: not measured.** The board's EnergyTrace needs the J10 shunt lifted, which needs someone at the bench. Energy rows stay `waiting`.
- **TI's own AM13E2x SDK AI examples:** the SDK ships thirteen as prebuilt libraries, each a different model and no ONNX file, so no same-model CPU and NPU pair can be built from it. Twelve ran on the NPU and were timed (table below); `jerk_detection` needs a BMI270 motion sensor and was not run.

Compiler and options, from TI's NNC user's guide section 2.2.4 (<https://software-dl.ti.com/mctools/nnc/mcu/users_guide/compiling.html>, read 2026-10-09): "The following example compiles a model into a library that can run on an Arm Cortex-M33 core with NPU acceleration" (`--target="c, ti-npu" --target-c-mcpu=cortex-m33`) and "...without NPU or CDE acceleration" (`--target="c, ti-npu type=soft"`). The guide's Cortex-M33 flags are `-DARM_CPU_INTRINSICS_EXIST -mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16 -mlittle-endian -O3`. The Tiny ML Tensorlab 1.4.0 guide lists the AM13E2 among the TINPU devices ("AM13E2 | AM13 | 8-bit inference, Cortex-M33 + NPU"). The compile ran as a GitHub Actions job; the rows are in `data/results/am13e230.csv`.

## Later runs, 2026-10-09 (200 MHz, code in flash, TI Arm Clang 5.1.1.LTS)

Each row is in `data/results/am13e230.csv`.

| Test | Result | Note |
|---|---|---|
| Dhrystone 2.1, -O3 without inlining | 1.046 per MHz (368k per second) | The data sheet says 325 DMIPS, 1.63 per MHz, and states no flags |
| Embench | 19 of 19 pass | One group needs 7.8 kB of stack and overflowed the default 4 kB, so it ran with 12 kB |
| FFT, 256 points, SDK CMSIS-DSP library | f32 23.4k cycles; q15 and q31 are in the CSV | Does not use the TMU |
| AES-256 ECB, hardware against software | 20 to 30 times faster in hardware (1 kB: 12.1k cycles against 359k) | The software side is tiny-AES-c unmodified |
| CRC-32 over 1 kB | 1.89 times faster in hardware (3.3k cycles against 6.2k) | The CPU feeds the CRC unit word by word |
| FreeRTOS task switch | 1.49 microseconds best, 2.105 worst of 1,000 passes | Yield between two tasks; interrupt-to-task tests not ported |
| Current loop, trigger to new PWM value | 1.33 microseconds with the TMU, 4.74 without | MCPWM4 zero event to ADC to interrupt to PWM write |
| TI's AI examples on the NPU, one model run | 0.089 ms (`generic_timeseries_regression`) to 31.0 ms (`washing_machine_load_weighing`) | 12 examples, TI's golden-output check passes in each; feature extraction is extra |
| SDK LaunchPad examples | 100 of 100 build; 79 of 80 run clean | "Clean" means verify passes and the fault registers read zero. The one failure is `mathlib/fastrts` (see the FAQ). 20 were skipped, for example motor-control solutions that need a power board |

## Did not run, and why

- Cortex-M33 CDE build from TI's compiler: built, hangs on an instruction bus error (see the NPU section above).
- Power and energy rows (sleep, shutdown and active current, CoreMark and per-inference energy): need the J10 shunt lifted and a meter.
- ADC maximum rate, interrupt entry latency and GPIO toggle time: no result (`waiting` rows).
- `jerk_detection` from TI's AI examples: needs a BMI270 motion sensor.
