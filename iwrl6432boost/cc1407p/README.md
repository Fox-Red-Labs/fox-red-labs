# IWRL6432BOOST driving a BP-CC1407P-9-FE sub-1 GHz radio: build and wiring

Terms: [docs/vocab.md](../../docs/vocab.md). 1 kB = 1,024 bytes.

**Status 2026-11-10: built, not flashed, not run.** The radar chip's Cortex-M4F is meant to be the host of TI's CC1407P radio driver, so one board senses and transmits (the target is a roadside speed sign that sends speed and car counts to a CC1354P10 hub indoors). TI publishes no example of this pairing. What exists: a wiring plan read from TI's documents, a transmit app that builds, and the numbers below. Nothing has been sent: the flash write is held until a restore path is confirmed (the radar's shipped image cannot be read back), and the eight wires are not on.

## What the app does

`radio_tx.c` opens TI's host driver, erases the radio's stored settings, stores the same device config and 50 kbps 2-GFSK settings as `mspm0g5187/radio/radio_tx.c` (915 MHz, high power amplifier at its top level, no whitening, packet = length byte plus payload), then sends a 20-byte payload once a second: `FRL n=000012 RADAR-1`. Progress and the radio's status go to the radar's USB serial port ending in 1, at 115200. The receiver is TI's `rfPacketRx` on the LP-CC1354P10-1 as in [cc1354p10/radio-rx](../../cc1354p10/radio-rx/README.md).

## Build

```bash
iwrl6432boost/cc1407p/build.sh                       # builds radio_tx.release.out under <scratch>/radar-cc1407p-build
```

Needs MMWAVE-L-SDK 05.05.04.02 in `~/ti`, TI Arm Clang and SysConfig 1.28.0 (the paths of `portability/radar-family/build/build.sh`). The script copies the SDK's `empty` no-RTOS example, drops in `example.syscfg`, `radio_tx.c` and `hooks_iwrl6432.c`, adds TI's host driver and `portability/cc1407p-host/hal_bare.c`, and runs the SDK's makefile with the Unix helpers passed in. The boot image is made as in `portability/radar-family/build/` (`rprc.sh`, `appimage.sh`, Docker).

| File | What it is |
|---|---|
| `example.syscfg` | McSPI A (CLK pad D10, MOSI B12, MISO C11, CS0 D11), reset on pad K11 (GPIO4), SPI_INT on pad J10 (GPIO5), console on UARTB (E10, F11) |
| `hooks_iwrl6432.c` | The seven `hb_*` functions on the SDK's MCSPI, GPIO, pinmux and ClockP calls. Everything runs from the main loop (`hb_poll()` does what the interrupts would); MISO is read as a level by flipping pad C11 to GPIO1 while chip select is low |
| `radio_tx.c` | The transmit app |
| `build.sh` | The build |
| `img/` | The wiring pictures (`radio-end.png`, `radar-end.png`) and their specs |

## Sizes (built 2026-11-10)

| | Bytes |
|---|---|
| Boot image (`.appimage`, with the SDK's RF patch) | 47,876 |
| Code and constants (text) | 42,360 |
| Initialized data | 1,598 |
| bss (RAM) | 24,280 |

Rows: `host_fit`, variants `cc1407p-radar-tx-*`, in `data/results/iwrl6432boost.csv`. The text includes the SDK's UART console and `printf`; TI's host driver and this repo's layer alone are 10.4 kB of flash (`portability/cc1407p-host/README.md`).

## Wiring (every pin from a TI document)

The radar board brings McSPI A, the chip's SPI port A, to a 1x7 header J2 (`SPI_HEADER`, top edge) and to its BoosterPack sockets J8 and J9 (underside). The board's BoosterPack sockets carry no 3.3 V or 5 V out of the box (J8 pins 1 and 2 go to capacitors; the 0 ohm links to the board rails are not fitted), so power comes from J2.

| Radio pin (BP-CC1407P-9-FE) | Signal | Radar board pin | Radar net | Chip pad (ball) | Source |
|---|---|---|---|---|---|
| 1 | 3V3 | J2 pin 1 | VCC_3V3 | none | schematic sheet 15 |
| 20 | GND | J2 pin 7 | GND | none | sheet 15 |
| 7 | SPI_SCLK | J2 pin 5 (also J8 pin 13) | DCA_LP_SPI_CLK | AG (D10), MCSPIA_CLK | sheets 6, 10, 15 |
| 15 | SPI_PICO | J2 pin 2 (also J9 pin 12) | DCA_LP_SPI_MOSI | AI (B12), MCSPIA_MOSI | sheets 6, 15 |
| 14 | SPI_POCI | J2 pin 3 (also J9 pin 14) | DCA_LP_SPI_MISO | AJ (C11), MCSPIA_MISO | sheets 6, 10, 15 |
| 18 | SPI_CS_N | J2 pin 4 (also J9 pin 6) | DCA_LP_SPI_CS | AH (D11), MCSPIA_CS0 | sheets 6, 10, 15 |
| 23 | SPI_INT | J2 pin 6 (also J9 pin 16) | DCA_LP_HOST_INTR_1 | AV (J10), GPIO5 | sheets 6, 10, 15 |
| 36 | RESET_N | J9 pin 1 | nERROR_LED_WD_TP | AU (K11), GPIO4 | sheets 6, 14, 15 |

Sources: BP-CC1407P-9-FE pins from SWRU633A table 2-1 (1 3V3, 7 SCLK, 14 POCI, 15 PICO, 18 CS_N, 20 GND, 23 SPI_INT, 36 RESET_N; supply 1.8 to 3.6 V); the radar's nets from TI's PROC117B schematic SWRR180 (sheet 6 chip pins, sheet 10 SPI/I2C mux and switch table, sheet 14 nERROR LED, sheet 15 connectors J2, J8, J9); pad functions from `tools/syscfg_deviceData/XWRL64XX/XWRL64XX.json` in MMWAVE-L-SDK 05.05.04.02. The pin functions agree with TI's own `mcspi_loopback` example (CLK D10, MOSI B12, MISO C11, CS D11).

Needs: S1 position 6 ON (SPI on the muxed nets; the mux table on sheet 10) and S1 position 3 ON (connects the nERROR net to J9 pin 1), both as shipped on the first unit. Reset is the nERROR net: its red LED D9 lights while reset is held low, harmless. Pads D10, C11, D11 and K11 are not used by the shipped Presence demo's console (UARTB on E10 and F11); whether the demo's own code touches nERROR or SPI A is not checked.

J2 orientation: TI's assembly drawing (PROC117B(001_IWR)_Assy.PDF in SWRR180, page 1) carries the pad names PAJ201 to PAJ207 at increasing y toward J5's pads (`pdftotext -bbox`), so pin 7 (GND) is the end beside the USB port J5 and pin 1 (3.3 V) is the far end, by C65 and R157/R158: the left end in `../img/top.jpg`. A meter beep from the GND test point TP11 to pin 7 is an optional check.

### Not confirmed from a TI document

- **The radio's current from the radar's 3.3 V rail.** SWRU633A gives only the 1.8 to 3.6 V range. The CC1407P data sheet gives 47 mA at +10 dBm and 134 mA at +22 dBm on its high-power amplifier. The rail is the radar board's 3.3 V buck through a ferrite bead (L11, BLM15PD300SZ1D); the bead's rating was not read. If the radar resets when the radio transmits, lower the power level first.
- **Whether the shipped demo uses the nERROR pad or SPI A.** Not checked in its source.

## What is not done

The flash write, the receive count, and the second build that keeps the radar demo running and sends per-frame packets. Experiment page: [docs/experiments/2026-11-10-iwrl6432boost-cc1407p-radio.md](../../docs/experiments/2026-11-10-iwrl6432boost-cc1407p-radio.md).
