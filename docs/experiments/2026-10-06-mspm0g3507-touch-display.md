<!-- experiment
id: E9
board: LP-MSPM0G3507
date: 2026-10-06
status: works
reached: verified
line: Works: fills, bars and touch dots on 12 jumper wires (seen, not saved); a full-screen fill takes 0.36 s at 8 MHz SPI and 67 ms at 40 MHz SPI with an 80 MHz CPU. The line is memory: a full frame (300 kB) is 9.4 times the RAM, so drawing goes in strips. It needs a 10 µs chip-select gap and software I2C.
stop: ram
-->
# Can a TI LP-MSPM0G3507 drive the Waveshare 3.5inch Capacitive Touch LCD?

**Short answer.** Yes. With 12 jumper wires and a 7.4 kB program, the board fills the 320x480 screen in 0.36 s at 8 MHz SPI and 67 ms at 40 MHz SPI (that needs an 80 MHz CPU clock and 16-bit frames), and reads touches from the FT6336U touch chip. The fills and touches were seen by a person and not saved; the speeds were measured. Two things are needed that the Waveshare wiki and the TI SDK do not mention: a 10 µs pause after chip select goes high, and software I2C for the touch chip.

Terms: [vocab](../vocab.md) and [FAQ](../../mspm0g3507/FAQ.md). Full write-up and code: [display-test README](../../mspm0g3507/display-test/README.md). LVGL on the same screen: [lvgl-g3507 README](../../mspm0g3507/lvgl-g3507/README.md). 1 kB = 1,024 bytes.

**Question.** Can a TI LaunchPad development kit with 32 kB of RAM run a 3.5 inch touch display, how fast, and where does it stop?

**Vendor position.** MSPM0 SDK 2.11.00.07 (read 2026-10-06) has no example for this screen or any TFT: a search for st7796, ili9341, st7789, ft6336 and lvgl in the SDK found nothing, and `source/ti/display` holds only a UART display. TI's LVGL demo for this same panel and touch chip exists for the MSPM33C321A, described in TI application note [SDAA158](https://www.ti.com/lit/pdf/sdaa158), "LVGL Use On MSPM33C". Waveshare's wiki gives wiring for Raspberry Pi, Pico and ESP32, not for TI boards.

**What was tried.** Intel Mac, macOS 15.7, SysConfig 1.26.3, TI Arm Clang 5.1.1, OpenOCD xPack 0.12.0. Pins from SysConfig with SPI1 (PB9, PB8, PB7) and I2C1 (PB2, PB3), the same pins as TI's `spi_controller_multibyte_fifo_poll` and `i2c_controller_rw_multibyte_fifo_poll` examples. This repo's own ST7796S and FT6336U code in `mspm0g3507/display-test/`. Diagnostics: a pin pull test for every wire, a backlight test, and status reads from the screen over MISO.

**How far it got.**

- Wire: all 12 wires tested connected by the pin pull test (`probe.c`). The 13th wire, MISO, is optional and was used for read-back.
- Run, first try: the screen showed power-on static, then black, never an image.
- Diagnose: with MISO wired, the screen's ID read `0x7796` and its power-mode register showed "display off" after this repo's setup (observed 2026-10-06; not saved). Each command sent alone took effect; the second of two back-to-back commands was lost. A 10 µs gap after chip select fixed it. The suspect is the module's TXS0108E level translator not passing the few-hundred-nanosecond chip-select pulse; the pulse was not looked at with a scope and no module without the translator was tried.
- Touch: the I2C1 block never got an answer from address 0x38. The same pins as software I2C read vendor `0x11`, chip `0x64` (FT6336U) and firmware `0x12` (observed 2026-10-06; not saved). The suspect is the same translator on an open-drain bus; external pull-ups, a slower clock and the glitch filter were not run.
- Verified on the board, observed by the person at it, not saved: color fills, color bars, a dot under the finger on each touch. Pictures: a half-resolution photo mode (160x240, 75 kB) and a full-resolution 16-color mode (320x480, 75 kB), both made by `photo.py` and drawn in strips.
- Measured: full-screen fill 364.9 ms at 8 MHz SPI (84% of the SPI clock's limit), 182.5 ms at 16 MHz with 8-bit frames (168.1 ms with 16-bit frames and DMA), 67.2 ms at 40 MHz with an 80 MHz CPU clock (set with a SysConfig clock-tree change) and 16-bit frames. At every speed 64 pixels of one row were read back from the screen and matched (64 of 64); no other pixels were checked. DMA gave no speed over the CPU loop. Observed, not saved: 2.92 s at 1 MHz.
- Program: 7.4 kB of flash and 648 bytes of RAM, from `tiarmsize`. A touch menu app with fonts and pictures: 46 kB of flash, kept outside the repo (observed 2026-10-06; not saved).
- LVGL, trimmed: runs, 7 refreshes per second at 32 MHz and 15 to 19 at 80 MHz with 40 MHz SPI, 25.1 kB of RAM and 125.4 kB of flash at 32 MHz (126 kB at 80 MHz). A refresh redraws only the changed areas, about 12,000 to 25,000 pixels of the screen's 153,600, so these are not full-screen frames per second. The screen picture and touch were not checked by eye in that run; the logs show `touch_err=0` and `presses=0`.

**The line.** Drawing and touch work, so the line here is memory, and it is a different memory for each approach. Drawing whole frames is limited by RAM. One 320x480 frame at 16 bits is 300 kB, 9.4 times the 32 kB RAM and more than twice the 128 kB flash, so the code written here draws in strips and stored pictures are cut to 75 kB. LVGL draws in strips too, so RAM is not its limit here (25.1 kB of 32 kB used); flash is: the trimmed demo leaves 2.6 kB of flash free at 32 MHz, and the same demo with a second font needs 146.7 kB, 18.7 kB over the chip's 128 kB. LVGL got under the limit by removing drawing features and a font; other trims were not run, an older LVGL or another graphics library.

- Look at the screen by eye at 16 and 40 MHz, and read back every pixel, not 64 of one row. Try longer wires or a second module at 40 MHz.
- The I2C1 block with 4.7 kOhm external pull-ups, the glitch filter off, or a lower clock.
- Touch through LVGL, checked by a person.
- The same screen on the LP-MSPM0G3519 (128 kB RAM, 512 kB flash) or LP-CC1354P10-1 (296 kB RAM).

**Ask.** Has anyone driven a TXS0108E-buffered SPI display from an MSPM0 at high clock rates? What chip-select gap did you need, and did hardware I2C work through it? To repeat this, start with the [display-test README](../../mspm0g3507/display-test/README.md), then try [lvgl-g3507](../../mspm0g3507/lvgl-g3507/README.md).
