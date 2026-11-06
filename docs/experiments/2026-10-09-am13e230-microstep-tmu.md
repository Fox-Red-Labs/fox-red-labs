<!-- experiment
id: E146
board: LP-AM13E230
date: 2026-10-09
status: in progress
reached: run
line: The same six wires that stepped the kit 28BYJ-48 on and off now microstep it. The chip's PWM units give sinusoidal duties, updated 20,000 times a second. The update handler costs 138 cycles with the trig unit (1.4 percent of the CPU) and 246 with the C library sine (2.5 percent). Whether the shaft turns under this drive was not watched: the run was hands-free.
stop: none
-->
# Does microstepping the kit stepper through the LP-AM13E230's PWM units cost the chip anything?

**Short answer.** Almost nothing. Four sinusoidal duties from the PWM units, recomputed 20,000 times a second, cost the chip 138 cycles an update with the trig unit (TMU) doing the sine, 246 with the C library sine on the FPU. That is 1.4 and 2.5 percent of a 200 MHz core. The other 97 percent is free for a current loop, a knob, a display, or a neural network. Nobody watched the shaft during this run, so "it turned" is not claimed here.

Terms: [vocab](../vocab.md). Board page: [LP-AM13E230 README](../../am13e230/README.md), [FAQ](../../am13e230/FAQ.md). On-off version of the same wiring: [stepper on and off](2026-10-09-am13e230-stepper-uln2003.md). Program: `am13e230/stepper-uln2003/microstep/`. MCPWM is the chip's motor PWM unit. The TMU is its trig math unit, reached through compiler intrinsics. DWT is the core's cycle counter.

**Question.** After the on-off version, the question was whether a motor-control chip earns its keep on a hobby stepper, since four on-off pins and a delay loop run on any microcontroller. Microstepping is the first step up: each coil gets a sine-shaped duty instead of on or off, so the shaft moves in hundreds of positions per step instead of one. What does the update loop cost, and does the trig unit matter?

**Vendor position.** TI's motor examples for this board (`solutions/motor_control/`) are three-phase field-oriented control on a DRV8323RH or DRV8329A BoosterPack; none covers a stepper. The data sheet (SPRSPC3A) gives the TMU as single-cycle-throughput sine and cosine instructions. This repo's FOC step measured the C library pair at 513 cycles against the TMU's handful.

**What was tried.** Same six wires as the on-off version. IN1, IN2 and IN3 are on MCPWM4 outputs 1A, 2A and 3A. IN4 is on MCPWM1 output 2A, because PB7 has no free MCPWM4 output. Both modules run at 20 kHz. MCPWM4's counter-zero event interrupts at 20 kHz. The handler advances the electrical angle and takes two sines, since the other two coils are the negatives. It clamps each to its positive half, because the ULN2003 is unipolar, scales to a 95 percent peak duty and writes four shadow compare values. One shaft turn is 512 electrical cycles over 100,000 updates (5 s), then a 1 s pause with all coils at the 1-count minimum, then a turn back. Three images: `tmu`, where fastrts_tmu.h remaps sinf to the TMU; `sw`, with the C library sinf; and `noisr`, where the interrupt is never enabled. Each ran hands-free with the cycle counter read at handler entry and exit. Results were read from RAM over the probe after the run.

**How far it got.** Run. The handler count, phase and turn counters advanced as designed, with 6 turn cycles in the 75 s the first image ran. Not verified: that the shaft turned, since the run was hands-free. The on-off version on the same wiring turned in the on-off version.

| Image | Handler, mean | Best | Worst | Share of the CPU at 20 kHz |
|---|--:|--:|--:|--:|
| TMU sine | 138.2 cycles (0.69 µs) | 61 | 219 | 1.4 percent |
| C library sine | 245.6 cycles (1.23 µs) | 61 | 354 | 2.5 percent |

Best is a pause-phase call, the sine of zero. The figures cover the handler body only. Hardware interrupt entry and exit take about 12 cycles each way on a Cortex-M33 and are not included. The `noisr` image was meant to show the main loop's lost share directly. Its run time differed from the others by the flash tail, so that comparison is not reported.

**The line.** None reached. At 20 kHz the update takes 1.4 percent of the core with the TMU.

**Where it stopped.** Nowhere in the chip. The limit is in the driver. The ULN2003 is a bank of Darlington switches with about a 1 V drop and no current sensing, so the coil current follows the duty only roughly and nothing closes the loop.

**Ask.** Does the shaft move smoothly under the sine duties through a ULN2003, or does the Darlington drop flatten the waveform enough to cog? One look at the shaft answers it.

**What a reader should take.** Microstepping is cheap on this chip: the trig unit halves an already small handler. The remaining cost is not in the core but in the driver board, which cannot tell the chip what current flows. That is the next part to change.
