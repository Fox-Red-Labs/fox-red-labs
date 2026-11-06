<!-- experiment
id: E59
board: FRDM-MCXA153
date: 2026-10-11
status: works
reached: verified
line: NXP's SDK example reads the board's I3C temperature sensor and prints it on the serial port with no wires: 22.56 C, 13.6 s to build and 12.4 s to flash.
stop: none
-->
# Can the NXP FRDM-MCXA153's on-board temperature sensor be read and printed in under ten minutes?

**Short answer.** Yes, in under a minute once the SDK is installed. NXP's example `driver_examples/i3c/master_read_sensor_p3t1755` builds in 13.6 s, flashes in 12.4 s and prints `Temperature:22.562500` on the board's serial port, with nothing wired.

Terms: [vocab](../vocab.md). **I3C** is a two-wire sensor bus that replaces I2C; **P3T1755** is NXP's temperature sensor soldered on this board.

**Question.** Is a "sensor to serial line" demo as quick here as the thermistor demo on the LP-MSPM0L1306?

**Vendor position.** UM12012 Rev. 2.0 lists the P3T1755 on the board's I3C bus (read 2026-10-07). The example ships in MCUXpresso SDK 26.09.00.

**What was tried.** An example-sweep script built the example with `west`, flashed it with pyOCD and captured the serial port for 6 seconds, as it does for every example that needs no wiring.

**How far it got.** Verified by the serial line (`Temperature:22.562500`). The reading was not checked against a second thermometer, and it was one reading in a 6-second window.

**The line.** No limit reached. The L1306 thermistor demo was not timed the same way, so no ratio is claimed.

**Ask.** None.
