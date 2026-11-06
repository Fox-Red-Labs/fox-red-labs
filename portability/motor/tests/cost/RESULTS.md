# Motor core cost matrix: flash and RAM per feature (2026-10-19)

The guard costs 0.77 to 1.07 kB of flash and 72 B of RAM on every core class; the whole FOC step is 880 B with libm and 2.3 kB with the table. Sizes are object files, not linked images, and cycles are not measured. Predictions were written before the first full build. Raw sizes: `sizes.csv` (72 object builds; flags and compiler in each row). Terms: [docs/vocab.md](../../../../docs/vocab.md). Rows in `data/results/{mspm0g3519,mspm33c321a,f2800137}.csv` (tests `motor_core_flash`, `motor_core_ram`, 144 rows). The core classes map to boards as Cortex-M0+ to mspm0g3519, Cortex-M33 to mspm33c321a, C28x to f2800137; the sizes come from compiling objects, not from a board, and each row says so.

## Method as run

Each variant (A shell, B shell + guard, C six-step Hall, D Hall FOC with the sector angle, E D + edge tracker, F E + guard) was compiled with `-O2` for the core class and number format, objects only. Flash is text with read-only data (the 1.0 kB sine table and the Hall tables are in it), RAM is data + bss. The ISR body reads and writes volatile words in place of registers. Tools: arm-none-eabi-gcc 14.3.1 (`size`), TI Arm Clang 5.1.1.LTS (`tiarmsize --format=berkeley`), TI C2000 CGT 25.11.1.LTS (`ofd2000`; RAM from symbol sizes because the C28x `.bss` section is block-aligned and its size carried 56 B of padding).

Checks that passed: the whole matrix was built twice into separate folders and the sizes were identical; `check_symbols.py` found the feature data in the builds that have the feature and not in those that do not (720 of 720 checks, including the libm backend carrying no sine table); `host_test.sh` prints `OK` for five number-format builds, the negative control fails as it must, and six impossible flag combinations are refused at compile time.

## Measured deltas (flash bytes / RAM bytes)

| Feature | m0p iq24/table gcc | m0p iq24/table clang | m0p q15/table gcc | m0p q15/table clang | m33 float/table gcc | m33 float/table clang | m33 float/libm gcc | m33 float/libm clang | m33 iq24/table gcc | m33 iq24/table clang | f28 float/table c2000 | f28 iq24/table c2000 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Shell (A) | 156 / 76 | 160 / 76 | 152 / 76 | 156 / 76 | 212 / 76 | 216 / 76 | 212 / 76 | 216 / 76 | 176 / 76 | 180 / 76 | 208 / 76 | 160 / 76 |
| Guard (B-A) | 778 / 72 | 774 / 72 | 774 / 72 | 770 / 72 | 938 / 72 | 868 / 72 | 938 / 72 | 868 / 72 | 770 / 72 | 786 / 72 | 1068 / 72 | 914 / 72 |
| Six-step Hall (C-A) | 120 / 0 | 96 / 0 | 120 / 0 | 96 / 0 | 120 / 0 | 108 / 0 | 120 / 0 | 108 / 0 | 120 / 0 | 108 / 0 | 180 / 0 | 180 / 0 |
| FOC, sector angle (D-A) | 2020 / 40 | 1968 / 40 | 2044 / 40 | 1980 / 40 | 2268 / 40 | 2134 / 40 | 880 / 40 | 890 / 40 | 1828 / 40 | 1840 / 40 | 2036 / 40 | 2044 / 40 |
| Edge tracker (E-D) | 704 / 48 | 764 / 48 | 668 / 48 | 748 / 48 | 696 / 48 | 590 / 48 | 684 / 48 | 598 / 48 | 692 / 48 | 596 / 48 | 910 / 56 | 940 / 56 |
| Guard on T2 (F-E) | 766 / 72 | 778 / 72 | 802 / 72 | 802 / 72 | 930 / 72 | 864 / 72 | 950 / 72 | 872 / 72 | 810 / 72 | 790 / 72 | 1054 / 72 | 910 / 72 |

Cells are flash bytes / RAM bytes. Flash = text with read-only data; RAM = data + bss; objects, not linked.

Read across: the feature costs are close between gcc and TI Arm Clang (within 20 percent), and the number format barely moves them on Cortex-M0+ (IQ24 and Q15 use the same 64-bit multiply). Float with the libm backend is 880 B for the whole FOC step against 2.3 kB with the table: the table and its interpolation are 1.4 kB, and the `sinf` and `cosf` the libm build calls are not in the object (they come from the library at link time). The guard costs 0.77 to 1.07 kB of flash and 72 B of RAM on every core class.

## Predicted against measured

Every measured delta is inside its predicted range except three cells (four with the Q15 build counted separately):

| Cell | Predicted | Measured | Cause |
|---|---|---|---|
| Six-step Hall flash, TI Arm Clang, Cortex-M0+, IQ24 and Q15 builds | 120 to 450 B | 96 B | Low end of the range was too high (gcc gave 120 B). Not investigated further |
| Edge tracker flash, C28x float | 350 to 800 B | 910 B | Above the range. Likely the 32-bit angle arithmetic and the 64-bit product on a 16-bit word machine; not checked in the listing |
| Edge tracker flash, C28x IQ24 | 350 to 800 B | 940 B | Same |

Everything else landed in range, including RAM: shell 76 B, guard 72 B, FOC 40 B, tracker 48 B (56 B on C28x), six-step 0 B.

Two reading notes. The predicted ranges for guard, FOC and tracker are wide enough that a hit says only that the order of magnitude is right; the narrow ones are RAM, which follow the structure sizes. The planning figures of 2 to 4 kB for the FOC kernel and loop with a 0.5 kB table, and 0.5 to 1 kB for a Hall tracker, bracket these builds: the FOC step with its table is 1.8 to 2.3 kB here, the tracker 0.59 to 0.76 kB on Arm.

## What the predictions covered

One trial compile of variant F (IQ24, table, Cortex-M0+, gcc and TI Arm Clang) was run to check the build commands before the predictions were written; it showed about 3.1 kB of text and 236 B of bss for the ISR object and 546 B for `guard.o`. The predictions for that corner were therefore not blind. No other size had been seen.

## Not done

- Cycles: the cost matrix needs the mean and worst ISR cycles per feature on a board. Only sizes were measured. The ISR body (`isr_body.c`) is the part a board harness would reuse: map `HW_ADC`, `HW_PWM`, `HW_HALL`, `HW_DRV` and `HW_CYC` to a board's registers and a counter (DWT on M33, a free-running timer on M0+, the CPU timer on C28x), feed synthetic samples, read `S.cost`. That needs a board build per SDK and a free debug probe. The cycle predictions stand unmeasured.
- Linked images, so helper routines (64-bit multiply and division on Cortex-M0+ and C28x, `sinf` and `cosf`) are missing from every size. A linked build per board is the next size step.
- Maximum PWM-synchronous rate (needs cycles and a rate sweep with this ISR as the body).
- The AM13 observer, identification, field weakening, MTPA, flying start, capture, table and NPU hook: not written; `frl_cfg.h` accepts and checks their flags.
