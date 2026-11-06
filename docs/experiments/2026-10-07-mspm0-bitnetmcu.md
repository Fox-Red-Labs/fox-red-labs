<!-- experiment
id: E12
board: LP-MSPM0G3507
date: 2026-10-07
status: works
reached: verified
video: media/bitnetmcu-touch-demo.mp4
poster: media/bitnetmcu-touch-demo-poster.jpg
caption: The touch demo on the LP-MSPM0G3507, filmed 2026-10-06 (40 s, no sound). Every single digit whose answer shows on screen, 3, 4, 7, 8, 9, 3 and 1, is read right in 33 ms; "10" and "12" come out as 6 and 4, because the model reads one digit at a time.
line: All four BitNetMCU handwriting models read 10 of 10 test digits on the LP-MSPM0G3507; at 32 MHz each took 3.5 to 19% fewer cycles than the author's CH32V002 figures, at 80 MHz (two wait states) the 99.55% model took 0.5% more; the LP-MSPM0L1306 runs it in its 4 kB of RAM (2.65M cycles); a touchscreen demo reads finger-drawn digits.
stop: none
-->
# Can the TI LP-MSPM0G3507 LaunchPad development kit run BitNetMCU's handwriting recognizer as well as the WCH chip it was written for?

**Short answer.** Yes. On the LP-MSPM0G3507 all four BitNetMCU models read all 10 of its test digits right, the same answers as on a Mac, and at 32 MHz each took fewer clock cycles than the author's published count on WCH's CH32V002: 2.64M against 2.74M for the 99.55% model, and 3.5 to 19% fewer across the four models. At 80 MHz, with two flash wait states like the author's chip, the 99.55% model took 0.5% more (2.75M). The LP-MSPM0L1306, with 4 kB of RAM, runs the same model too: 10 of 10 in 2.65M cycles. At 80 MHz that model answers in 34 ms, and a touchscreen demo reads digits drawn with a finger.

Terms: [vocab](../vocab.md) (CNN, MNIST, fully connected, inference, flash wait state, SysTick). Errors and fixes: [LP-MSPM0G3507 FAQ](../../mspm0g3507/FAQ.md), [LP-MSPM0L1306 FAQ](../../mspm0l1306/FAQ.md). 1 kB = 1,024 bytes. Full write-up and commands: [`ports/bitnetmcu/README.md`](../../ports/bitnetmcu/README.md).

**Question.** BitNetMCU (github.com/cpldcpu/BitNetMCU, 344 stars, GPL-3.0) is an open neural net for 10-cent microcontrollers, built for WCH's RISC-V chips. Does it run on TI's MSPM0, unchanged, and how does the speed compare?

**Vendor position.** The author publishes cycles per answer measured on a CH32V002 at 48 MHz with two flash wait states and the inner loops running from SRAM (`docs/documentation_cnn.md` in his repo, read 2026-10-06). TI's MSPM0 SDK 2.11.00.07 has no handwriting example; its Edge AI examples target the larger MSPM0 chips.

**What was tried.** BitNetMCU commit `0715bfc`, copied unchanged into `third-party/bitnetmcu/`, built with TI Arm Clang 5.1.1.LTS at -O3 on the LP-MSPM0G3507's own clock setup (32 and 80 MHz) and timed per digit with the SysTick cycle counter. The only change around upstream's code: its test digits are read in as `const`, so they stay in flash instead of taking 2.5 kB of RAM. The expectation, written before the first board run: the same 10 answers as the Mac, and a cycle count close to the author's. The touch demo (`ports/bitnetmcu/touch/`) adds the Waveshare 3.5 inch touch screen: the drawing is scaled and centered the way MNIST digits were made, then classified.

**How far it got.** Measured on the board, every model, every digit:

| Model | Size | Author, CH32V002 (cycles) | LP-MSPM0G3507, 32 MHz | 80 MHz | Right |
|---|--:|--:|--:|--:|---|
| 64-wide CNN, 99.55% | 11.0 kB | 2.74M | 2.64M (82.5 ms) | 2.75M (34.4 ms) | 10 of 10 |
| 16-wide CNN, small, 98.92% | 3.2 kB | 686k | 661k (20.6 ms) | not run | 10 of 10 |
| 12k fully connected, 99.02% | 12.3 kB | 528k | 430k (13.4 ms) | not run | 10 of 10 |
| 1k fully connected, 94.22% | 1.1 kB | 100k | 89k (2.8 ms) | not run | 10 of 10 |

Accuracy percentages are the author's, on the full MNIST test set; the 10 digits that ship with the code were checked. The touch demo's self-test drew each test digit into its box and read it back, 10 of 10, and digits drawn on it with a finger were read (how many were right was not counted). LP-MSPM0L1306, run 2026-10-07: the 64-wide model reads 10 of 10 on the chip with 4 kB of RAM, in 2.65M cycles (82.8 ms at 32 MHz), 3.2% fewer than the author's figure, with 2.2 kB of stack.

**The line.** No line reached for digits: every model fits and runs, with room to spare on the G3507 (deepest stack 2.2 kB of 32 kB). Two findings: running the inner loops from SRAM, as the author does on WCH's chip, made the G3507 slower (2.95M cycles at 32 MHz), **Unknown (suspected: the Cortex-M0+ fetches instructions and data over one bus).** And the comparison is chip plus compiler: the author's numbers come from GCC for RISC-V, this repo's from TI Arm Clang, and his code was not rerun on his chip.

- Rerun the author's code on a CH32V002 board ourselves, so both numbers come from this bench.
- Letters (the author's letters model) and two-digit numbers on the touch demo.
- The 1k model on the 1 kB-RAM LP-MSPM0C1104.

**Ask.** If you have run BitNetMCU on another chip, post the cycle counts and the compiler, and they will be added here.
