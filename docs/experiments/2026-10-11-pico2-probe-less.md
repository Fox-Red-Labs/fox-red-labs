<!-- experiment
id: E64
board: Raspberry Pi Pico 2 H
date: 2026-10-11
status: line found
reached: run
line: Almost all: speed, size and model rows come back over USB serial. Not reached without extra hardware: RAM reads after a crash, power, and the two loopback tests that need wires.
stop: debugger reads and power
-->
# Can a board with no debug probe give every battery number, and which does it not reach?

**Short answer.** Nearly everything in the standard suite ran with no probe: the program prints its own report over USB serial, `picotool load -f` loads and restarts it with no button, and the flash was backed up with `picotool save`. The gaps are reads after a hang, power (needs the LP-XDS110ET), and the UART and ADC loops (wires).

Terms: [vocab](../vocab.md). 1 kB = 1,024 bytes. Numbers: `data/results/pico2.csv`.

**Question.** Can a board with no debug probe give every battery number, and which does it not reach?

**Vendor position.** Pico 2 datasheet section 5.8: a 3-pin SWD header, no probe on the board.

**What was tried.** Every image prints its report on USB serial repeated every 10 s. Flash backup and restore by picotool in the loader.

**How far it got.** Run: roughly 100 result rows on two core types without a probe. An example without the SDK's USB reset (`dev_multi_cdc`) left the board needing a button press.

**The line.** The limit is the missing button-free path for programs that have no USB serial, which needs a hand per image; the examples that print on a UART and the Zephyr images fall here.

**Ask.** Does the Debug Probe change which rows are reachable?
