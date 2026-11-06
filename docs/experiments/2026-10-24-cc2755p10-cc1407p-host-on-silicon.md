<!-- experiment
id: E266
board: LP-EM-CC2755P10
date: 2026-10-24
status: line found
reached: run
line: TI's CC1407P host driver, this repo's bare-metal HAL and a hook file written for the LP-EM-CC2755P10 run on the chip: reset pulse, one 12 MHz SPI transfer, timer and interrupt hooks all fire, and then TRX_Host_open waits for a radio that is not fitted. The same wait stops TI's four Zephyr CC140xP samples. No packet was sent: the BP-CC1407P was not on this board.
stop: no-radio
-->
# Does the CC1407P host driver run on the LP-EM-CC2755P10, as plain C and as TI's Zephyr samples?

**Short answer.** The host side runs; the radio side was not tested. A no-RTOS build with this repo's HAL runs on the chip and exercises every hook, then waits in `TRX_Host_open`, which has no timeout, for a reply from a radio that is not connected. TI's four Zephyr samples (rf_diagnostics, genfsk_tx, genfsk_rx, carrier_wave) built, flashed and started; only rf_diagnostics prints a line.

Terms: [vocab](../vocab.md). FAQ: [LP-EM-CC2755P10](../../cc2755p10/FAQ.md). Code: `cc2755p10/host-cc1407p/` and `portability/cc1407p-host/`. Checked 2026-10-24, one LP-EM-CC2755P10 on an LP-XDS110, TI's Zephyr fork v3.7.0-ti-1.00.01_ga, F3 SDK 9.21.00.36.

**Question.** The earlier plain-C and Zephyr builds only compiled. What happens when they run on the chip?

**Setup.**

- Plain C: TI's driver files unmodified, `hal_bare.c`, `app_tx.c` without its stub start-up code, and `hb_p10.c` (SPI, GPIO, a one-shot timer and an interrupt on TI Drivers). Pins from `ports/cc1407p-zephyr/boards/lp_em_cc2755p10.overlay`: SCLK DIO3, POCI DIO4, PICO DIO5, CS DIO7, RESET DIO16, SPI_INT DIO28. No radio connected.
- Zephyr: the four samples built with that overlay and no shield, flashed as hex files carrying the board's own CCFG and SCFG.
- The plain-C image counts every hook call; the counters were read from RAM with the debugger.

**Expectation.** With no radio the driver cannot get a reply. A reset pulse, a chip-select transfer and a timeout would be a good sign for the hooks.

**Result.**

| Counter (read from RAM) | Value |
|---|---|
| RESET asserted | 1 |
| SPI transfers started, completed, failed | 1, 1, 0 |
| CS asserted, released | 1, 2 |
| SPI_INT interrupt enabled, fired | 1, 0 |
| One-shot timer started, fired | 2, 2 |
| POCI reads, read high | 1, 0 |
| Where it stands | stage 1: inside `TRX_Host_open`, which loops until the radio answers; no fault (CFSR and HFSR 0) |

The Zephyr samples: rf_diagnostics printed `RF Diagnostics Example` and nothing more; genfsk_tx, genfsk_rx and carrier_wave printed nothing, not even the Zephyr boot line. That fits the same wait in the driver with the log thread starved, but it was not confirmed by a debugger.

**The line.** No radio. `TRX_Host_open` (`third-party/ti-cc140xp-host/host/trx_host.c`) ends with `while(TRX_HOST_IS_PENDING(handle));`, so a host with no radio, or one that never answers, hangs there. A host that must survive a missing radio needs its own timeout around the open call.

**Ask.** The BP-CC1407P on the LP-EM-CC2755P10: red LED jumper off and one wire from BoosterPack pin 23 to pin 10 (`cc2755p10/FAQ.md`). Then the transmit sample can be paired with the CC1354P10 receiver that already hears the radio.
