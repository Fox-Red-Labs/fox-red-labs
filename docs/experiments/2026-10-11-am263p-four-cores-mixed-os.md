<!-- experiment
id: E179
board: LP-AM263P
date: 2026-10-11
status: works
reached: verified
line: All four Cortex-R5F cores of the LP-AM263P ran CoreMark at once at 1,462 to 1,464 per second each (5,854 together), within 0.1% of each core alone. A message between cores takes 3.36 to 3.87 µs round trip with TI's IPC (FreeRTOS or ThreadX on core 0, bare metal on the others). Zephyr 4.4.2 boots on the chip with out-of-tree board files from this repo (no upstream support) and runs on core 1 beside TI's FreeRTOS on core 0, sharing memory; it has no timer driver.
stop: none
-->
# Can the LP-AM263P run four cores at once, with different operating systems, including Zephyr?

**Short answer.** Yes, from a Mac, loaded over the USB cable. On the LP-AM263P (AM263P4 chip, four Cortex-R5F cores at 400 MHz):

| What | Result |
|---|---|
| CoreMark on all four cores at once | 1,462.1, 1,464.1, 1,463.7 and 1,464.1 per second, 5,854 together; each within 0.1% of its score alone |
| Message round trip between cores (TI's IPC) | 3.49 µs FreeRTOS to one bare-metal core, 3.81 µs each to three at once; ThreadX 3.36 and 3.87 µs |
| Zephyr | Zephyr 4.4.2 boots and prints on core 0 with out-of-tree board files from this repo; upstream Zephyr has no AM263P support |
| Two operating systems at once | TI's FreeRTOS on core 0 printed a line and a counter that Zephyr on core 1 kept writing into shared memory (the counter is bumped after a fixed busy loop, about 36 times a second: proof Zephyr is running, not a timer and not Zephyr's IPC) |

The Zephyr port is a first step: it has no system timer (so no `k_sleep`), no pin-control driver and runs only on cores 0 and 1. The rest of the page says what each test is and what it took.

Terms: [vocab](../vocab.md) (DSLite, development boot, GEL, IPC, ThreadX, out-of-tree, lockstep). Bare metal means no operating system: one loop and interrupt handlers. VIM is the R5F's interrupt controller and RTI is the AM263P's timer block. The four cores are named by cluster and position: core 0 = R5FSS0-0, core 1 = R5FSS0-1, core 2 = R5FSS1-0, core 3 = R5FSS1-1. Code, logs and the Zephyr files: `am263p/bench/multicore/`, `am263p/zephyr/`, `am263p/data/logs/multicore/`, `am263p/data/logs/zephyr/`. Problems and fixes: [LP-AM263P FAQ](../../am263p/FAQ.md). Rows: `data/results/am263p.csv`, tests `coremark` (variants `ti-port-o3-ocsram-*`), `ipc_roundtrip_us` and `zephyr_support`.

**Question.** The AM263P is sold on having four real-time cores that can run separately or in pairs. Do four cores really give four times the work, how fast can they talk, and can each one run a different operating system, including Zephyr?

**Vendor position.** TI's MCU+ SDK 26.01 (read 2026-10-11):

- The IPC Notify page: "low latency IPC between different core's ... extremely fast transfer of message values". It gives no number.
- The SDK ships multicore examples with FreeRTOS or ThreadX on R5FSS0-0 and no RTOS on the other three cores (`examples/drivers/ipc/ipc_notify_echo`, `system_freertos_nortos`, `system_threadx_nortos`).
- The Zephyr 4.4.2 tree on the Mac used here has board support for the R5F cores of TI's AM64x and AM243x parts (`boards/ti/am243x_evm`, contributed by TI) and no AM263P board or SoC.

**What was tried.** LP-AM263P Rev A, MCU+ SDK 26.01.00.08, TI Arm Clang 5.1.1, SysConfig 1.28, Zephyr 4.4.2 with Zephyr's own GCC 12.2 toolchain, Intel Mac. Every image is loaded into RAM over the debug probe with DSLite, one core at a time with a small Python script that loads each core's image and reads its serial output. No switch was moved.

1. **Message round trips.** TI's `ipc_notify_echo`, unchanged except a timer on core 0: core 0 sends a number to each remote core, each sends it back, 1,000,000 times per core. Core 0's FreeRTOS clock could not be used: the echo interrupts starve its timer tick, and in a first run it read 664 µs for an exchange the remote cores' own clocks timed at about 3.9 s (that first log was overwritten by a rerun). In the saved logs the last-started remote core's own clock reads 3.81 to 3.89 s for the exchange; the cores loaded earlier read longer because their clocks were already running while the others loaded. The timer is the CPU's cycle counter, which needs no interrupt.
2. **CoreMark on every core.** TI's CoreMark example inside the same four-core project. All chosen cores wait at a common start line (`IpcNotify_syncAll`), then run. Each core's code and data sit in its own 256 kB of on-chip RAM. Each core writes its score into a shared table, and core 0 prints the table. This repo's copy of TI's `core_main.c` has three changes, each marked with a comment: 1) its driver open calls are removed, because the wrapper opens the drivers once; 2) its driver close calls are removed, because core 0's console must stay open; 3) it calls `frl_cm_export()` to hand its result to the table.
3. **Zephyr.** An out-of-tree SoC (`am263p/zephyr/soc/ti/am26x`) and board (`lp_am263p`) for upstream Zephyr 4.4.2. They reuse Zephyr's existing drivers for TI's interrupt controller (VIM) and for the UART, with the AM263P's addresses from TI's SDK headers. A small start-up hook sets UART0's clock and pins to the values TI's SDK uses.
4. **Two operating systems at once.** TI's FreeRTOS on core 0 and Zephyr on core 1, sharing only memory.

**How far it got.** Everything ran and was checked. TI's IPC example reported "All tests have passed" in every run. Every CoreMark run had CoreMark's correct checksums. Each CoreMark and IPC configuration was loaded three times. The Zephyr results with the current build are one load each: its boot banner in the standalone load, its banner after a TI program with the start-up script, and Zephyr's line and counter printed by core 0 in the mixed load. An earlier start-up-script load printed nothing from Zephyr; that build did not set the UART clock (the clock setting in `soc.c` came later).

**CoreMark per core** (per second, median of 3 loads; core 0 runs FreeRTOS, the others bare metal):

| Core | Alone | All four at once | Change |
|---|---|---|---|
| R5FSS0-0 | 1,463.4 | 1,462.1 | -0.09% |
| R5FSS0-1 | 1,464.1 | 1,464.1 | 0.00% |
| R5FSS1-0 | 1,463.7 | 1,463.7 | 0.00% |
| R5FSS1-1 | 1,463.5 | 1,464.1 | +0.04% (inside run-to-run spread) |
| Sum | | 5,854.0 | 4.00 times one core, on CoreMark |

**Message round trip** (1,000,000 per remote core, median of 3):

| Core 0 runs | One remote core | Three remote cores at once |
|---|---|---|
| FreeRTOS | 3.49 µs | 3.81 µs each |
| ThreadX | 3.36 µs | 3.87 µs each |

**The line.** Nothing stopped, but two results have limits to state.

- CoreMark is the easy case for four cores: its code and data are small (suspected: they stay in each core's own caches, so the cores barely touch shared memory; not measured). A program that streams data through the shared on-chip RAM or the flash would contend more; none was tested.
- The Zephyr port boots and runs code, but without a timer driver nothing that sleeps or times out works. The AM263P's timers (RTI) have no Zephyr driver.

Three things cost time and are worth knowing:

- **Tool or docs:** after a reset of the board, loads to the third core (R5FSS1-0; the fourth was not checked) failed with "Does not match the target endianness". The image was fine. Each core needs its start-up script (GEL) run once; then the loads work.
- **Unknown:** loaded with their start-up scripts, core 2's (R5FSS1-0) console lines never reached core 0's console in these runs, even though it ran (its echoes and results arrived); core 3's lines did. Results pass through shared memory, not the console.
- **Unknown:** Zephyr loaded straight after a TI program, with the connection file that skips the start-up script, printed nothing, and markers it writes at each start-up stage stayed at zero (read over the probe; those reads were not saved): suspected: some state TI's program left behind stops Zephyr's start-up assembly. Which state it is was not found. Loaded on its own, or after a TI program with the start-up script, it boots and prints its banner.

- A control loop on one core while the other three run CoreMark: done since, on the same page (no change to the cycle); a neighbour that streams memory is still open.
- Lockstep, where two cores run the same code as a safety check: done since, on [lockstep and a loaded neighbour](2026-10-11-am263p-lockstep-and-loaded-loop.md) (reached from the debugger; TI's lockstep connection file did not get there in these tries).
- A Zephyr timer driver for the AM263P's RTI timers, then Zephyr's IPC with TI's mailbox hardware instead of raw shared memory.
- Zephyr on cores 2 and 3, and Zephyr on all four.

**To repeat this.** You need CCStudio 21 (DSLite), MCU+ SDK 26.01, Python with pyserial, and your probe's serial. Load the images one core at a time over the probe, core 0 last or first as noted, and read the serial output.

1. CoreMark on all four cores: build TI's CoreMark example for each core with the SDK makefile. Expect four lines like `FRL_CMR core 2 score 1463.7 errors 0`, then `FRL_CM_ALL mask 0xf got 0xf`.
2. TI's `ipc_notify_echo` example with FreeRTOS (or ThreadX) on core 0 and no RTOS on cores 1 to 3: build the system project with the SDK makefile and load all four cores. Expect `FRL_IPC ... remote_cores 3 ns_per_round_trip_per_core 3809`, then `All tests have passed!!`.
3. Zephyr on core 1 beside FreeRTOS on core 0: build a Zephyr app for the R5FSS0-1 core with a board, SoC and devicetree root of your own (the steps are in the [LP-AM263P FAQ](../../am263p/FAQ.md), "Does Zephyr run on the LP-AM263P?"), and a FreeRTOS image for core 0 that releases core 1. Expect core 1 to print "Zephyr 4.4.2 on R5FSS0-1" with a rising counter.

The loader needs a connection file for your probe, `AM263Px_ZCZ_C_<serial>.ccxml`, and a copy without the GEL script, `AM263Px_nogel_<serial>.ccxml`.

**Ask.** If you have Zephyr's timer or IPC running on an AM26x part, compare notes in an issue.
