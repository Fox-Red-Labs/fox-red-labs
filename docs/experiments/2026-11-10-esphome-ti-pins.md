<!-- experiment
id: E320
board: LP-EM-CC2755P10
date: 2026-11-10
status: works
reached: verified
line: Every DIO of the LP-EM-CC2755P10 (23 pins) and LP-EM-CC2340R5 (26 pins) is now a valid ESPHome pin on the cc27xx and cc23xx platforms, with a table of header pin, board use and signals made from TI's board and device data. On the CC2755P10, 19 DIOs read 1 with a pull-up and 0 with a pull-down and followed an output in a self test; SWD and the 32 kHz crystal pads are refused.
stop: none
-->
# Can every DIO of the TI LaunchPad kits be an ESPHome pin, with the pins the board already uses marked?

**Short answer.** Yes. The first platforms mapped two LED pins and ignored the rest. The TI GPIO driver on these chips indexes its pin table by DIO number, so the platform's pin number is the driver index and every DIO works with no extra SysConfig entry. A table (`components/cc27xx/pindata.json`, printed in the README) lists each DIO, its BoosterPack header pin, what the LaunchPad already uses it for and which peripheral signals the chip can route to it; ESPHome's pin validation uses it, so a pin that does not exist or is the debug port gets an error naming the board. A self test on the LP-EM-CC2755P10 set 19 DIOs to input with pull-up, input with pull-down, and output high and low, and read each back: 18 gave 1, 0, 1, 0 as expected; DIO16 (the red LED) read 0 with the pull-up because the LED loads the pin, and DIO19 (the SPI flash's chip select) read 1 with both pulls because the flash holds it.

Terms: [vocab](../vocab.md). Board facts: [CC2755P10](../../cc2755p10/README.md), [CC2340R5](../../cc2340r5/README.md).

**Question.** Which DIOs can an ESPHome YAML name on each board, and which are the board's own?

**What was expected.** The user guides name the header pins; TI's SDK carries the same data in `source/ti/boards/.meta/<board>.syscfg.json`, and SysConfig's device files list which signals each pin can carry. Combining them gives the table. The CC2340R5 board file is for the 40-pin RKP package, not the 32-pin RHB package in the SDK's default device name.

**What was tried.**

1. `gen_pins.py` reads the two TI files and writes the JSON. First run used the wrong package for the CC2340R5 (RHB), which put the LEDs on DIO17 and DIO18; the quick start guide (SWRU588A) has them on DIO14 and DIO15, buttons on DIO10 and DIO9, flash select on DIO6, UART on DIO20 and DIO22, I2C on DIO0 and DIO24. The RKP package matches all of those.
2. Pin validation in the platform; GPIO mapping in the shim for any DIO between the driver's lower and upper bound.
3. Self test on the CC2755P10 (in a YAML `interval` lambda, 19 pins).

**How far it got.** Verified on the CC2755P10; the CC2340R5 uses the same code, checked against the guide's table and not self-tested pin by pin.

| Check | Result | Cause tag | What fixed it |
|---|---|---|---|
| Table for the CC2340R5 with the SDK's default package | LEDs on DIO17, DIO18 | Our mistake | Use the RKP package the LaunchPad carries |
| `binary_sensor: gpio` default | `KeyError: 'use_interrupt'` | Third-party behavior | Per-platform defaults have no entry for the TI names; the platform uses nrf52's, with interrupts off |
| Self test, 19 DIOs | 17 of 19 gave pull-up 1, pull-down 0, out high 1, out low 0; DIO16 pull-up 0 (LED); DIO19 pull-down 1 (flash) | | Both explained by the board |

**What this does not establish.** The read-back tests the driver and the input buffer, not a connected circuit. GPIO interrupts are not wired up, so a button is polled (16 ms loop). The SPI-flash pins share the BoosterPack SPI bus. No pin was checked against a scope.

**Ask.** None.

**Effort.** 50 minutes.

**Verdict.** Good. The pin table is the useful part; the GPIO code was already almost right.
