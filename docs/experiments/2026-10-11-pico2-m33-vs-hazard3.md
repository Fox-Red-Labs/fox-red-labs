<!-- experiment
id: E60
board: Raspberry Pi Pico 2 H
date: 2026-10-11
status: works
reached: verified
line: On the Pico 2 at 150 MHz Hazard3 beats the Cortex-M33 on integer CoreMark (495.2 against 437.6 at -O3) and loses 15 to 1 on a floating-point control step (4.5k against 308 cycles).
stop: none
-->
# How much faster is the Cortex-M33 than the Hazard3 RISC-V core on the same Pico 2 chip?

**Short answer.** Hazard3 is ahead on integer code and far behind where the M33's floating-point unit matters: CoreMark 495.2 against 437.6 per second at GCC -O3, a control step of 4.5k cycles against 308, the 30k-parameter occupancy model 29.0 ms against 27.0 ms.

Terms: [vocab](../vocab.md). 1 kB = 1,024 bytes. Numbers: `data/results/pico2.csv`.

**Question.** How much faster is the Cortex-M33 than the Hazard3 RISC-V core on the same Pico 2 chip?

**Vendor position.** Raspberry Pi Pico 2 datasheet release 5 (read 2026-10-07): the RP2350 boots two Arm Cortex-M33 or two Hazard3 RISC-V cores, chosen at build time.

**What was tried.** The same C files built twice (`pico2/bench`, M33 with GCC 12.2 and 14.3, Hazard3 with GCC 16.1.0), three runs each, results over USB serial. The compilers differ, so part of the gap is the compiler.

**How far it got.** Verified: every CoreMark run passed its own check; the clock (150 MHz) and timer (one tick per cycle) were read back by each image; the run time the chip reports matches the Mac's wall clock.

**The line.** No limit reached. The M33 figure is lower than the guess made beforehand (450 to 630), which came from other boards, not from Raspberry Pi. With loop unrolling the gap widens (585.8 against 511.6).

**Ask.** Has anyone measured both cores with one compiler family on the same source?
