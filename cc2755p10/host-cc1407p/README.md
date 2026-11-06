# Does the plain-C CC1407P host run on the LP-EM-CC2755P10?

Answer: the host side runs on the chip, and with no radio fitted it stops where TI's driver waits for the radio's first reply. Nothing was sent over the air.

Terms: [docs/vocab.md](../../docs/vocab.md). Errors and fixes: [FAQ.md](../FAQ.md). The portable part (TI's driver, `hal_bare.c`, `app_tx.c`) is in `portability/cc1407p-host/`. This folder holds what is specific to this board: `hb_p10.c` (the seven `hb_*` hooks on TI Drivers: SPI in callback mode, GPIO for chip select, reset and SPI_INT, a one-shot clock for the 30 microsecond ready poll, the CPU cycle counter for delays), `empty.c` (runs the transmit sequence once and prints the hook counters on the backchannel UART at 115200 baud), `empty.syscfg` and `app_tx_nostart.c` (`app_tx.c` without its stub start-up code, plus a stage counter). Checked 2026-10-24 on one LP-EM-CC2755P10, F3 SDK 9.21.00.36 (NoRTOS), TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0. 1 kB = 1,024 bytes.

## Pins (header number, DIO)

SCLK 7 (DIO3), POCI 14 (DIO4), PICO 15 (DIO5), CS 18 (DIO7), RESET 36 (DIO16, the red LED), SPI_INT 10 (DIO28, wired from BoosterPack pin 23). The red LED jumper must be off when a radio is fitted. Same map as `ports/cc1407p-zephyr/boards/lp_em_cc2755p10.overlay`.

## Build and run

```bash
REPO=<repo root> bash cc2755p10/host-cc1407p/build.sh <work folder>
```

The image is `<work folder>/ticlang/empty.hex`; `build.sh` ends by checking its CCFG and SCFG against the board's known-good files. Flash it with DSLite as in `cc2755p10/AGENTS.md`, never with a chip erase. The program prints `X45 start spi_open=1`.

## Result with no radio fitted

The image links at 31.8 kB of code, 8 B of data and 16.2 kB of RAM (it includes the SDK's drivers). Hook counters read from RAM with the debugger: RESET asserted 1, SPI transfers started 1, completed 1, failed 0, CS asserted 1 and released 2, one-shot timer started 2 and fired 2, SPI_INT enabled 1 and fired 0, POCI read once and low. The program then stays inside `TRX_Host_open`: TI's driver ends that call with `while(TRX_HOST_IS_PENDING(handle));`, so a host with no radio never leaves it. No fault (CFSR and HFSR read 0). The line `X45 tx_once returned` never prints.

If you see `X45 start` and nothing after it, the radio is not answering: check the BoosterPack, the red LED jumper and the wire from pin 23 to pin 10, or add a timeout around the open call.
