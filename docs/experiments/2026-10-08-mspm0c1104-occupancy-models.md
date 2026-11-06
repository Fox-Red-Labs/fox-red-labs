<!-- experiment
id: E15
board: LP-MSPM0C1104
date: 2026-10-08
status: line found
reached: link
line: None of the four room-occupancy models links for the LP-MSPM0C1104; the smallest needs 1.2 kB of RAM for its buffers against 1 kB.
stop: ram
-->
# Can any room-occupancy model fit in the 1 kB of RAM on a TI LP-MSPM0C1104?

**Short answer.** Not as built. The smallest of this repo's four models (636 parameters) needs 1.2 kB of RAM for its two activation buffers and an input copy, 176 B more than the chip has even with no stack. The 1.7k-parameter model needs the same 1.2 kB; the two larger ones need 2.2 kB and 4.2 kB.

Terms: [vocab](../vocab.md) (stack, linker). Errors and fixes: [LP-MSPM0C1104 FAQ](../../mspm0c1104/FAQ.md). 1 kB = 1,024 bytes.

**Question.** Does a model that decides whether a room is occupied fit in 1 kB of RAM?

**Vendor position.** TI's Edge AI examples for MSPM0 target boards with 16 kB or more of RAM; the LP-MSPM0C1104 SDK folder has none.

**What was tried.** Four models in plain C at -Os and -O3, TI Arm Clang 5.1.1, 24 MHz, 512 B stack, link only (no run), `mspm0c1104/prebuild.sh`, 2026-10-08.

**How far it got.** Configure and compile passed. Link failed for all eight builds: 636 and 1.7k parameters needed 1.2 kB of `.bss` (688 B over with a 512 B stack); 8.5k parameters 2.2 kB and 18.8 kB to 19.2 kB of read-only data; 33k parameters 4.2 kB of RAM and 33.6 kB of weights, more than the 16 kB of flash.

**The line.** 1.2 kB of buffers against 1 kB of RAM.

**Ask.** If you have a model under 500 B of working memory for room occupancy, say so.
