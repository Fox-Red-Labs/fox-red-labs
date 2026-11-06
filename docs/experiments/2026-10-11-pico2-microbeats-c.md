<!-- experiment
id: E61
board: Raspberry Pi Pico 2 H
date: 2026-10-11
status: line found
reached: run
line: MicroPython: 65 s from download to a changed blink; C: 87 s including a build, with the tools already installed. A timing from a fresh Mac was not done.
stop: tools install not timed
-->
# Does MicroPython beat C to a first blink on the Pico 2, and by how much on the second change?

**Short answer.** On a ready Mac MicroPython took 65 s and C 87 s to a changed blink, both scripted. A first-time reader will take longer: the SDK install (about 1 minute of downloads plus 0.8 GB) and typing by hand are not in these numbers.

Terms: [vocab](../vocab.md). 1 kB = 1,024 bytes. Numbers: `data/results/pico2.csv`.

**Question.** Does MicroPython beat C to a first blink on the Pico 2, and by how much on the second change?

**Vendor position.** Raspberry Pi calls MicroPython the fastest way to get code running on Pico 2 (datasheet section 1).

**What was tried.** MicroPython v1.29.0 UF2 copied to the loader, `main.py` written through the REPL, half period 250 to 100 ms. C: pico-sdk 2.3.1 blink with `BLINK_MS` changed, `picotool load -f`.

**How far it got.** Run: both proven by serial timestamps (intervals 250 then 100 ms). The three-run comparison from the same start and the second change were not done.

**The line.** Not a limit; the experiment is incomplete: one run each, on a Mac that already had the tools.

**Ask.** Would a beginner find the Thonny path faster than the scripted REPL?
