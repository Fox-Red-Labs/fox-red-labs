<!-- experiment
id: E154
board: LP-MSPM0G3519
date: 2026-10-10
status: line found
reached: link
line: Zephyr 4.4.2 plus its LVGL module links for the G3519 (253.5 kB of flash, 49.5 kB of RAM, with a dummy display) but cannot reach the panel: the tree has no MSPM0 SPI or I2C driver, so a touch screen on this chip under Zephyr needs a driver written first.
stop: run
-->
# Can Zephyr run LVGL with a touch screen on the TI LP-MSPM0G3519?

**Short answer.** It builds, it does not reach the screen. Zephyr 4.4.2 with its LVGL module (9.5.0) links for the board at 253.5 kB of flash and 49.5 kB of RAM using a dummy display. The panel's SPI and the touch chip's I2C have no driver for the MSPM0 family in this Zephyr tree, so it cannot draw on the Waveshare panel. The blinky NMI on this board is also still open.

Terms: [vocab](../vocab.md). 1 kB = 1,024 bytes.

**Question.** Does the Zephyr route work for a touch UI on the G3519, and what does it cost against this repo's bare-metal LVGL build?

**Vendor position.** Zephyr's LVGL page lists the module, a display API and a `st7796s` driver; the board page for `lp_mspm0g3519` lists UART, GPIO and a console. The tree read was a Zephyr 4.4.2 checkout (dated 2026-08-06): `drivers/display/display_st7796s.c` and `drivers/input/input_ft5336.c` exist; `drivers/spi` and `drivers/i2c` contain no file for MSPM0.

**What was tried.**

- `west update lvgl` fetched LVGL 9.5.0 (this repo's bare-metal build uses 9.6.0).
- An app in `thermostat-system/g3519-zephyr/`: a thermostat-shaped screen (title, badge text, setpoint, plus and minus buttons), a devicetree overlay with a `zephyr,dummy-dc` display of 320x480, LVGL pool 12 kB, Montserrat 14 and 20, `CONFIG_SIZE_OPTIMIZATIONS`.
- Built with `west build -b lp_mspm0g3519` and the cross-compile toolchain used by the board's other Zephyr builds. Not flashed, because it draws nothing and the board holds the thermostat screen for other tests.

**How far it got.** Configure, compile, link. Not flashed.

| Measure | Zephyr 4.4.2 + LVGL 9.5.0, dummy display | This repo's bare-metal LVGL 9.6 thermostat |
|---|---|---|
| Flash | 253.5 kB (99 percent of the 256 kB the board file allows; the chip has 512 kB) | 242.4 kB (includes a 48 px font and the radar parser) |
| RAM | 49.5 kB (77 percent of the 64 kB the board file allows; the chip has 128 kB) | 27.3 kB plus 6 kB stack |
| Frames per second | not run | 8 refreshes a second under the radar replay |

The two columns are not the same feature set: Zephyr's LVGL turns on its default widget set and a kernel; this repo's is the trimmed config. The reading is "same order, Zephyr costs roughly 10 kB more flash and 20 kB more RAM for fewer features", not a precise price.

**The line.** The missing piece is a driver, not memory: no `spi_mspm0` and no `i2c_mspm0` in Zephyr's `drivers` folder. This repo's bare-metal port drives SPI1 by DMA and bit-bangs the touch I2C (the touch chip answers only software I2C on this board), and a Zephyr port would need an SPI driver at minimum, plus a GPIO-bit-banged I2C node (`i2c-gpio` exists in Zephyr).

**Ask.** Is an MSPM0 SPI controller driver on Zephyr's roadmap, and is there a branch?
