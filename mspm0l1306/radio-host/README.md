# CC1407P host driver on an LP-MSPM0L1306

TI's CC1407P host driver (plain C, from its Zephyr fork) with this repo's bare-metal layer, linked and flashed on the LP-MSPM0L1306. The same layer ran with a radio on the LP-MSPM0G5187 (`mspm0g5187/radio/README.md`). Run 2026-10-24 with no BP-CC1407P-9-FE attached, so the radio link itself is untested here.

Terms: [docs/vocab.md](../../docs/vocab.md). 1 kB = 1,024 bytes.

## Result

| What | Value |
|---|---|
| Image with the driver, HAL, UART and 2-GFSK 50 kbps settings | 14,400 B (14.1 kB) flash, 2,277 B (2.2 kB) RAM (4 buffers of 156 B), 256 B stack: 2.5 kB of the 4 kB |
| Same with the SPI trace on | 15,672 B (15.3 kB) flash |
| With TI's 24 buffers | does not fit (5,244 B (5.1 kB) needed against 4,096 B (4.0 kB), `portability/cc1407p-host/README.md`) |
| First SPI frame after reset, with trace on | `00 04 11 00 02 32 00 00 e9 71`, byte for byte the frame the LP-MSPM0G5187 sent to the radio in its recorded link |
| Reply with no radio | all `ff` (the POCI line reads high); the G5187 with no radio read all `00` |
| After that frame | stops inside the driver's wait for the radio (halted in `hb_delay_us` in the SPI-complete handler, 3 s after reset) |

## Pins

| Signal | Pin |
|---|---|
| SCLK | PA6 (SPI0) |
| PICO | PA5 |
| POCI | PA4 |
| CS_N | PA3 |
| RESET_N | PA7 |
| SPI_INT | PA2 |
| Trace UART0 | PA8 TX, PA9 RX, 115200 (jumpers J16 and J17 on XDS) |

These are SPI0's default pins in TI's SPI example for this board. Wire to a BP-CC1407P-9-FE as in the G5187 README (radio pins 7, 15, 14, 18, 36, 23, plus 3V3 and GND); not wired for this run.

## Build and run

```text
FRL_SYSCONFIG=<sysconfig_cli.sh> mspm0l1306/radio-host/build.sh -DHB_TRACE
PROBE=<serial> mspm0l1306/tools/flash_and_capture.py mspm0l1306/radio-host/out/radio_host.out trace.log Ct 8
```

`build.sh` copies TI's `spi_controller_echo_interrupts` for this board, replaces its `.c` with `mspm0g5187/radio/radio_host.c` and its `.syscfg` with `radio.syscfg`, and links the driver (buffer pool cut to 4 by `NBUF`), `portability/cc1407p-host/hal_bare.c` and `hooks_mspm0l1306.c`. The banner still says LP-MSPM0G5187 because the application file is shared.
