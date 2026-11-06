# Porting the motor stack to a new board: LP-MSPM33C321A, LP-AM13E230 and the next ones

How to move the motor platform from one board to the next: pins, current sensing, protection, the bring-up order, and what each chip's hardware is worth. It grows with every board; each rung below says what to run, what passing looks like, and what went wrong the first time. Design of the layers and verbs: `DESIGN.md`.

Terms: [docs/vocab.md](../../docs/vocab.md). Contents: 1 board profile, 2 bring-up ladder, 3 traps, 4 what each chip brings.

First worked board: LP-MSPM33C321A with the DRV8376EVM and the BLY171S, 2026-10-20 (`mspm33c321a/motor/`). Reference board: LP-AM13E230 (`am13e230/foc-drv8376/`).

## 1. The board profile

Everything board-specific goes in one profile before any motor code is written: `boards/<board>/board.json`. Fields:

| Field | What | Where it comes from |
|---|---|---|
| `site` | which BoosterPack site the driver board sits on | the LaunchPad kit's SysConfig board file lists every site (`source/ti/boards/.meta/<board>.syscfg.json`); confirm with the pin scan (rung 1) |
| `pins` | MCU pin for each driver signal: INHx, INLx, Halls, nSLEEP, DRVOFF, nFAULT, ISENx, VSENx, VSENVM, ILIMIT | header pin number from the driver board's guide, then the board file's map of header pin to MCU pin |
| `pwm` | timer instance and the channel each phase lands on, complementary pins, dead band | the data sheet's pin function table and SysConfig: the INH and INL of one phase must be a channel and its complement on one timer, or the board is six-step only |
| `adc` | converter and channel for each sense signal; which can sample at the same instant | SysConfig's `adcPinN` assignments; two phase currents on two converters sample together |
| `sense` | gain (V/A), reference (V), divider ratio for voltages, ADC reference | driver board straps and schematic; zero-current counts read at rung 2 tell the reference |
| `protect` | which hardware stops exist on this board: comparator on a current-sense pin, timer fault input on nFAULT, a DAC that can set ILIMIT | data sheet pin functions; each missing one is a wire, a resistor or a software check |

## 2. The bring-up ladder

Run in order. Each rung is safe on its own and has a pass line; do not climb past a failed rung. Write the expected value for each rung before you run it.

| Rung | What runs | Pass | M33 result |
|---|---|---|---|
| 0. Map | profile from the board file and the driver guide | every signal has a pin; gaps listed (no comparator, no DAC, no fault input) | three gaps: no DAC on the ILIMIT pin, nFAULT not a timer fault input, no comparator on the current-sense pins; none stops a spin (stacking alone runs the motor) |
| 1. Pin scan | read-only: every candidate pin of every site read with the MCU's pull-up, then pull-down | the driver board's pins hold their level (its pull resistors, Halls, sense outputs); the empty site follows the pull | site 1 held, site 2 followed: profile right |
| 2. Safe state and sense | outputs off, driver asleep then awake; read sense zeros, bus, Halls, nFAULT | sense zeros near VREF/2, bus matches the supply, a valid Hall state, nFAULT released within 10 ms of wake | zeros 1,845 to 1,856 counts (1.49 V), bus 18.1 V, Hall state 2, nFAULT released |
| 3. Phase voltage | one high side on, others off (no current path), with the output disable on and off | phases at 0 V with the disable on, at the bus with it off; nFAULT stays high | 0.0 V and 18.1 V on all three phases; showed the driver is strapped for 6x PWM |
| 4. Single pulse | each of the six line vectors for 18 us, current read at the end | the current shows on that vector's low-side phase, near V t / 2L | 0.164 to 0.185 A against 0.19 A |
| 5. Burst | each vector for 40 PWM periods at the first-spin duty | the current builds period to period toward D V / 2R | 0.34 to 0.36 A on all six |
| 6. Table search | 12 Hall-to-vector tables, 0.4 s each, then each candidate steady for 1 s | three tables turn each way; keep the one with the lowest current | forward offsets 1, 2, 3 at 271, 40, 173 mA: offset 2 is centered |
| 7. First spin | the best table, 3 s | steady sector rate, no trip | about 160 rpm at 6 % on time, 0.40 A peak |
| 8. Timer PWM | pad duty with the outputs disabled (input buffers on), then the same spin from the PWM timer with dead band | pads show the set duty and dead band; the spin at least matches rung 7 | pads 5.8 % and 93.8 %; about 430 rpm against 160, 515 of 515 sectors forward |
| 9. Fast stop | the fastest path from the driver's nFAULT to outputs off: a timer fault input or comparator where the pins allow, a top-priority GPIO interrupt where they do not; stimulus a real driver fault (ILIMIT at 0 V) | outputs off within a microsecond or two; the fault clears and the next run starts | no fault pin or comparator on PB30: the interrupt set DRVOFF high 0.97 to 1.31 us after nFAULT fell, five of five, at 32 MHz |
| 9b. Characterize | coast capture of the phase voltages with the outputs off, start from standstill, loop cost per period | back-EMF within 10 % of another board on the same motor; loop under 50 % of the period | back-EMF at least 3.17 to 3.36 V per 1,000 rpm (AM13 3.44); start 10 ms to the first edge, about 30 ms to speed; loop 22 % mean, 32 % worst at 32 MHz |
| 10. Sense timing | center-aligned PWM checked at the pads, then the ADC started by the timer's zero event | all high sides off at the trigger; a fixed vector gives Ia with Ib and Ic each minus half, summing to zero | zero within 8 mA; dead band accounts for the shortfall against R |
| 11. Hall calibration | the field angle swept one mechanical turn up and back | rising angle turns the rotor forward in the core's Hall order; six boundaries 50 to 70 degrees apart | 53 to 66 degrees; 44 degrees between the up and down crossings, averaged |
| 12. Current loop | Hall FOC from portability/motor/core, small Iq, voltage cap | Id near zero, Iq on its reference until the cap, steady speed; the voltage balance matches R, back-EMF and dead band | 685 rpm at 1.5 V, Id 0.000 A, Iq 63 mA = friction; 1.51 V predicted against 1.495 V |
| 13. Speed loop | PI on the tracker's speed over the current loop, designed from the measured J and Kt; steps up and down | means within a few percent; overshoot and rise near the design | means within 1 %; rise 4 to 17 ms; overshoot near 40 % (Hall speed lag) |
| 14 and up | sensorless, identification, field weakening | per tier, `DESIGN.md` section 5 | |

## 3. Traps, and the rule each one left

| Trap | What happened | Rule |
|---|---|---|
| The generated init resets the GPIO ports | `SYSCFG_DL_init()` resets GPIOA, B and C; called after the safe-state pin setup, it turned every output back into an input. The driver board's own DRVOFF pull-up kept it safe | call the generated init first, then set pins; a driver board needs a pull-up on its disable line (the DRV8376EVM has one) |
| ILIMIT floating | the driver's current-limit input reaches only a header pin; on the M33 that pin has no DAC, it floats near 0 V and the driver faults on the first milliamps of low-side current | the profile names how ILIMIT gets a defined voltage: an MCU DAC, a resistor divider, or driven above VREF to turn the limit off (the M33 now) |
| A lead carried over from the last board | the AM13 drives ILIMIT through a temporary lead; after a board swap that lead can look connected and not be | rung 1 checks every protection net as well as the signal pins |
| Two BoosterPack sites | the M33 has two; their maps differ completely | the pin scan settles which one is in use |
| Halls on the debug UART pins | on the M33 (and the G3507, G3519, G5187) Hall A and B are the backchannel UART pins | results go to RAM and are read over the probe, or the console moves to other pins |
| Rewriting every gate pin each period | on the M33's software PWM, clearing and setting all six gate pins every period stopped the current building (about 15 mA after each pulse instead of 0.3 A); writing the low side only on a commutation fixed it. Cause not found | change gate pins only when the vector changes; the timer PWM avoids it |
| A sleep is not a fault test | taking nSLEEP low pulls nFAULT low but also turns the sense amplifiers off, so a software current check trips first and the time measured is the driver's sleep entry | use a real driver fault as the stimulus: ILIMIT below VREF/2 trips the limit on the first pulse |
| Compare at the period is not 0 % | on the M33's down-counting edge-aligned timer a compare equal to the period held the high side on all period; two phases high, no current | read the pins with the outputs disabled before driving current; hold a phase low with GPIO, not with a timer edge case |
| Synchronous freewheel raises stall current | the timer's complementary low side carries the freewheel current, so stall current rises from about 0.3 to 0.7 A at the same duty | set the first-spin duty and trip from D V / 2R, not from the diode case |
| The best-travelling table is not the best table | the search kept the table that travels furthest, which commutates 60 degrees early: 438 rpm at 173 mA on the M33, where the centered table ran 280 rpm at 40 mA (friction alone). Both the M33 and the AM13 showed the early table's 440 rpm per volt | among the tables that turn, keep the one with the lowest current |
| Waiting on the ADC inside the period | not the cause of the slow period: a conversion pair takes 6.6 us at 32 MHz | time anything blocking inside a period before blaming it |

## 4. What each chip brings, and what it is worth

The same stack runs on every part; what changes is which blocks do the work in hardware. Each cell is measured on the bench or taken from the data sheet or register map; the value row comes from the cost matrix (`DESIGN.md` section 10) and the ladder.

| Board | Core, clock | Math help | Complementary PWM with dead band | Hardware trip | ILIMIT reference | ADC for current | Ceiling |
|---|---|---|---|---|---|---|---|
| AM13E230 | M33 200 MHz, FPU | TMU (sin, cos, atan in hardware) | MCPWM | CMPSS comparators on the sense pins, two-sided on two phases | DAC, by a lead | 3 x 12-bit, simultaneous | sensored and sensorless FOC measured; NPU beside the slow loops |
| MSPM33C321A | M33 160 MHz, FPU, DSP | none | TIMA0_1 | none on the sense pins or nFAULT; a top-priority GPIO interrupt stops the outputs in 0.40 to 0.43 us at 160 MHz | none on the header pin | 2 x HSADC 12-bit; the three currents share HSADC0 | six-step and float Hall FOC measured (32 to 35 % of the CPU at 20 kHz, libm trig) |
| MSPM0G3519 / G3507 | M0+ 80 MHz | MATHACL (divide, sqrt, sin, cos, atan) | TIMA0, TIMA1 | 3 comparators; nFAULT is a TIMA fault input | DAC on the header pin | 2 x 12-bit 4 Msps, simultaneous | TI's own Hall and sensorless FOC libraries ship for it |
| MSPM0G5187 | M0+ 80 MHz | none (an NPU) | TIMA0 | 1 comparator; nFAULT is a TIMA fault input | none | 1 x 12-bit | fixed-point FOC, NPU |
| MSPM0L1306 | M0+ 32 MHz | none | none (TIMG only) | 1 comparator | none | 1 x 12-bit | six-step only |
| CC2340R5 (CC23) | M0+ 48 MHz | none | LGPT, dead band, fault and park on the instances that report them (register map, `hw_lgpt.h` DESCEX.HDBF) | LGPT fault input | to check | 1 x 12-bit | six-step, fixed-point FOC at low PWM rates; motor plus Bluetooth on one chip |
| CC27xx (CC2745) | M33 96 MHz, FPU | APU (vector and matrix accelerator) | LGPT, as the CC23 | LGPT fault input | to check | 1 x 12-bit | float FOC at moderate rates; motor plus Bluetooth |
| CC35xx | Wi-Fi 6 and Bluetooth part | to check | to check | to check | to check | to check | motor plus Wi-Fi, to be assessed |

First measured trade-off, one board at two clocks: moving the M33 from 32 to 160 MHz cut the fault reaction from 0.97 to 1.31 us to 0.40 to 0.43 us (2.5 to 3x, not 5x; running the handler from SRAM did not help, so the limit is not the flash: suspected the peripheral bus clock) and the six-step loop from 22 to 4.6 percent of the CPU; the ADC, clocked with the CPU, got the full 5x.

Second measured trade-off, one board, one build switch: the core's table sine against libm on the M33's float FOC cut the control loop from 1.8k to 1.3k cycles a period (27 percent) with no change in the motor's behavior.

What the table turns into, per board, once the ladder and the cost matrix have run: the highest PWM and control rate it carries, control ISR cycles per tier (`foc_step` is already measured on most), current-sense latency and whether two phases sample at once, hardware-trip latency or the software check's worst case, flash and RAM per feature, and the highest tier reached. Those numbers are the trade-off story: what a comparator, a DAC, a math block or a faster clock buys in rate, safety and code.
