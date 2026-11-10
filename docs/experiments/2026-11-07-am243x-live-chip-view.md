<!-- experiment
id: E318
board: LP-AM243
date: 2026-11-07
status: works
reached: verified
line: on the LP-AM243's R5F core 0 (R5FSS0-0) the debug port cannot see sleep, so CPU load needs a few lines in the program; with them a plain program read 10.1, 50.0 and 90.0% awake against a true 10, 50 and 90, and a FreeRTOS program with five tasks read 29.99% for a task designed at 30%
stop: none
page: none
-->
# Can you watch a running LP-AM243 from a debug port without halting it, and how much does it need from the program?

Terms: [docs/vocab.md](../vocab.md), [LP-AM243 FAQ](../../am243x/FAQ.md). The **debug port** is the set of wires a debugger uses to look inside the chip; here the board's XDS110 probe reaches it over USB. **TCM** is a small RAM wired straight to one core. **OCRAM** is the on-chip RAM the cores share. **PSC** is the chip's power and sleep controller. **WFI** is the instruction that puts a core to sleep until an interrupt. The earlier MSPM0 half of this work is [E317](2026-11-06-mspm0l1306-live-chip-view.md).

**Short answer.** Partly. Power, pins, pad settings and timer counts read from the running chip with no help. CPU load does not: it needs the program's help. The Cortex-R5 cores of the AM2434 have no program counter sample register, and the chip's sleep status bit cannot be read from the debug port, so the program has to count its own time asleep (a few lines, `frl_wfi()`) or run a timer interrupt that stamps a cycle counter. With that, a plain program read 10.1, 50.0 and 90.0% awake against a true 10, 50 and 90, and in a FreeRTOS program the crunch task read 29.99% for a designed 30%; the other tasks' loads were not checked.

| Live value on the LP-AM243 | Known value | Read |
|---|---|---|
| Core 0 load, plain program, 10% | 10% | 10.1% |
| Core 0 load, plain program, 50% | 50% | 50.0% |
| Core 0 load, plain program, 90% | 90% | 90.0% |
| Crunch task load, FreeRTOS | 30% designed | 29.99% |
| Core 0 load, cycle counter | 30% crunch plus interrupts; the OS total reads 30.05% | 30.2% |
| LED pin changes | 4 a second | 4.000 |
| Timer 6 interrupts | 100 Hz | 99.988 |
| I2C reads | 10 a second | 10.001 |
| UART baud, estimated from busy bits | 115,385 set (48 MHz / 16 / 26) | 112,701 |
| UART baud, Mac decoding the console | 115,200 | 188 lines decoded, none malformed |
| Pad mux and interrupt lines | what SysConfig sets | 9 of 9 |

**Question.** The MSPM0 live chip view works with no help from the program. Does the same idea work on the AM2434's R5F cores, and what does the program have to provide?

**Expectation, written first.** Not written before the run. The run records hold no expectation for this test, so none is quoted here.

**Vendor position.** No vendor figure for reading load or peripheral state through the debug port was found. The chip's technical reference manual lists a WFI status bit for each core (`CLSTR0_CORE0_PMSTAT`, 0x45A00130); it is the first thing tried below.

**What we tried.** Reads go through the debug port's system view, never connecting to a core, because connecting to a core halts it. Each reading is a register read of the chip as the program runs.

- **Sleep seen by the debug port.** The Cortex-R5 has no program counter sample register: `DBGDIDR` reads 0x77040013 with bit 13 clear, and `DBGDEVID` reads 0. The `PMSTAT` register's whole security block reads 0 from the debug port.
- **Idle beacon.** frl_load.h replaces a bare WFI with `frl_wfi()`, which adds the time asleep to a counter in the core's TCMB. TCM is not cached, so the debug port reads it fresh with the core running. The R5F test program ran core 0 at 10, 50 and 90 percent.
- **A busy program.** The busy program is FreeRTOS on R5FSS0-0 built from SDK examples and run from RAM. It has five tasks and one interrupt beside the kernel's 1 kHz tick:
  - An LED task toggles GPIO0 pin 22 every 250 ms.
  - An I2C task reads 16 bytes of the board EEPROM every 100 ms. It only reads.
  - A console task prints one status line a second on UART0.
  - A crunch task spins a checksum loop for 30 ms of every 100 ms.
  - A stats task copies the SDK's per-task load into TCM every 500 ms.
  - A second hardware timer interrupts at 100 Hz. It is an interrupt, not a sixth task.
- **The picture.** The chip map comes from the SDK headers and SysConfig's device data (`am243x/tools/chip_map.py`). With the live hash, `chip-live.html#busy,,live`, the page reads `live.json` from `am243x/tools/live_probe.js` and draws blocks colored by kind and filled when powered, the pad mux of the pins in use, the pin as the pad sees it, the timer counters, the interrupt lines that are on, the task list with each task's load, and core 0 load as a bar on the core.

**How far it got.** Verified on one board and one core, R5FSS0-0, with the checks in the table. All rows are in `data/results/am243x.csv` (tests starting `live_`), raw logs in `am243x/bench/logs/busy-*`.

What the debug port reads, and what needs help from the program:

- **Power.** The PSC module status (0x400800 plus four times the LPSC number) reads fine. Blocks with no switch count as always on, and a thick outline marks the ones whose own enable bit the program set. A module that is powered off reads 0 at its revision register, and reading it neither hangs nor errors. Six power-switch states agreed with their modules answering.
- **Load.** Needs the program. The idle beacon from `frl_load.h` does not work under an RTOS: a task that wakes from WFI on a tick can be preempted before it reads the clock, so the time other tasks ran is booked as sleep and the load reads near zero. The fix is `frl_pmu_load.h`: the Cortex-R5 cycle counter stops while the core sleeps, so a timer interrupt stamps the counter and the time together, and the awake share is the counter against the clock.
- **Interrupts.** The interrupt controller is core-local, so it is not visible in the system view. The program copies its enable mask into TCM.
- **UART.** The divisor latch hides behind a write to the line control register, which a probe should not make. The transmitter-busy bit gives a coarse baud check instead: 112,701 estimated against 115,385 set. The estimate scatters by several percent between runs. The Mac decoded 188 console lines cleanly at 115,200, so the estimate supports the rate to about 5%, and the clean decode is the stronger check.
- **Averaging.** A one-second window averages ten crunch periods, so core load stays within about a point and a half of its mean. A shorter window swings by several points.

**The line.** The stop is on the chip, not the tool: the R5F's debug block has no program counter sampler and the sleep status register reads 0 from the debug port (a firewall is suspected, not confirmed). The consequences:

- Load needs the program's help (a counter in TCM or a timer interrupt). A program you did not build, and cannot change, shows power and pins but no load.
- If you read a variable in OCRAM and it never changes, the R5F is holding it in its data cache. Put the variable in TCM, or read it from the core's own view while it is halted.
- The interrupt enable mask needs the program's help too.
- If a core is connected to by a debugger, it halts. The live view avoids that by using the system view only.

**Next moves.** Read the other three R5F cores and the M4F. Try the cycle-counter method on Zephyr. Find a firewall setting that exposes `PMSTAT` to the debug port.

**Limits.** One board, one core, one program. The bare-metal load rows are from one run each; the busy-program rows come from a 113-sample run. No power was measured, and the debug connection may change what the chip does in deep sleep.
