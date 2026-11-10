<!-- experiment
id: E247
board: LP-MSPM33C321A and LP-MSPM0G3519
date: 2026-10-24
status: works
reached: verified
stop: none
line: TI's DriverLib examples for GPIO, UART, timers, SPI and the CRC block run on the other chip with the app files untouched: 7 of 8 G3519 examples checked on the LP-MSPM33C321A and 9 of 9 MSPM33 examples checked on the LP-MSPM0G3519 (one needed 5 app lines). I2C and ADC do not move: the MSPM33 has different blocks.
page: none
-->
# Do TI's MSPM0 DriverLib examples run on the MSPM33, and the MSPM33's on the MSPM0G3519, with the app files unchanged?

**Short answer.** Yes for GPIO, UART, timers, SPI and CRC, in both directions. The example's own SysConfig file goes with it, the target's SysConfig rejects the lines that do not apply, and the app builds and runs. Each result below was read over the probe from pins, registers and RAM variables the app defines. I2C and ADC stopped at build: the MSPM33's I2C is the UNICOMM block with new function names and its ADC is the HSADC block.

Terms: [vocab](../vocab.md). Method, tools and the migration notes: [PORTING.md section 7](../../portability/everywhere/PORTING.md). Checked 2026-10-24 on macOS 15.7 (Intel), MSPM0 SDK 2.11.00.07, MSPM33 SDK 1.04.00.00, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0 (the validated 1.26.3 is not installed), probes <probe serial> and <probe serial>. Both boards were backed up first (main flash twice, equal) and restored and read back equal after.

**Question.** An earlier pass carried 83 G3519 examples to the MSPM33 and ran 40 with clean fault registers, but no run read what an example does. Do the examples do what they say, and does the reverse direction work?

**Setup.**

- Tool: `portability/everywhere/crossport_pair.py` (either direction). Swap the device header, unlock pins, drop the source chip's clock-tree lines, comment out each line SysConfig rejects, build with the target SDK's makefile. App `.c` files are copied as they are.
- Where the target SDK has no example by that name, the base project is the target's `empty` example.
- G3519 pins: PA8 to PA13 (console and CAN header), PA15 and PA25 (DAC to ADC wire), PA18, the 3.5 inch display pins and the LEDs are reserved as inputs in the configuration, so SysConfig cannot give them to a peripheral. No wire was moved.
- Check: `portability/everywhere/bench_pair_run.py` flashes, lets the app run 1 to 1.5 s, then reads CFSR and HFSR, the GPIO output registers 8 times 0.23 s apart, and the RAM variables or timer registers the test names. SPI and UART tests use the chips' internal loopback, so no wires.

**Expectation.** From the earlier passes: GPIO, UART and timers should run, SPI should run where the block exists on both sides, I2C and ADC should fail at build.

**Result.**

| Example | MSPM0G3519 to MSPM33 | MSPM33 to MSPM0G3519 |
|---|---|---|
| gpio_toggle_output | Works: 4 pins toggle | Works with 5 app lines edited (see below) |
| systick_periodic_timer | Works: pins toggle in the handler | Works |
| timx_timer_mode_periodic_sleep | Works | Works |
| timx_timer_mode_pwm_edge_sleep | Works: LOAD 1999, compare 500 and 1000 as configured | Works: same values |
| spi_controller_internal_loopback_poll | Works: gRxData 1 equals gTxData | Works |
| UART loopback (G3519: uart_rw_multibyte_fifo_poll, MSPM33: uart_tx_rw_multibyte_fifo_poll) | Works: rxPacket reads MSP! | Works: same |
| uart_internal_loopback_spgss0_uc1 and the interrupt version | no G3519 twin | Works: rxData 5 equals txData 5, pass LED pin high |
| crcp_calculate_checksum | Works: CRCOUT 0x2C81 | Works: 0x2C81 |
| UART standby, UART DMA console, UART RX | build stops (group alias, DMA instance, pull-up on open drain) | build stops |
| I2C controller | build stops: DL_I2C_ClockConfig unknown | build stops: 12 DL_I2CC_ names differ |
| ADC12 and HSADC | build stops: no ADC12 block | build stops: no HSADC block |

Builds with the app untouched: 8 of 12 in each direction. Silicon runs: 8 forward (7 checked, 1 needs a wire), 9 reverse (all checked).

**What needed more than the SysConfig move.**

- The UART examples expect a wire from TX to RX. Adding `UART1.enableInternalLoopback = true;` to the configuration (the app is untouched) lets them run without one. Cause tag: Setup or handling, the source example is written for a wire.
- The MSPM33 `gpio_toggle_output` names a port per LED (`GPIO_LEDS_USER_LED_2_PORT`) because its three LEDs sit on three ports. The G3519 LEDs share port B, so SysConfig writes one `GPIO_LEDS_PORT` and the unedited app does not build. Five lines changed. Cause tag: tool or docs, the generated names depend on the pin choice.
- The `BKPT` an example ends with sets HFSR bit 31 on the Cortex-M33 and nothing on the Cortex-M0+, which has no HFSR. A clean run reads zero in CFSR on both.

**The line.** I2C and ADC do not move: the MSPM33's I2C is the UNICOMM block with new function names and its ADC is the HSADC block, so they need a rewrite, not a port.

**Not checked.** The LEDs and the PWM pins were not watched and the PWM frequency was not measured; the checks are register reads. I2C and ADC need a rewrite, not a port. No example here touched flash, NONMAIN or the display wires.

**Second attempt, the CC1407P host driver.** TI's host driver (7 files, unmodified) plus this repo's bare HAL and a fake radio, built as a normal SDK image for both boards (`portability/cc1407p-host/build_board.sh`). On both, the first SPI message the driver builds is `00 04 11 00 02 32 00 00 E9 71`, the same 10 bytes as the PC reference, and the CRC-16 check value is 0x29B1. The 9 structs that go on the wire have the PC sizes; two API structs are smaller on both Arm cores (1-byte enums) and are not sent. 10.8 kB of flash on the MSPM33 and 10.6 kB on the G3519, 5.8 kB of RAM. No radio was attached, so no packet was sent from either board.

**Board state.** Main and data flash read back equal to the saved copies on both boards after the runs.
