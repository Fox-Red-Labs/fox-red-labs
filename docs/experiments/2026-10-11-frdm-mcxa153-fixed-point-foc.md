<!-- experiment
id: E55
board: FRDM-MCXA153
date: 2026-10-11
status: parked
reached: run
line: On the FRDM-MCXA153 (Cortex-M33, no FPU) NXP's fixed-point RTCESL library does one motor-control step in 448 cycles, against 5.4k cycles for the same job in float; timed only, the output was not compared with the float step.
stop: none
-->
# Does NXP's fixed-point library fix the FRDM-MCXA153's missing floating-point unit for motor control?

**Short answer.** Yes for speed. One field-oriented control step (the MCXA153 has no floating-point unit) takes 448 cycles with NXP's fixed-point RTCESL library, 4.7 µs at 96 MHz, against 5.4k cycles for the float `foc.c` the other boards run. That is 12 times fewer cycles, close to the LP-MSPM33C321A with an FPU (489 cycles at 32 MHz) and the LAUNCHXL-F2800137 (400). The two steps are not the same arithmetic (Q15 fractions against float) and their outputs were not compared, so this is a speed result only.

Terms: [vocab](../vocab.md). 1 kB = 1,024 bytes. **RTCESL** is NXP's Real-Time Control Embedded Software Libraries; **Q15** is a 16-bit fraction between -1 and 1; **FOC** is field-oriented control, the math a motor drive runs every PWM period.

**Question.** Can the FRDM-MCXA153's soft-float penalty on motor control be avoided with the library NXP ships for this chip?

**Vendor position.** The MCUXpresso SDK 26.09.00 ships `middleware/rtcesl/CM33NODSP_RTCESL_4.8.1_MCUX`, a build of the library for a Cortex-M33 without DSP instructions, which is this core. NXP sells the chip for BLDC and PMSM motors (data sheet MCXAP64M96FS3 Rev. 5.1).

**What was tried.** A test program: Clarke, sine and cosine, Park, two PI controllers with anti-windup, inverse Park and standard space-vector modulation, all RTCESL calls, linked with `libGFLIB.a`, `libGMCLIB.a` and `libMLIB.a`. GCC 12.2 -O2, code in flash with the cache on, 96 MHz, 256 inputs, best of 5 passes, loop overhead of 5.1 cycles included.

**How far it got.** Run: `foc_step_q15` 448.0 cycles per step. Not done: a comparison of its duty cycles with the float step, and the ADC-triggered loop around it.

**The line.** No limit reached. For reference, the float `foc.c` on this chip: 5.4k cycles at -O2. A 448-cycle step would allow a loop of about 200 kHz on the step alone; a real loop adds the ADC and interrupt entry (17 to 29 cycles of latency here).

**Ask.** Is the `CM33NODSP` build the one NXP intends for the MCXA family, and does NXP publish cycle counts for its steps to compare?
