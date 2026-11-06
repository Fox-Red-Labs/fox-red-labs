<!-- experiment
id: E13
board: LAUNCHXL-F2800137
date: 2026-10-07
status: works
reached: verified
line: Works: two wires from the LP-MSPM0L1306's EnergyTrace probe to the chip side of the C2000 board's JP2 jumper make it the meter. Above the HALT reading: 35.4 mA running CoreMark at 120 MHz, 2.6 mA in STANDBY. The C2000 board's own debugger cannot connect on borrowed power (open).
stop: none
-->
# Can the TI LAUNCHXL-F2800137, which has no EnergyTrace, borrow the LP-MSPM0L1306 probe to measure its chip's current?

**Short answer.** Yes. The LAUNCHXL-F2800137 has no EnergyTrace hardware, so its power rows cannot come from its own probe. Two wires from the LP-MSPM0L1306's EnergyTrace probe (3.3 V and GND) to the chip side of the C2000 board's JP2 jumper made the MSPM0 board the meter, and one flash image gave the whole profile in one trace: above the 6.10 mA HALT reading that holds the board's own circuits, 35.4 mA running CoreMark from flash at 120 MHz (828.1 µJ per iteration), 23.2 mA in IDLE, 2.6 mA in STANDBY, and 263 µA per MHz between 60 and 120 MHz. Two catches: the board's own debugger cannot connect while the chip is on borrowed power, so flash first; and STANDBY needs a 30 ms wait after a watchdog wake before re-entry, or the chip stays in IDLE.

Terms: [vocab](../vocab.md) (EnergyTrace, debug probe, isolation jumper). Errors and fixes: [LAUNCHXL-F2800137 FAQ](../../c2000-launchpad/FAQ.md).

**Question.** The results store has every power row for the F2800137 waiting on a bench meter. Can a probe already on the bench stand in for the meter?

**Vendor position.** The LAUNCHXL-F2800137 user guide SPRUJ32 was read in full on 2026-10-07. It lists "On-board XDS110 debug probe" and never mentions EnergyTrace. TI's SimpleLink F3 SDK "EnergyTrace User Guide", read 2026-10-07, measures a CC23xx LaunchPad with a CC26x2R LaunchPad: "Connect just the 3.3 V and GND lines." A TI staff answer on E2E (thread 1243687, 2023) puts the embedded EnergyTrace's supply at about 75 mA.

**What was tried.** Asked the F2800137's probe (<probe serial>) for EnergyTrace through CCStudio 21.0.1's XDS110 driver with `tools/energytrace/etcap.py`: `XDS_Open` in EnergyTrace mode returns -260, and opened plainly the probe reports hardware id 0x2F with `ET_HardwareInfo` -670 and EnergyTrace type 0; the LP-MSPM0L1306's probe returns 0, type 2, and streams. Drew the wiring from the two user guides: J101 on the MSPM0 board (3V3 jumper out, wire from the probe-side pin), JP2 on the C2000 board (+3V3 shunt out, wire to the chip-side pin), GND to GND.

**How far it got.** Measured. It was wired from the picture; the demo ran on borrowed power at about 11 mA; the profile image was flashed on normal power (the debugger refused on borrowed power) and the trace from power-up shows every phase edge: spin 38.7 mA, CoreMark 41.5 mA, IDLE 29.3 mA, STANDBY phase 30.2 mA, HALT 6.78 mA raw. A second image with a 30 ms wait before re-entering STANDBY, a 60 MHz spin and HALT with the PLL off, restarted by the reset button, gave STANDBY 8.68 mA between wakes, 22.9 mA at 60 MHz and HALT 6.10 mA.

**The line.** The method holds to 41.5 mA of load, under the probe's 75 mA, and the chip-side pin of JP2 was the one drawn. Two lines found: the board's own XDS110 answers `Error -2131 Unable to access device register` whenever the chip is on borrowed power, and STANDBY entered by the TRM's steps and re-entered straight after the wake read 30.2 mA against IDLE's 29.3. HALT was first entered with the PLL on against the TRM's instruction; the 6.10 mA reference is chip-in-HALT plus the CAN transceiver, power LED and encoder buffer.

**Ask.** Anyone who has run EnergyTrace on a C2000 LaunchPad this way: what did the chip side read at idle?
