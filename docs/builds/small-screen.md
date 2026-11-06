# Put colors and touch on a small screen

Wire a 3.5 inch touch screen to a LaunchPad kit with 11 jumper wires and run a 7.4 kB program that paints colors, bars and a dot under your finger.

The page: [site/project-small-screen.html](../../site/project-small-screen.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Colors, bars and touch dots on a 3.5 inch screen

The LP-MSPM0G3507 driving the Waveshare touch screen over 11 jumper wires (12 with the optional touch INT wire) with a 7.4 kB program from this repository.

Status: Works. Boards: g3507.

Folders this version uses (in a sparse clone, fetch them first, then read `mspm0g3507/AGENTS.md`):

```bash
git sparse-checkout add mspm0g3507 ports/bitnetmcu
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM0G3507 LaunchPad development kit | 1 | the board |
| Waveshare 3.5 inch capacitive touch LCD (ST7796S screen, FT6336U touch) | 1 | the screen and touch panel |
| ELEGOO jumper wires (120-piece pack) | 12 female-to-female as tested (11 are required); 13 with the optional MISO wire | the wires from the screen to the LaunchPad header |
| Micro-USB data cable | 1 | power and programming |

### Before you start

- A Mac with the MSPM0 SDK 2.11.00.07 (0.4 GB download, 2.2 GB installed), TI Arm Clang 5.1.1, which comes inside CCStudio 21.0.1 (1.9 GB download, 8.1 GB installed, free myTI login), and xPack OpenOCD 0.12.0-7 (2 MB). Checked on an Intel Mac on macOS 15.7 only; Apple Silicon was not tested. The SysConfig output the program is built from is in the repository (`gen/`). You only need SysConfig 1.26.3 (0.96 GB installed) if you change pins.
- OpenOCD needs a connection file for the board. The repository has one, `ports/bitnetmcu/openocd-mspm0.cfg`: the same three lines as Zephyr's `boards/ti/lp_mspm0g3507/support/openocd.cfg`, so Zephyr is not needed.
- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`. Take the board folder alone with the sparse-checkout line in its README, or clone the whole repository.
- Your LaunchPad kit's debug probe serial number: the text after `usbmodem` in `ls /dev/cu.usbmodem*`, minus the last digit, or the `Serial Num:` line of `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e`.

### Wiring

The 40-pin edge is four 10-pin headers. Hold the board with the USB connector at the top: J1 (pins 1 to 10) and J3 (21 to 30) are the two left rows, J4 (31 to 40) and J2 (11 to 20) the two right rows. Pin 1 of J1 and pin 21 of J3 are the top pins of the left rows, nearest the USB connector; J4 and J2 count the other way, so pin 40 and pin 20 are at the top. A diagram drawn on TI's board drawing, checked against the SysConfig pin output this demo is built with (`mspm0g3507/display-test/gen/ti_msp_dl_config.h`). Signal and supply are both 3.3 V: the module's level translator takes the LaunchPad kit's 3V3, and nothing is driven at 5 V. No jumper on the LaunchPad moves.

| Module pin | Board pin | What it is | Needed |
|---|---|---|---|
| VCC pin (not the 3V3 pin) | J1 pin 1 (3V3) | power, 3.3 V only | required |
| GND | J2 pin 20 (GND) | ground | required |
| SCLK | J1 pin 7 (PB9) | screen clock, SPI1 | required |
| MOSI | J2 pin 15 (PB8) | screen data in, SPI1 | required |
| LCD_CS | J2 pin 13 (PB6) | screen chip select | required |
| LCD_DC | J4 pin 39 (PB1) | command or data | required |
| LCD_RST | J4 pin 40 (PB4) | screen reset | required |
| LCD_BL | J4 pin 38 (PA28) | backlight on | required |
| TP_SDA | J1 pin 10 (PB3) | touch data, software I2C | required |
| TP_SCL | J1 pin 9 (PB2) | touch clock, software I2C | required |
| TP_INT | J2 pin 11 (PB16) | touch interrupt. The program reads it once at start for a diagnostic value and finds touches by polling the touch chip, so the demo does not need it. Wired in the tested setup. | optional |
| TP_RST | J2 pin 12 (PB0) | touch reset | required |
| MISO | J2 pin 14 (PB7) | screen read-back; only to diagnose a dark screen | optional |
| 3V3 pin and SD_CS | not connected | the module's own 3V3 pin, which stays empty, and its SD card select | leave off |

### Steps

1. **Mac, terminal.** Install the tools once, all free. Get CCStudio 21.0.1 from [TI's CCStudio download page](https://www.ti.com/tool/download/CCSTUDIO) (1.9 GB download, 8.1 GB installed, free myTI login). It brings TI Arm Clang 5.1.1. Get the MSPM0 SDK 2.11.00.07 from [TI's MSPM0 SDK download page](https://www.ti.com/tool/download/MSPM0-SDK) (0.4 GB download, 2.2 GB installed). Then get xPack OpenOCD 0.12.0-7 from [its GitHub release page](https://github.com/xpack-dev-tools/openocd-xpack/releases/tag/v0.12.0-7) (2 MB; the file `xpack-openocd-0.12.0-7-darwin-x64.tar.gz` on an Intel Mac, `...darwin-arm64.tar.gz` on Apple Silicon) and unpack it in your home folder. Clone this repository into `<repository folder>`.

   You should see: `/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin/tiarmclang --version` prints 5.1.1, and the SDK folder `/Applications/ti/mspm0_sdk_2_11_00_07` exists.

   If your tools live somewhere else, change the `SDK` and `CC` lines at the top of `build.sh` in step 3.

2. **Bench.** With the LaunchPad unplugged, run the wires in the table above from the display to the header, power from the 3V3 pin only. Leave the display's own 3V3 and SD_CS pins empty. A 13th wire, MISO, is optional and only for reading the screen's status back.

   You should see: Every wire sits on the header row the picture shows; nothing is plugged into the display's 3V3 or SD_CS.

   If you are not sure a wire is on the right pin, count from the header's pin 1, the corner nearest the USB connector at J1. Check it against the table before plugging in.

3. **Mac, terminal.** From the repository folder, build every variant of the display test into `out/`. Replace `<repository folder>` with the folder you cloned.

   ```text
   cd <repository folder>/mspm0g3507/display-test
   ./build.sh
   ```

   You should see: For each build, a line with the address of its `res` results struct and the program size. The demo, `out/display_8mhz/display_8mhz.out`, is 7.4 kB of flash and 648 bytes of RAM.

   If the compiler or SDK is not found, change the `SDK` and `CC` lines at the top of `build.sh`.

4. **Mac, terminal.** Flash the 8 MHz demo with OpenOCD. Replace `<probe serial>` with your probe's serial number (read it with `xdsdfu -e`), and `<repository folder>` with your clone. The first command sets `OCD` to the folder you unpacked OpenOCD into.

   ```text
   export OCD="$HOME/xpack-openocd-0.12.0-7"
   openocd -s "$OCD/openocd/scripts" -f <repository folder>/ports/bitnetmcu/openocd-mspm0.cfg -c "adapter serial <probe serial>" -c init -c "reset halt" -c "program out/display_8mhz/display_8mhz.out verify" -c "reset run" -c shutdown
   ```

   You should see: OpenOCD finishes with the program verified, and the screen starts the demo: it fills red, green, blue, white and black, then draws eight color bars.

   If OpenOCD reports a CRC mismatch after the program step, see [the OpenOCD CRC mismatch entry](../../site/board-mspm0g3507.html#openocd-reports-a-crc-mismatch-after-program-file-verify-but). If the screen stays dark or shows static with every wire right, see [the screen stays dark entry](../../site/board-mspm0g3507.html#the-screen-stays-dark-or-shows-static-even-though-every-wire). The chip-select gap or a wire is the usual cause, and LCD_BL on J4 pin 38 is the first wire to check.

5. **Bench.** Touch the screen with a finger and drag it around.

   You should see: A white dot is painted wherever you touch, and the board's red LED toggles on each touch.

   If nothing follows the finger, touch is not answering: [the touch chip at address 0x38 entry](../../site/board-mspm0g3507.html#the-touch-chip-does-not-answer-on-i2c1-at-address-0x38-is-it) has the two checks (software I2C pins, TP_RST).

### Expected result

The screen shows red, green, blue, white and black fills, then eight color bars. Then it paints a white dot under your finger and toggles the red LED on every touch. The whole program is 7.4 kB of flash and 648 bytes of RAM.

### What this shows

- A TI LaunchPad kit with 32 kB of RAM drives a 320 x 480 SPI touch screen with 11 jumper wires (12 with the optional touch INT wire) and a small program from this repository. The screen's ID reads back as the ST7796S and the touch chip as the FT6336U.
- A full-screen fill of 300 kB takes 364.9 ms at 8 MHz SPI, 182.5 ms at 16 MHz, and 67.2 ms at 40 MHz SPI. The CPU runs at 80 MHz and frames are 16-bit. At every speed 64 pixels of one row were read back and matched.
- TI's MSPM0 SDK has no example for this screen or any TFT, so the driver is this repository's own.

### Limits

- That the screen is right everywhere. At each speed only 64 pixels of one row were read back. The colors were not checked by eye at 16 MHz or faster.
- That 40 MHz SPI is safe in general. The chip's data sheet rates these standard-drive pins to 16 Mbit/s and the panel's allows about 15 MHz. It worked here on short wires; the margin may not hold elsewhere.
- The fills, bars and touch dots were seen by a person at the board and not recorded, so no photo or video of this exact program exists in the repository.
- A graphics library. LVGL runs on the same wiring, at 7 refreshes a second at 32 MHz and 15 to 19 at 80 MHz, using 25.1 kB of RAM. That is a different program, and its screen was not checked by eye.
- Chip select needs a 10 microsecond pause after it goes high between commands. Without it the screen drops the second of two back-to-back commands and never turns on. The program has the pause (`cs_gap()` in `display_test.c`); a rewritten driver must keep it.
- The touch chip answered only to software I2C on the same two pins. It did not answer to the chip's I2C block, so the program uses software I2C.
- One 320 x 480 frame at 16 bits is 300 kB, 9.4 times the chip's RAM, so drawing goes in strips and stored pictures are cut to 75 kB.
- The speed results need the separate `perf` builds and a RAM dump through OpenOCD. The README's read command was assembled from the flash command and not re-run, so this build leaves it out.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Screen and touch | ST7796S over SPI1 (3-wire, 8 MHz in the demo) and FT6336U touch, software I2C at about 100 kHz, on a Waveshare module with a TXS0108E level translator | `mspm0g3507/display-test/README.md` |
| Full-screen fill | 364.9 ms at 8 MHz SPI; 182.5 ms at 16 MHz; 168.1 ms with 16-bit frames and DMA at 16 MHz; 134.4 ms at 20 MHz and 67.2 ms at 40 MHz SPI with the CPU at 80 MHz; 84 to 92% of what the SPI clock allows | `mspm0g3507/display-test/README.md` |
| Read-back check | 64 of 64 pixels of one row matched at every speed; no other pixels were checked | `mspm0g3507/display-test/README.md` |
| Program size | 7,592 bytes of flash (7.4 kB) and 648 bytes of RAM, from tiarmsize on display_8mhz.out | `mspm0g3507/display-test/README.md` |
| Start-up | 370 ms from power-up to screen ready with the usual init waits; 12 ms with the shortest waits the data sheet allows | `mspm0g3507/display-test/README.md` |
| Tools | Intel Mac, macOS 15.7; MSPM0 SDK 2.11.00.07; SysConfig 1.26.3; TI Arm Clang 5.1.1; xPack OpenOCD 0.12.0; 2026-10-06 | `mspm0g3507/display-test/README.md` |
