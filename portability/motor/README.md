# Portable motor-control core in C99

Plain C99, no vendor include. `core/` is the control kernel, the Hall angle code, the safety guard and the cost stamps; `tests/` checks them on the Mac and measures what each feature costs in flash and RAM for three toolchains. The design is `DESIGN.md`; this folder is its first slice. Boards, the hardware layer (`hal.h`) and the profiles are not here. Terms: [docs/vocab.md](../../docs/vocab.md).

## What is here

| Path | What it holds |
|---|---|
| `core/frl_cfg.h` | `FRL_TIER` 0 to 5 and the `FRL_F_*` feature flags (`FRL_F_GUARD` and `FRL_F_COST` are added to the design's list); refuses impossible combinations at compile time, and float on an Arm core or C28x without an FPU |
| `core/frl_num.h` | `frl_real` as float, Q15 or IQ24 (one macro, `FRL_NUM`); angles in per-unit turns; `FRL_SINCOS` with libm, a 256-entry interpolated table, or a hook for a trig unit or math accelerator |
| `core/kernel.h` | Clarke, Park, inverse Park, PI with conditional integration, min-max modulation, ramp, q-axis voltage limit; static inline |
| `core/angle_hall.h` | Hall decode, 6-entry tables, six-step pattern, and the edge tracker (the logic of the guarded AM13 image's `hall_pll()` at pole 0, in integers) |
| `core/guard.h`, `core/guard.c` | Limit writes clamped to firmware ceilings, start refusals, trips (current, non-finite, cap, heartbeat, background stall, clock, driver, bus), reaction table |
| `core/cost.h` | `FRL_COST_BEGIN/END`: cycle stamps with a board-supplied counter, mean kept x256 and a maximum, no division in the ISR |
| `tests/host_test.c`, `tests/host_test.sh` | Host test in five number-format builds, a negative control and the compile-time refusals |
| `tests/cost/` | The flash and RAM cost matrix: `build.sh`, `summarize.py`, `check_symbols.py`, `rows.py`, `sizes.csv`, `RESULTS.md` |

## Ten-minute path

1. Run the host test (needs only `cc`):

   ```bash
   portability/motor/tests/host_test.sh
   ```

   It builds and runs float with libm, float with the table, IQ24 with libm, IQ24 with the table and Q15 with the table. Each run checks the kernel, a current step and a Hall-tracked speed run against a double-precision controller on a minimal test plant, the tracker's scripted cases, the guard and the cost stamps, then prints `OK <build>`. The script ends with `OK`.
2. Read the bounds at the top of `tests/host_test.c`; they were set before the first run.
3. Rebuild the cost matrix (72 object builds, about 30 seconds, needs `arm-none-eabi-gcc`, the TI Arm Clang and the TI C2000 compilers at the paths in `tests/cost/build.sh`):

   ```bash
   portability/motor/tests/cost/build.sh /tmp/frl-cost
   python3 portability/motor/tests/cost/summarize.py /tmp/frl-cost/sizes.csv
   ```
4. Use the core in a build: define `FRL_NUM`, `FRL_TIER` and any flag overrides, put `core/` on the include path, include `kernel.h`, `angle_hall.h`, `guard.h`, and compile `core/guard.c`. `tests/cost/isr_body.c` is a complete example of an ISR for each tier.

## Rules the code follows

- The test plant (R 0.75 ohm, L 1.05 mH, flux 4.74e-3 Wb, 4 pole pairs, J 2.4e-6 kg m2; bench values) only lets two number formats be compared with each other. Engineering predictions come from the trusted solvers, not from this folder.
- A feature that is off removes its code and data (`check_symbols.py` checks the object files).
- Sizes are object-file sizes: no startup, HAL, stack or compiler-library helpers (64-bit multiply, division, `sinf`). Cycles are not measured; `tests/cost/RESULTS.md` says why.
- Known behaviors kept from the proven AM13 image, not corrected here: after a reversal the first edge's delay compensation uses the old speed (the second back edge reads about 9 percent high and settles in two more edges), and the second forward edge after standstill is uncompensated (the third reads about 8 percent high, then settles). Both are in `host_test.c`.
