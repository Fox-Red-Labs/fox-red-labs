<!-- experiment
id: E186
board: FRDM-MCXA153
date: 2026-10-11
status: works
reached: verified
line: All four BitNetMCU models read 10 of 10 digits right on the FRDM-MCXA153 with the inference code unchanged; the 64-wide CNN takes 1.30M cycles, about half the LP-MSPM0G3507's count at 32 MHz.
stop: none
-->
# Can the NXP FRDM-MCXA153 run BitNetMCU's handwriting recognizer, and how many cycles does it need?

**Short answer.** Yes. All four BitNetMCU models classify all 10 of the project's test digits correctly on the FRDM-MCXA153, with BitNetMCU's inference code and models unchanged. The 64-wide CNN takes 1.30M cycles per digit (13.6 ms at 96 MHz), about half of the 2.64M the LP-MSPM0G3507 needed at 32 MHz and the 2.74M the author published for WCH's CH32V002.

Terms: [vocab](../vocab.md). 1 kB = 1,024 bytes. **BitNetMCU** is an open-source project (GPL-3.0) for neural nets small enough for 10-cent microcontrollers; the test input is 10 handwritten 16 x 16 digits.

**Question.** Does the same port that ran on the TI MSPM0 run on this Cortex-M33, and is it faster per cycle?

**Vendor position.** Not a vendor feature. The port in `ports/bitnetmcu/` ran on the LP-MSPM0G3507 (all four models, 10 of 10); BitNetMCU's own timings are for the CH32V002.

**What was tried.** The same `ports/bitnetmcu/bitnet_run.c` (BitNetMCU's `BitMnistInference()` unchanged) and the four model headers from `third-party/bitnetmcu`, built with GCC 12.2.0 at -O3 on NXP's board setup, code in flash with the cache on, 96 MHz; cycles from SysTick, mean of the 10 digits.

**How far it got.** Verified: 10 of 10 right on every model.

| Model | Cycles per digit (mean) | Time at 96 MHz | LP-MSPM0G3507, 32 MHz | Author, CH32V002 |
|---|--:|--:|--:|--:|
| 64-wide CNN | 1,301,510 | 13.6 ms | 2,640,383 | 2,736,250 |
| 16-wide CNN, small | 332,079 | 3.5 ms | 660,687 | 686,490 |
| 12k fully connected | 244,755 | 2.5 ms | 429,789 | 528,377 |
| 1k fully connected | 43,989 | 0.46 ms | 89,336 | 99,783 |

**The line.** No limit reached. Read these as chip plus compiler, not core alone: GCC for Arm here, TI Arm Clang on the G3507, GCC for RISC-V on the CH32V002, and this chip's cache against the other two chips' flash wait states.

**Ask.** None.
