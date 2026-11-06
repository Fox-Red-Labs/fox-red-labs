<!-- experiment
id: E138
board: LP-AM263P
date: 2026-10-11
status: works
reached: verified
line: The same motor-control step (one current-loop pass, F28-style C code, sine and cosine on each chip's TMU) runs trigger-to-new-PWM in a mean 1.24 µs on the LP-AM263P (400 MHz Cortex-R5F), 1.33 µs on the LP-AM13E230 (200 MHz Cortex-M33) and 2.58 µs on the LAUNCHXL-F2800137 (120 MHz C28x). Split in two, the AM263P runs the control code fastest (0.61 µs against 0.97 and 2.03) but takes longest to reach the handler (0.63 µs against 0.37 and 0.56), so the totals end close.
stop: none
-->
# How fast is the same current loop on the LP-AM263P (AM263P4), LP-AM13E230 and LAUNCHXL-F2800137?

**Short answer.** One pass of a field-oriented current loop, from the PWM event that starts the ADC to the new PWM compare value, with the same C code on each chip and the chip's trig unit (TMU) doing sine and cosine. Mean of 1,000 passes:

| LaunchPad | CPU | Trigger to handler (ADC conversion + interrupt entry) | Handler (FOC step + PWM write) | Trigger to new PWM value |
|---|---|---|---|---|
| LP-AM263P | Cortex-R5F, 400 MHz | 0.63 µs | 0.61 µs | **1.24 µs** |
| LP-AM13E230 | Cortex-M33, 200 MHz | 0.37 µs | 0.97 µs | **1.33 µs** |
| LAUNCHXL-F2800137 | C28x, 120 MHz | 0.56 µs | 2.03 µs | **2.58 µs** |

The AM263P runs the control code itself about 1.6 times faster than the AM13E230, and gets to the handler 0.27 µs later, so the totals are close. At 50 kHz the AM263P's loop takes about 6% of the 20 µs period on average and at most 10.7%.

Terms: [vocab](../vocab.md). TMU = trigonometric math unit, hardware that computes sine and cosine. FOC = field-oriented control, the usual way to drive a brushless motor; one step reads currents, transforms them, runs two regulators and computes three PWM duty values. Problems and fixes: [LP-AM263P FAQ](../../am263p/FAQ.md). Rows: `data/results/<board>.csv`, tests `control_loop`, `isr_entry`, `control_loop_worst_pct`, `control_loop_jitter`. Code: TI's `adc_soc_epwm` example with the repo's control loop added, on each board.

**Question.** If a product spins a motor, how much of the control period does the current loop take on each of these chips, measured the same way?

**Vendor position.** TI's AM263Px MCU+ SDK 26.01 page for its TMU example (read 2026-10-11): "This example calls the mathlib operations for 500 different input values. Calls the math library operations with TMU mathlib APIs, SDK mathlib APIs and CLANG mathlib APIs. Compares the performance between these three versions". TI publishes per-function cycle counts like these; no published trigger-to-PWM time was found for a whole loop on these LaunchPad development kits.

**What was tried.** The same `foc.c` and `foc.h` on every chip, unchanged (one file, `foc.c` sha1 c69a210d, included by all three builds) (single-precision float; sine and cosine through each chip's TMU). On each LaunchPad: a PWM time base at 50 kHz whose period start triggers one ADC conversion of a pin with nothing driven; the ADC's end-of-conversion interrupt runs one FOC step and writes the PWM compare value. The PWM counter is read at the handler's first line and after the compare write, so both times are measured from the trigger. 1,000 passes per run. Intel Mac, macOS 15.7.9.

- LP-AM263P (2026-10-11): MCU+ SDK 26.01.00.08, TI Arm Clang 5.1.1, -Os (TI's example makefile), code in on-chip RAM; TI's `adc_soc_epwm` example with two SysConfig lines changed (EPWM0 period 3999, ADC trigger at counter zero); interrupt through TI's interrupt dispatcher; 3 runs per variant.
- LP-AM13E230 (2026-10-09): AM13E230x SDK 26.01.00.03, TI Arm Clang 5.1.1, -O2, code in flash with cache; 1 run.
- LAUNCHXL-F2800137 (earlier runs): TI's C2000 compiler and C2000Ware, code in flash; 1 run per row.

**How far it got.** Ran and checked on all three. On the AM263P: no PWM period missed in 3,000 passes per variant, and the PWM clock checked against the CPU clock (200 MHz, half a count per CPU cycle).

**AM263P with three ways to compute sine and cosine** (means; best and worst in cycles at 400 MHz; median of 3 runs):

| Sine and cosine from | Trigger to new PWM value | Best | Worst | Trigger to handler |
|---|---|---|---|---|
| TMU | 1.24 µs | 484 | 856 | 0.63 µs |
| TI's software mathlib | 1.45 µs | 576 | 940 | 0.64 µs |
| C library | 2.06 µs | 738 | 1,094 | 0.65 µs |

With the TMU and the mathlib, the worst pass is the first one after the program loads (cold caches). With the C library the first pass takes 2.51 µs and the worst is a later pass (2.74 to 2.80 µs); which pass was not found.

**The line.** Nothing stopped. The comparison has limits:

- **Setup or handling:** code location differs: on-chip RAM on the AM263P (it has no internal flash), flash with cache on the AM13E230, flash on the F2800137 (2.32 µs with the step copied to RAM).
- **Setup or handling:** compiler settings differ (-Os on the AM263P, -O2 on the AM13E230), and so do the cores and how each TMU is reached. Clock speed is not the only variable.
- **Chip limit:** each chip's ADC converts at its own speed, and the conversion is inside the trigger-to-handler time; the conversion alone was not measured.
- **Setup or handling:** the AM263P's interrupt goes through TI's interrupt dispatcher. A handler wired to the core's fast interrupt (FIQ) would enter sooner: an earlier software-pended test measured 195 cycles through the dispatcher and 144 as FIQ (best of 200; worst 227 and 205).
- The AM263P figures are medians of 3 runs; the AM13E230 and F2800137 figures are single runs.

- The AM263P handler on FIQ, and the step in the core's tightly coupled memory.
- The AM263P's ADC early-interrupt option, as on the F2800137's `early` rows (0.39 µs to the handler there).
- The same loop on core 1 while the other three cores run CoreMark, to see whether shared memory traffic moves it.
- A real motor and power stage.

**Ask.** If you run current loops on any of these parts, post your trigger-to-PWM numbers and how you measured them.
