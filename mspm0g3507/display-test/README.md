# Waveshare 3.5inch Capacitive Touch LCD on the TI LP-MSPM0G3507

It works: 12 jumper wires (13 if you read the screen back), a 7.4 kB program, a full-screen redraw in 0.36 s at 8 MHz SPI and 67 ms at 40 MHz SPI (that speed needs an 80 MHz CPU clock set with a SysConfig clock-tree change and 16-bit frames, and only 64 pixels of one row were read back), and touch read from the FT6336U touch chip. Two traps cost most of the time. Chip select needs a pause between commands, because the module's level translator loses short pulses. And the touch chip answered only to software I2C, not the chip's I2C block.

Terms: [vocab](../../docs/vocab.md) and [FAQ](../FAQ.md). The screen's driver chip is the ST7796S, the touch chip is the FT6336U, and the level translator is the TXS0108E, all on the Waveshare module. Checked 2026-10-06 on an Intel Mac, macOS 15.7, MSPM0 SDK 2.11.00.07, SysConfig 1.26.3, TI Arm Clang 5.1.1, OpenOCD xPack 0.12.0. Apple Silicon was not tested. Sizes in kB, 1 kB = 1,024 bytes.

Contents: [You need](#what-you-need), [Wiring](#wiring), [Build, flash and read results](#build-flash-and-read-results), [Results](#results), [Performance](#performance-how-fast-it-can-go), [Start-up time](#start-up-time), [Trap 1](#trap-1-the-screen-stays-dark-or-shows-static-even-though-every-wire-is-right), [Trap 2](#trap-2-the-touch-chip-does-not-answer-the-i2c-block-but-does-answer-software-i2c), [Mistakes to avoid](#mistakes-to-avoid).

## What you need

- A TI LP-MSPM0G3507 LaunchPad development kit. The one used here has probe serial `MG350001`; yours will differ.
- The Waveshare 3.5inch Capacitive Touch LCD (320x480) and 13 female-to-female jumper wires.
- MSPM0 SDK, SysConfig and TI Arm Clang installed where `build.sh` expects them (the `SDK`, `CC` and `SYSCFG` lines at its top; change them if yours are elsewhere).
- OpenOCD with a connection file for the board. This repo uses Zephyr's `boards/ti/lp_mspm0g3507/support/openocd.cfg`.
- Python 3 to decode the speed results.

## Wiring

12 wires, no jumpers moved. MISO is an optional 13th wire (J2 pin 14, PB7): wire it only if you want to read the screen's status back, which is how trap 1 was found. Power goes to the 3.3 V pin. Waveshare says supply and signal voltage must match, and this chip's pins are 3.3 V. 5 V was not run.

| Panel pin | LaunchPad header pin (chip pin) | Note |
|---|---|---|
| VCC | 3V3 | 3.3 V supply |
| GND | any GND | |
| SCLK | J1 pin 7 (PB9, SPI1 clock) | Standard-drive pin, so SPI1 here is capped at 16 Mbit/s by the data sheet |
| MOSI | J2 pin 15 (PB8, SPI1 data out) | |
| MISO | J2 pin 14 (PB7, SPI1 data in) | Optional, read-back only |
| LCD_CS | J2 pin 13 (PB6, SPI1 chip select 0) | |
| LCD_DC | J4 pin 39 (PB1) | Plain GPIO |
| LCD_RST | J4 pin 40 (PB4) | Plain GPIO |
| LCD_BL | J4 pin 38 (PA28) | Backlight, GPIO high or PWM |
| TP_SDA | J1 pin 10 (PB3, I2C1 data) | Used as software I2C, see trap 2 |
| TP_SCL | J1 pin 9 (PB2, I2C1 clock) | |
| TP_INT | J2 pin 11 (PB16) | GPIO input |
| TP_RST | J2 pin 12 (PB0) | GPIO output |

Pins to avoid: PA0 (red LED), PB22, PB26 and PB27 (RGB LED), PA18, PB23 and PA9 (buttons), PB24 (thermistor), PA10 and PA11 (backchannel UART). The board's I2C pull-ups for PB2 and PB3 (R59, R60) are not fitted; the software I2C here uses the chip's internal pull-ups. SPI1 and I2C1 use the same pins as TI's own `spi_controller_multibyte_fifo_poll` and `i2c_controller_rw_multibyte_fifo_poll` examples.

## Build, flash and read results

Build every variant into `out/`. Run this in this folder:

```bash
./build.sh
```

It prints, for each build, the address of the `res` results struct (from `tiarmnm`) and the program size. Flash the demo:

```bash
openocd -s <openocd-scripts-folder> -f <openocd.cfg> -c "adapter serial <probe-serial>" -c init -c "reset halt" -c "program out/display_8mhz/display_8mhz.out verify" -c "reset run" -c shutdown
```

Replace `<openocd-scripts-folder>` with OpenOCD's `scripts` folder, `<openocd.cfg>` with the connection file path, and `<probe-serial>` with your kit's probe serial (the text after `usbmodem` in `ls /dev/cu.usbmodem*`, minus the last digit). On this bench they were `~/zephyr-toolchain/xpack-openocd-0.12.0-7/openocd/scripts`, `~/zephyrproject/zephyr/boards/ti/lp_mspm0g3507/support/openocd.cfg` and `MG350001`. `show.sh` in this folder is a working example for the picture screens.

The demo fills the screen red, green, blue, white and black, draws eight color bars, then paints a white dot wherever you touch and toggles the red LED.

To read the speed results, flash `out/perf/perf.out` the same way, then dump its `res` struct after it has run. The `res` address and size come from `tiarmnm -S out/perf/perf.out` (here: `0x20200280`, 520 B). Reading restarts the program, so do it in one OpenOCD connection with a pause:

```bash
openocd -s <openocd-scripts-folder> -f <openocd.cfg> -c "adapter serial <probe-serial>" -c init -c "reset halt" -c "reset run" -c "sleep 10000" -c halt -c "dump_image mine.bin <res-address> <res-size>" -c shutdown
```

Decode it:

```bash
python3 perf.py mine.bin
```

For the 80 MHz build (`out/perf80/perf80.out`) add the two SPI speeds: `python3 perf.py mine.bin 20,40`. This read command is assembled from the flash command above and the `dump_image` form used elsewhere in this repo; it was not re-run as written.

| File | What it is |
|---|---|
| `display.syscfg`, `gen/` | SysConfig pins: SPI1 3-wire at 8 MHz, I2C1 100 kHz, GPIO for DC, reset, CS, backlight, touch reset and interrupt |
| `display80.syscfg`, `gen80/` | The same at an 80 MHz CPU clock |
| `display_test.c` | The demo, plus the `-DREADBACK`, `-DEXPERIMENT` and `-DDIAG` diagnostics |
| `perf.c`, `perf.py` | Speed test and its decoder |
| `probe.c` | Wire check: each pin read with the chip's pull-up, then pull-down. A loose wire follows the pull |
| `bltest.c`, `bloff.c` | Backlight blink and backlight off |
| `photo.py`, `show.sh` | Turn an image into a screen build and flash a saved one |

## Results

| What | Measured | Source |
|---|---|---|
| Full-screen fill, 300 kB, 8 MHz SPI | 364.9 ms, 84% of the 307 ms the clock allows | perf results struct (32 MHz CPU) |
| Same at 16 MHz | 182.5 ms, 84% of 154 ms | perf results struct (32 MHz CPU) |
| Same at 1 MHz | 2.92 s | observed 2026-10-06; not saved |
| Screen ID read back | `0x7796` (ST7796S), register D3 over MISO | observed 2026-10-06; not saved |
| Touch chip | vendor `0x11` (FocalTech), chip `0x64` (FT6336U), firmware `0x12`, registers A8, A3, A6 | observed 2026-10-06; not saved |
| Touch | dots drawn under the finger, LED toggles per touch | observed 2026-10-06 by the person at the board; not saved |
| Program size | 7.4 kB of flash, 648 B of RAM | `tiarmsize` on the built `display_8mhz.out` |

Colors were not checked by eye at 16 MHz or faster; the panel's data sheet allows about 15 MHz.

## Performance: how fast it can go

Measured 2026-10-06 with `perf.c` in three builds: 32 MHz CPU, 32 MHz CPU with short start-up waits, and 80 MHz CPU. `perf.py` decodes each results struct. Every speed was checked by reading back 64 pixels of one row from the screen's own memory over MISO: 64 of 64 matched at every speed below. That is a spot check, not the whole screen.

| CPU clock | SPI clock | Method | Full screen | Full screens per second | 100x100 box |
|---|---|---|--:|--:|--:|
| 32 MHz | 8 MHz | CPU, 8-bit frames | 364.9 ms | 2.7 | 23.8 ms |
| 32 MHz | 16 MHz | CPU, 8-bit frames | 182.5 ms | 5.5 | 11.9 ms |
| 32 MHz | 16 MHz | CPU or DMA, 16-bit frames | 168.1 ms | 6.0 | 11.0 ms |
| 80 MHz | 20 MHz | CPU or DMA, 16-bit frames | 134.4 ms | 7.4 | 8.8 ms |
| 80 MHz | 40 MHz | CPU or DMA, 16-bit frames | 67.2 ms | 14.9 | 4.4 ms |

- **The wire is the limit.** Every method reaches 84 to 92% of what the SPI clock allows. DMA (a block that copies data without the CPU) is no faster than the CPU loop, but it leaves the CPU free while the screen fills. The free-time measure over-counts, so no percentage is given.
- **40 MHz worked on the standard pins** (with an 80 MHz CPU clock, 16-bit frames and the clock-tree change below). The chip's data sheet promises more than 16 Mbit/s only on its high-speed pins, and the panel's data sheet (ST7796S, 66 ns minimum write cycle) allows about 15 MHz writes. 40 MHz is about 2.7 times that. Only 64 pixels of one row were read back at 40 MHz. Not checked: every pixel, the screen by eye at 40 MHz, longer wires and a second module. So the margin may not hold elsewhere.
- **16-bit frames need the SPI block switched off to change.** Setting the frame size while SPI is enabled did nothing, silently. The `DL_SPI_setDataSize` documentation does not say so.
- **80 MHz needs the clock tree, not the default line.** `display80.syscfg` is a copy of TI's `sysctl_mclk_syspll` clock tree. With `SYSCTL.forceDefaultClkConfig` left in, SysConfig ignored every clock setting and stayed at 32 MHz. SysConfig gave no warning.

### Start-up time

| | Power-up to screen ready | To first full frame |
|---|--:|--:|
| Usual init (120 ms waits after reset, software reset and sleep out) | 370 ms | 538 ms at 16 MHz; 461 ms seen once, not saved |
| Shortest waits the ST7796S data sheet allows (`-DFAST_BOOT`) | 12 ms | 180 ms at 16 MHz, 79 ms at 40 MHz SPI with the 80 MHz CPU clock |

The chip itself starts in 0.26 to 0.31 ms (data sheet SLASEX6C, cold start-up). Almost all of the usual 370 ms is waits copied from common drivers. The screen's power-mode register read `0x9C` (awake, display on) after the fast init too.

### Running code from external memory

Not possible on this chip. The MSPM0G3507 data sheet lists no external memory bus or quad-SPI interface, so code must fit in its 128 kB of flash. External SPI flash or the display's SD slot can hold pictures, fonts and logs, streamed in over SPI. The LP-MSPM0G3519 has 512 kB of flash and 128 kB of RAM.

### LVGL on this screen

Yes, trimmed. LVGL (an open-source graphics library) runs on this chip with this screen: 25.1 kB of RAM, 125.4 kB of flash at 32 MHz (126 kB at 80 MHz), and 7 refreshes per second at 32 MHz and 15 to 19 at 80 MHz. A refresh redraws only the changed areas (about 12,000 to 25,000 pixels), so it is not a full-screen update; the full-screen figures above are for filling all 153,600 pixels. Details: [lvgl-g3507](../lvgl-g3507/README.md).

## Trap 1: the screen stays dark or shows static, even though every wire is right

Measured: each command sent alone took effect; sent back to back, the second was lost. The code raised chip select for a few hundred nanoseconds between two commands, and "display on" was always the second one, so the screen never turned on. Status reads over MISO showed it. A 10 µs pause after chip select goes high fixed it (`cs_gap()` in `display_test.c`). Slowing the SPI clock does not help, because the pulse is between bytes, not in them.

 The module puts a TXS0108E level translator between the header and the screen, and the suspect is that it does not pass a pulse that short, so the screen saw two commands as one. Not checked: the pulse on a scope, and a module without the translator.

## Trap 2: the touch chip does not answer the I2C block, but does answer software I2C

 With the chip's I2C1 block, every read of address 0x38 failed: first not acknowledged, later the controller reported the bus busy. The same two pins driven as plain open-drain GPIO, bit by bit at about 100 kHz, got an acknowledgement and correct registers every time, so the demo uses software I2C. The suspect is the same level translator on an open-drain bus.

## Mistakes to avoid

- Calling `DL_GPIO_initPeripheralInputFunctionFeatures` to add a pull-up rewrites the whole pin register and clears the open-drain setting that SysConfig's code sets next with `DL_GPIO_enableHiZ`. Call `DL_GPIO_enableHiZ` again after it. Fixing that alone did not make the I2C block work here.
- Switch an output pin off before reading it back in a wire check, or a good wire looks wrong (`probe.c` does).

How the faults were found without a scope: the probe showed every display wire connected and the unwired control pin floating; the backlight test gave a yes or no a person can see; and wiring MISO let the chip read the screen's ID and power-mode register after every step (`-DEXPERIMENT`), which isolated the dropped commands in one run.
