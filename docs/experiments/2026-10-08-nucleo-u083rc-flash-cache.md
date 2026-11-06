<!-- experiment
id: E129
board: NUCLEO-U083RC
date: 2026-10-08
status: works
reached: verified
line: On the NUCLEO-U083RC, CoreMark per MHz with one flash wait state at 48 MHz (2.03) is within 1.3 percent of the reset clock with none (2.05); suspected cause, not tested: the instruction cache.
stop: none
-->
# How much does the flash wait state cost the ST NUCLEO-U083RC (STM32U083) at 48 MHz?

**Short answer.** Almost nothing. CoreMark gives 2.03 per MHz at 48 MHz with one flash wait state and 2.05 per MHz at the 4 MHz reset clock with none, a 1.3 percent difference. The likely reason is the chip's 1 kB instruction cache; it was not turned off to check. Same compiler, same files, same flags.

Terms: [vocab](../vocab.md). 1 kB = 1,024 bytes. A wait state is a clock cycle the core waits for flash; CoreMark per MHz is iterations per second divided by the clock.

**Question.** Does running from flash at full speed cost this chip as much as it costs other M0+ parts?

**Vendor position.** ST data sheet DS14463 Rev 2: "1-Kbyte instruction cache" with "0-wait-state execution from flash" (read 2026-10-07). ST's `GPIO_IOToggle` example sets `FLASH_LATENCY_1` at 48 MHz.

**What was tried.** CoreMark (unmodified EEMBC files), GCC 12.2 `-O3 -funroll-loops`, bare metal, code in flash. Once at 48 MHz with ST's clock setup copied from `GPIO_IOToggle` (MSI range 11, one wait state), 3k iterations; once at the reset clock (MSI 4 MHz, no wait state), 1k iterations. SysTick counts CPU cycles; three runs each.

**How far it got.** Verified: every run passed CoreMark's own check, and the three runs at each clock gave identical tick counts.

| Clock | Wait states | Iterations per second | Per MHz |
|---|--:|--:|--:|
| 48 MHz | 1 | 97.35 | 2.028 |
| 4 MHz | 0 | 8.216 | 2.054 |

**The line.** No limit reached. For comparison on this bench, the same GCC 12.2 files on the LP-MSPM0G3507 gave 1.82 per MHz at 80 MHz with two wait states.

**Ask.** Has anyone measured the U0's cache hit rate on a larger program than CoreMark?
