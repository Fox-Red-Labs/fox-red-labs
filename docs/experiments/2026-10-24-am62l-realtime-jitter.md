<!-- experiment
id: E272
board: TMDS62LEVM (AM62L evaluation module)
date: 2026-10-24
status: line found
reached: run
line: cyclictest on the stock PREEMPT kernel gives a 442 us median-of-3 worst case idle (389 to 645) and 1,074 us under two-core load (375 to 1,835 in four runs, one run at 93,699 us). PREEMPT_RT and OpenPLC were not run.
stop: unknown
page: none
-->
# How late can a 200 us timer wake up on the AM62L's stock Linux?

**Question.** How late can a 200 us timer wake up on the AM62L's stock Linux?

**How far it got.** Idle, the worst wake-up in 60 s was 389, 645 and 442 us (median 442). With `stress-ng --cpu 2 --vm 1 --vm-bytes 128M` it was 726, 1,835, 93,699, 1,074 and 375 us (median 1,074). Average stays 8 us idle and 13 to 14 us loaded. A soft real-time loop at 1 ms is safe on average and not guaranteed. Terms: [vocab](../vocab.md). FAQ: [AM62L](../../am62l-evm/FAQ.md).

**Expectation.** The earlier single runs gave 474 us idle and 546 us loaded; a repeat should land in the same range. It did idle. Loaded it did not: four of five runs were higher than 546 us and one was 93.7 ms.

**The line.** The worst case under load is the limit, not the average. The 93,699 us happened in one of five loaded runs and did not come back in two repeats. Whether it was the first sample after cyclictest started (stress-ng had begun 3 s earlier) or a mid-run spike was not separated; the run had no histogram or trace. Treat it as an unexplained outlier, not a typical value.

**What was tried.** `cyclictest -m -p 90 -i 200 -l 300000 -q`, one thread per run (60 s), kernel 6.18.38 PREEMPT, performance governor, 1.25 GHz. Every run's worst case is listed in the short answer.

**Not run.** PREEMPT_RT: no RT kernel is on the boot media and booting one means writing another image. OpenPLC v4 runtime: the arm64 container image pulled (1.45 GB), then removed unstarted.

Rows: `sched_jitter_us`, variants `idle-60s-x3`, `load-2cpu-60s-x5`, `preempt-rt-idle`, `openplc-v4-cycle`.
