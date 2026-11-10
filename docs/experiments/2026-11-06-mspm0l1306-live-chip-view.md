<!-- experiment
id: E317
board: LP-MSPM0L1306
date: 2026-11-06
status: works
reached: verified
line: the page's debug code reads a running LP-MSPM0L1306 through the board's XDS110 without stopping it, and the awake share matched a program of known load to within 0.1 points at 10, 50 and 90 percent; the view is mapped for four MSPM0 chips (L1306, G3507, G3519, G5187), with load checked only on the L1306
stop: none
page: none
-->
# Can you watch a running LP-MSPM0L1306 through its debug port without changing its program?

Terms: [docs/vocab.md](../vocab.md), [LP-MSPM0L1306 FAQ](../../mspm0l1306/FAQ.md). The **debug port** is the set of wires a debugger uses to look inside the chip; on this board the on-board XDS110 probe reaches it over USB. **WebHID** is the Chrome feature that lets a web page talk to that probe. **WFI** is the instruction that puts the CPU to sleep until an interrupt. **PWM** is a square wave with a set on-time. Every register named below is defined in the vocab page.

**Short answer.** Yes. The page's debug code reads the L1306 through the XDS110 about 500 times a second when run from Node, which is where the numbers below were measured. The same code ran in Chrome over WebHID on the L1306 and the G3519; the browser's read rate was not recorded. The program is not halted, not reset and not instrumented: load, the running code, powered blocks, pin setup and slow pin changes all come from registers the debug port can read while the CPU runs. On the L1306 the awake share was within 0.1 points of the program's own load.

| Reading on the L1306 | Program's true value | Read through the debug port |
|---|---|---|
| CPU awake, 10% program | 10% | 10.1% |
| CPU awake, 50% program | 50% | 50.0% |
| CPU awake, 90% program | 90% | 90.0% |
| PA13 changes a second | 10 | 10.05 |
| PA26 changes a second | 2 | 2.00 |
| UART0 rate, TI's out-of-box demo | 115,200 baud set | 115,212 baud from its divider |

**Question.** Can a browser show what a running chip is doing, with no change to the program and no halt?

**Expectation, written first.** Not written before the run. The run records hold no expectation for this test, so none is quoted here.

**Vendor position.** A search on 2026-11-06 found no tool, from TI or anyone else, that draws a modern microcontroller's block diagram colored from the running chip (a search result, not proof that none exists). CCStudio's register and memory views show the same registers as text. No vendor figure exists to compare the load or pin readings with.

**What we tried.** Every number comes from registers the debug port can read while the CPU runs:

| What you see | Register |
|---|---|
| CPU awake or asleep | `DHCSR`, the sleep bit (Arm's debug halting control and status register) |
| Where the CPU is | `DWT_PCSR`, a sample of the program counter |
| Interrupt running | `ICSR`, the number of the active interrupt handler |
| Interrupts turned on | `NVIC_ISER`, one bit per interrupt |
| Blocks with power | `PWREN`, the power enable register of each peripheral |
| What each pin does | `PINCM`, the pin control register of each pin, and `DOE`, which pins of the port are outputs |
| Pin levels | `DOUT`, the level the program drives, and `DIN`, the level on the pin |
| RAM that changes | Each RAM word, compared between readings |

The test program, known_load.c, spins for a set share of each 100 ms and sleeps in WFI for the rest. It steps through 10, 50 and 90 percent, 10 s each. The other programs were read as found: TI's out-of-box demo, TI's PWM example, a CoreMark build and the thermometer image. Saved readings: readings/; the rows are in `data/results/mspm0l1306.csv` (tests starting `live_`).

**How far it got.** Verified on the L1306: six programs read with the page's own debug code (`site/flash/mspm0-dap.js`) run from Node.

- **Program counter.** While the CPU slept, all but 9 of about 5.4k samples (99.8%) landed on one of the program's 12 WFI instructions. The compiler made 12 copies of that loop.
- **CoreMark.** The CoreMark build shows its benchmark functions, and the awake share drops to 0% when the benchmark returns (a series of 100, 100, 100, 100, 100, 50.7, 0).
- **TI's PWM example.** It sleeps at 0% awake with TIMG0 running on PA12 and PA13. The period computes to 16.0 ms from the timer registers: load 1999, prescale 256, 32 MHz clock.
- **Thermometer image.** The ADC0 block reads powered, and no other peripheral does.
- **Read rate, sets of the fast registers per second.** L1306 about 498, G3519 328, G5187 478, each as counted by the viewer itself.
- **Other chips.** The view carries maps for four MSPM0 chips (L1306, G3507, G3519, G5187). Reads were also seen on the G3519, where UART0 computed to about 115,108 baud, and on the G5187. Load was checked against a known program only on the L1306; the G3507 has not been read live.

**The line.** The L1306 readings hold for CPU load, the running function, powered blocks, pin setup and slow pin changes. These are the limits:

- A pin that changes more than about 100 times a second is marked fast, not counted.
- A slow, uneven pattern reads low when the count is short: a blink with a true 2.96 changes a second read 2.7 over 20 seconds.
- A PWM pin driven by a timer does not show in `DOUT`. The view reads its period from the timer instead.
- The debug connection can keep the chip out of its deepest sleep, so the view is not a power measurement.
- If a program enters STANDBY, the debug port stops answering and the view says so. TI's `timx_timer_mode_periodic_standby` did this: every read answered "ack 4", and a reset pulse with a halt did not catch it. The log of that lockup was not saved. The recovery that worked, recorded in `data/recovery.json`, was TI's DSLite flash from UniFlash 9.6 with a non-sleeping image and the board's ccxml; the first try worked and needed no hands.

**Next moves.** Run the known-load program on the G5187 and G3519 and compare their awake share to the program. Read the G3507 live.

**Limits.** One L1306 board, one run of each program. Load was compared with a known program only on the L1306. Pin counts above about 100 a second are not given. No power was measured. The AM243 half of this work, where load needs a change to the program, is on its own page: [E318](2026-11-07-am243x-live-chip-view.md).
