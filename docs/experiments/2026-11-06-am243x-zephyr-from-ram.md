<!-- experiment
id: E316
board: LP-AM243
date: 2026-11-06
status: works
reached: verified
line: upstream Zephyr hello_world and blinky run from RAM on the LP-AM243 (AM2434) with the boot switch (SW4) as shipped and no bootloader change; the loader must stop the last program's timer and set up the MPU and caches first
stop: none
-->
# Can upstream Zephyr hello_world run on the LP-AM243 (AM2434) without moving a boot switch?

Terms: [docs/vocab.md](../vocab.md), [LP-AM243 FAQ](../../am243x/FAQ.md). **MPU** is the core's memory protection unit. **OCSRAM** is the on-chip RAM. **ELF** is the compiled program file with its load addresses. **SBL** is TI's second stage bootloader. **TIMER8** is the timer TI's SDK uses for its operating system tick. A **start stub** is a few instructions the loader writes into the core's own fast memory and runs before the image.

**Short answer.** Yes. Zephyr's `hello_world` prints its banner and `blinky` toggles repeatedly on the console, loaded to RAM by a debugger script with the boot switches as shipped. The blink interval was not timed.

**Question.** Zephyr's board page boots the LP-AM243 through TI's SBL, with a patched entry point and a boot-switch change. Is there a route that leaves the board as it came?

**Vendor position.** The Zephyr board page for the LP-AM243 (docs.zephyrproject.org/latest/boards/ti/lp_am243/doc/index.html, read 2026-11-07) says: "Based on the MCU+ SDK from Texas Instruments then a Second Stage Bootloader (called SBL) should be executed", that the SBL's source "can be selected with the DIP switches", and, under Flashing, "Since there exists no Zephyr support it's required to use one of the SBL bootloader examples from the TI MCU+ SDK." It also has you edit the SDK's bootloader to force the entry point to 0.

**Expectation, written first.** A fault early in boot, because the image expects the bootloader's setup.

**What we tried.** On 2026-11-06 on a Mac with CCStudio 21.0.1: Zephyr 4.5.0-rc2 (a915b2f1267) with Zephyr SDK 1.0.1 (GCC 14.3.0), built for the board target `lp_am243/am2434/r5f0_0` (for example `west build -b lp_am243/am2434/r5f0_0 samples/hello_world`; the exact command was not saved, the target is the one in the results row). Load and capture the console with the loader script, setting `AM243X_BARE=1`:

```
AM243X_BARE=1 python3 am243x/tools/ram_capture.py <path to zephyr.elf> run.log
```

The capture script needs Python 3 with pyserial (`python3 -m pip install pyserial`) and CCStudio 21.0.1 at its default path; it calls CCStudio's `run.sh` with `am243x/tools/ram_run.js` to load the image.

"Boot switches as shipped" means SW4 is left as it came in the box; its position was not recorded. In that state CCS's `memory.loadProgram` call hangs, so `am243x/tools/ram_run.js` instead halts R5F core 0, resets that core, writes each ELF segment straight to RAM and starts it; flash is never written, and a power cycle returns the board to its shipped firmware. A plain load of the Zephyr ELF faulted on the first exclusive load in OCSRAM with `Synchronous External Abort`, in `slice_reset`. Suspected cause, from the fault address and the fix that worked: Zephyr's image writes no MPU or cache enable, so `AM243X_BARE=1` adds a start stub that programs the MPU, turns on the caches and jumps to the entry. With it, the `hello_world` sample printed. The `blinky` sample printed once and stopped: its timer and interrupt state were left by the previous program. The loader now stops TIMER8 and closes the interrupt controller before every load, and the `blinky` sample ran. TI's own images also ran after a Zephyr run once the same clean-up was in; this was seen, not logged. Logs: `am243x/bench/zephyr-hello-2026-11-06.log` (banner and `Hello World! lp_am243/am2434/r5f0_0`) and `am243x/bench/zephyr-blinky-2026-11-06.log` (repeated `LED state: OFF` and `LED state: ON` lines, no timestamps).

**How far it got.** Run: both samples on R5F core 0.

**The line.** No limit found for these samples. The clean-up matters both ways: without it, a program loaded after Zephyr's timer was left on stays in its interrupt handler and prints nothing.

**Next moves.** Other Zephyr samples and the Ethernet driver; time the blink; run the M4F target's `hello_world`; try the SBL route on a second board.

**Limits.** One board. Blink proven on the console only, not on an LED and not timed. The M4F target's `hello_world` builds and was not run; its `blinky` does not build (no `led0` alias). The SBL route on the Zephyr page was not tried. Results: `data/results/am243x.csv`, logs in `am243x/bench/`.
