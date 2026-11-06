<!-- experiment
id: E17
board: LP-MSPM0C1104
date: 2026-10-08
status: line found
reached: flash
line: The standard CoreMark does not link for the LP-MSPM0C1104; reduced 400 and 200 byte data sets link and flash, then hard fault with the stack pointer below RAM.
stop: ram
-->
# How much of CoreMark runs on a TI LP-MSPM0C1104 with 1 kB of RAM?

**Short answer.** None of it could be made to run. The standard 2 kB data set does not link (a 2.5 kB stack in 1 kB of RAM, and 20.2 kB of code in 16 kB of flash at -O3). Reduced 400 and 200 byte sets link at -Os, flash, then take a hard fault inside `core_bench_list`. They would not be valid CoreMark scores anyway.

Terms: [vocab](../vocab.md) (CoreMark, stack, hard fault). Errors and fixes: [LP-MSPM0C1104 FAQ](../../mspm0c1104/FAQ.md). 1 kB = 1,024 bytes.

**Question.** Does the reduced-data CoreMark give a per-MHz figure that can be compared with the LP-MSPM0L1306?

**Vendor position.** EEMBC's rules require the 2 kB data set for a reported CoreMark score.

**What was tried.** EEMBC CoreMark unchanged (`third-party/coremark/`), this repo's MSPM0 CoreMark port, TI Arm Clang 5.1.1 -Os, 24 MHz, with `TOTAL_DATA_SIZE` 400 and 200, 10 and 1k iterations, stacks of 720 and 920 B, SRAM painted with `0xA5A5A5A5`, read back with OpenOCD, 2026-10-08.

**How far it got.** Compile, link and flash passed. The run stops in a hard fault (`pc 0x0000154a`, `msp 0x1FFFFF30`) at the first instruction of `core_bench_list` that moves the stack pointer. The painted RAM shows the whole reserved stack used. A smaller data set did not change it.

**The line.** The stack use is more than 920 B before the first benchmark loop, and does not depend on the data-set size. The cause was not found.

**Ask.** If CoreMark has run on a 1 kB chip, say how.
