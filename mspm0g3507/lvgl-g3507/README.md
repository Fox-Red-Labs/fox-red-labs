# LVGL on the TI LP-MSPM0G3507 with 32 kB of RAM

**Status: Works (serial numbers).** Built and run on the board 2026-10-06. The screen picture and touch were not checked by eye in this run, so only the serial numbers are claimed.

Yes, trimmed. LVGL 9.6 with a button, slider, switch, arc, chart and one font runs on the LP-MSPM0G3507 using 25.1 kB of its 32 kB of RAM and 125.4 kB (32 MHz build) or 126 kB (80 MHz build) of its 128 kB of flash. It refreshes 7 times a second at 32 MHz with the CPU about 100% busy, and 15 to 19 times a second at 80 MHz with 40 MHz SPI and the CPU about 60% busy. A refresh redraws only the parts of the screen that changed, not the whole screen. RAM is not the problem; flash is. The second font and a few LVGL drawing paths had to be cut to fit.

Terms: [vocab](../../docs/vocab.md) and [FAQ](../FAQ.md). LVGL is an open-source C graphics library for small screens. A refresh is one pass in which LVGL redraws only the areas that changed and sends them to the screen; in these logs that is about 12,000 to 25,000 pixels, against 153,600 pixels in the full 320 x 480 screen. The heap is the pool LVGL takes its memory from. The draw buffer holds the strip of pixels being drawn. The stack is the RAM for function calls. Built and run 2026-10-06 on an Intel Mac with TI Arm Clang 5.1.1, MSPM0 SDK 2.11.00.07 and SysConfig 1.26.3. Sizes are bytes unless marked kB; 1 kB = 1,024 bytes. The screen is the Waveshare 3.5inch Capacitive Touch LCD wired as in [display-test](../display-test/README.md) (the pin table is there).

Contents: [Results](#results-on-the-board), [Build and flash](#build-and-flash), [On the LP-MSPM0G3519](#on-the-lp-mspm0g3519), [Sizes](#sizes), [Choices](#the-choices-and-why), [What did not fit](#what-did-not-fit-and-what-was-cut), [What was not checked](#what-was-not-checked).

## Results on the board

Each build was read for 19 complete stats lines over about 21 seconds. The first 32 MHz line has three stray characters in front, and the last 80 MHz line is cut where the capture stopped. Defaults: 12 kB heap, 6 kB stack, 6.2 kB draw buffer. Flash and RAM are the `tiarmsize` totals of the 2026-10-06 builds.

| Build | Refreshes per second | CPU busy | Refresh (one pass) | Flush (SPI send) |
|---|--:|--:|--:|--:|
| 32 MHz CPU, 16 MHz SPI | 7 | 96 to 105% | 137 to 150 ms | 27.5 to 27.8 ms |
| 80 MHz CPU, 40 MHz SPI | 15 to 19 | 58 to 62% | 32 to 39 ms | 5.6 to 7.1 ms |

- **Heap:** peak 8.5 kB used at both 32 and 80 MHz, in a pool LVGL reports as 10.3 kB usable (a 12 kB pool). That leaves about 1.8 kB of headroom over the peak in these runs.
- **Stack:** `port.c` paints the stack with `0xA5A5A5A5` at boot. After the demo had run about 20 seconds, 2.1 kB of 6 kB were untouched on both builds, so the peak was 3.9 kB. The peak covers the idle demo's redraws; nobody used the touch screen, so the button, slider and full-redraw paths are not covered.
- **RAM total** with 6 kB stack and 12 kB heap: 25.1 of 32 kB.
- **What a refresh covers:** the `px` field is pixels sent to the screen per second: 172,086 to 174,381 at 32 MHz (about 24,600 to 24,900 per refresh) and 233,674 to 237,037 at 80 MHz (about 12,400 to 15,600 per refresh). A full screen is 153,600 pixels, so one refresh is a fraction of the screen. Refreshes per second are not full-screen frames per second; [display-test](../display-test/README.md) measured full-screen fills at 6.0 and 14.9 a second.
- **Busy above 100%:** `busy` is exactly refreshes per second times the average refresh time, divided by a fixed 1,000 ms (`app.c`). In all 38 lines `fps` times `refr_ms` divided by 10 matches `busy` within 1 point (for example 7 x 145.0 ms = 1,015 ms, shown as 101%). Seven refreshes of 145 ms cannot fit in 1,000 ms, so the real window was a little longer than one second. The one-second stats window probably runs late: the stats timer runs after a 145 ms refresh, and the `t=` counter skips a second twice in the 32 MHz file (no `t=5s`, `t=22s`) and once in the 80 MHz file (no `t=6s`), which fits a window longer than 1 s. The window length was not logged. Read the 32 MHz result as "the CPU is saturated", not as a number above 100%.
- **Serial line length:** a stats line is about 180 characters, so by arithmetic about 15 ms to send at 115,200 baud, and sending blocks the main loop.

Each 80 MHz refresh also sends only 50 to 63% of the pixels of a 32 MHz refresh, so refresh and flush times are not like for like. Per pixel, flush is about 2.5 times faster (about 1.1 µs at 32 MHz, about 0.45 µs at 80 MHz), which matches the change from 16 MHz to 40 MHz SPI. The CPU clock was not read back.

## Build and flash

What is here: `lv_conf.h` (the trimmed LVGL settings), `src/port.c` (1 ms tick, ST7796S flush over SPI1 with DMA, FT6336U touch over software I2C, UART text, stack paint), `src/app.c` (the demo), `syscfg/` (SysConfig for 32 MHz and 80 MHz, same pins as `display-test` plus UART0), `build.sh`, `tools/mapsize.py` (flash and RAM split from the map) and `tools/host_check.sh` (renders the screen on the Mac). LVGL v9.6.0, commit `80ca777e37a2b176770726a02e07a6fb79ef0b39`, MIT license (`third-party/lvgl/LICENCE.txt`), is vendored once for every board in `third-party/lvgl/` with unused parts removed. Three small size patches (12 added lines in three files) are in `size-patch.diff`. `host-render.png` is the screen as rendered on the Mac by the same `app.c`; it is not a photo of the board.

Build the three flashable variants. Run this in this folder:

```bash
./build.sh
```

| Build | CPU | SPI clock | `.out` |
|---|---|---|---|
| `lvgl_32mhz` | 32 MHz | 16 MHz | `out/lvgl_32mhz/lvgl_32mhz.out` |
| `lvgl_80mhz_spi40` | 80 MHz | 40 MHz | `out/lvgl_80mhz_spi40/lvgl_80mhz_spi40.out` |
| `lvgl_80mhz_spi20` | 80 MHz | 20 MHz | `out/lvgl_80mhz_spi20/lvgl_80mhz_spi20.out` |

Options go before the command, for example `OPT=-Os`, `ROWS=20` (draw buffer rows), `MEM_KB=12` (LVGL heap in kB), `STACK=6144` (bytes). The defaults are 12 kB heap and 6 kB of stack. LVGL compiles once per variant (about a minute), then each config links in seconds. `./build.sh oversize` builds the full-feature demo only to measure it; it does not fit and must not be flashed.

The 40 MHz SPI clock is about 2.7 times the panel's write spec of about 15 MHz (ST7796S data sheet, 66 ns minimum write cycle). [display-test](../display-test/README.md) read back 64 pixels of one row correctly at 40 MHz, and the LVGL runs above sent data at that speed, but nobody looked at the screen. If it shows noise at 40 MHz, try `lvgl_80mhz_spi20`, which was built but not run.

Flash one build. The values in angle brackets are this bench's examples, and you substitute yours. `<out>` is one of the `.out` paths above. `<openocd-scripts-folder>` was `~/zephyr-toolchain/xpack-openocd-0.12.0-7/openocd/scripts`, `<openocd.cfg>` was `~/zephyrproject/zephyr/boards/ti/lp_mspm0g3507/support/openocd.cfg`, and `<probe-serial>` was `MG350001`; yours is the text after `usbmodem` in `ls /dev/cu.usbmodem*`, minus the last digit.

```bash
openocd -s <openocd-scripts-folder> -c "gdb_port disabled" -c "tcl_port disabled" -c "telnet_port disabled" -f <openocd.cfg> -c "adapter serial <probe-serial>" -c init -c "reset halt" -c "program <out>" -c "reset run" -c shutdown
```

This programs without `verify`: on these images OpenOCD's verify reported a CRC mismatch ([FAQ](../FAQ.md)), and the programmed images ran.

Read the serial output (UART0 through the probe, 115200 8N1) with any terminal program. The board's serial port is the `/dev/cu.usbmodem` device whose name ends in 1; the one ending in 4 is not used. For example:

```text
screen /dev/cu.usbmodem<probe-serial>1 115200
```

You get a boot banner `LVGL on LP-MSPM0G3507: starting`, then one line a second. A real line from the 32 MHz run:

```text
t=4s fps=7 busy=101% refr_ms=145.0 flush_ms=27.8 px=174381 full_ms=0.0 heap_used=7236 heap_total=10536 heap_peak=8712 heap_free=3300 frag=4% stack_free=2200 touch_err=0 presses=0
```

## What the screen is meant to show

Portrait, dark theme, as in `host-render.png` (the Mac render; the board's screen was not checked by eye): a title, a **Press me** button with a counter, a slider that sets the red LED's brightness in 10 steps, a switch that turns the LED on and off, a **Full redraw** button that times one 320 x 480 redraw, an arc and a line chart fed by a made-up wave every 100 ms, and five stats lines refreshed once a second. The `full_ms` value stays `0.0` until someone presses **Full redraw**, so it was not measured.

## On the LP-MSPM0G3519

The same demo runs on the LP-MSPM0G3519 LaunchPad development kit (512 kB flash, 128 kB RAM, same Cortex-M0+ core), with the display wired like this: SPI1 on PB31, PB8 and PB7, chip select PB12, data/command PB9, backlight PC4, reset PC5, touch I2C2 on PC3 and PC2, touch interrupt PB14, touch reset PB11. Build it with:

```text
BOARD=mspm0g3519 ./build.sh
```

Only the board-specific parts are new, in `boards/mspm0g3519/`: `board_pins.h` (reset, backlight and touch pins that moved to port C) and `lvgl.syscfg` and `lvgl80.syscfg` (the display pins plus UART0 on PA10 and PA11, the same backchannel pins as the G3507). `src/`, `lv_conf.h` and the LVGL copy are shared. Output goes to `out/mspm0g3519/`. Checked 2026-10-09, same tools as above.

Results, 3 runs each, 19 to 24 stats lines per run, read from UART0 through the probe at 115200. The 80 MHz build with 40 MHz SPI was seen on the screen; the other builds' screens were not looked at. Rows are in `data/results/mspm0g3519.csv`.

| Build | Refreshes per second | CPU busy | Refresh | Flush | G3507 |
|---|--:|--:|--:|--:|--:|
| 32 MHz CPU, 16 MHz SPI | 7 | 96 to 105% | 138 to 151 ms | 27.5 to 27.8 ms | 7 |
| 80 MHz CPU, 40 MHz SPI | 15 to 19 (mean 17.1) | 57 to 61% | 32 to 39 ms | 5.6 to 7.1 ms | 15 to 19 |
| 80 MHz CPU, 20 MHz SPI (1 run) | 13 to 18 (mean 15.6) | 63 to 71% | 36 to 52 ms | 10.6 to 16 ms | not run |

The runs of one build were identical line for line after the boot banner, so the demo is repeatable here. Flash is 125.5 kB at 32 MHz and 126.2 kB at 80 MHz, of 512 kB; RAM is 25.1 kB and 25.2 kB, of 128 kB. Heap peak is 8.5 kB and 2.1 kB of the 6 kB stack stayed untouched, as on the G3507.

With the extra room (1 run each, `ROWS=40 BOARD=mspm0g3519 ./build.sh`): a 40-row draw buffer (25 kB) raises the 32 MHz build from 7 to 11.8 refreshes a second on average (10 to 13), because LVGL sends fewer, larger strips; at 80 MHz it stays at 15 to 19 and the CPU drops to 43% busy, so something other than drawing sets the rate there. The cause was not found. The full demo with the second font (`BOARD=mspm0g3519 ./build.sh oversize`, 146.8 kB of flash) also runs at 7 a second at 32 MHz. It is the stock demo with this repo's size cuts left in, not an unmodified LVGL.

Two gotchas. The G3519's linker script sets a 256-byte stack, so `build.sh` now rewrites whatever stack size the script holds; the first build kept 256 bytes and its log showed `stack_free=0`. And the G3519 build prints `LVGL on LP-MSPM0G3519: starting` as its banner; a serial capture keeps everything the previous firmware printed before the reset, so cut the log at the banner.

## Sizes

| | 32 MHz build | 80 MHz, SPI 40 | 80 MHz, SPI 20 | Oversize (not flashable) |
|---|--:|--:|--:|--:|
| Flash used | 128,456 (125.4 kB) | 129,072 (126.0 kB) | 129,072 (126.0 kB) | 150,264 (146.7 kB) |
| Flash free of 131,072 | 2,616 (2.6 kB) | 2,000 (2.0 kB) | 2,000 (2.0 kB) | -19,192 (-18.7 kB) |
| RAM used (`.bss` + `.data` + stack) | 25,733 (25.1 kB) | 25,733 | 25,733 | 25,733 |
| RAM free of 32,768 | 7,035 (6.9 kB) | 7,035 | 7,035 | 7,035 |

All four are `tiarmsize` totals from builds made 2026-10-06 after the stats buffer grew from 160 to 256 bytes (96 bytes more RAM than the first builds, which were 25,637). Flash split for the 32 MHz build, from the map file (`tools/mapsize.py`):

| Part | Flash | RAM |
|---|--:|--:|
| LVGL | 122,052 (119.2 kB), of which fonts 13,649 | 12,288 (the 12 kB heap, `LV_MEM_SIZE`) |
| Port (`port.c`) | 1,744 | 6,417 (draw buffer 6,400, counters 17) |
| App (`app.c`, `main.c`) | 2,107 | 348 (the 256-byte stats buffer is most of it) |
| SysConfig and startup | 768 | 0 |
| C library | 1,785 (what remains of the total) | 536 (rest of `.bss`) |
| Stack (linker) | 0 | 6,144 |
| Total | 128,456 | 25,733 |

The 80 MHz builds are 616 bytes bigger in flash. The map shows 300 of them in the SysConfig and startup code, which holds the clock setup; the other 316 were not traced.

## The choices, and why

- **Heap 12 kB, draw buffer 320 x 10 rows (6.2 kB), stack 6 kB.** One partial buffer in RGB565, no double buffering. Ten rows is the size that was built and run; more rows mean fewer strips per redraw but more RAM. `ROWS=20` was not built; by arithmetic it adds 6.2 kB of RAM and leaves 635 bytes (0.6 kB) free. The heap peak is measured (8.5 kB); the stack peak is measured (3.9 kB).
- **No byte swap.** The SPI block sends one 16-bit frame high byte first, which is the order the ST7796S wants, so LVGL's native RGB565 goes straight to DMA, as in `display-test/perf.c`.
- **DMA with a blocking wait.** One buffer means the CPU cannot render the next strip while DMA sends this one. DMA is no faster than a CPU loop here (the wire is the limit, see display-test), so it is used to free the CPU bus, not for speed.
- **16-bit frames need the SPI block off to change size.** `frames16()` disables SPI, sets the size, and enables it again around every strip. The `DL_SPI_setDataSize` documentation does not say so.
- **Chip select gap of 10 µs after every raise.** The suspect is the module's level translator dropping shorter pulses ([display-test trap 1](../display-test/README.md#trap-1-the-screen-stays-dark-or-shows-static-even-though-every-wire-is-right)).
- **Touch by software I2C at about 100 kHz,** polled every 20 ms, because the I2C block never got an answer ([display-test trap 2](../display-test/README.md#trap-2-the-touch-chip-does-not-answer-the-i2c-block-but-does-answer-software-i2c)). A read that is not acknowledged keeps the last touch state and counts as a touch error.
- **1 ms SysTick** feeds `lv_tick_set_cb`. The same interrupt runs a 10-step software PWM on the red LED (PA0, assumed active high, as in TI's `gpio_toggle_output`). The main loop is `lv_timer_handler()` then `__WFI()`.
- **`-Oz`.** `-Os` was 8,544 bytes (8.3 kB) bigger than `-Oz` before the size patch, so only `-Oz` is known to fit. `-Oz` is slower than `-O2`; no speed was measured.
- **LVGL config** (`lv_conf.h`): software renderer only, RGB565 only, no ARGB8888 or A8 destinations, no float, no OS, no log, no file system, no image decoders, no flex or grid, no sysmon, default theme (dark) only, 14 px Montserrat.

## What did not fit, and what was cut

 The 128 kB flash is where the full demo stops. LVGL is made of many small pieces, so no one function is large and there is no single thing to remove. The build got under the limit by removing a font and drawing features. So this says what fits in this build, not the most any build could fit.

The builds, in the order they were made (flash in bytes, chart and arc in every one):

| Build | Flash |
|---|--:|
| `-O2`, Montserrat 14 and 24, ARGB8888 and A8 drawing on, stock LVGL | 266,768 (260.5 kB) |
| `-Os`, Montserrat 14 and 20, ARGB8888, A8 and other formats off in `lv_conf.h` | 178,976 (174.8 kB) |
| `-Oz`, same | 170,432 (166.4 kB) |
| `-Oz`, size patch (no shadow, blur, layer, triangle, mask-rectangle or image drawing, no image decoder), Montserrat 20 dropped | 133,504 (130.4 kB) |
| Same, plus no image-format glyphs in `lv_draw_sw_letter.c` | 128,464 (125.5 kB) when measured; 128,456 in the final build (8 bytes less, from later small changes that were not isolated) |

Montserrat 20 costs 21,808 bytes (21.3 kB): the oversize build below, 146.7 kB, is the final demo with that font added back. The size patch is worth 20,168 bytes (19.7 kB) with that font in both builds (166.4 kB down to 146.7 kB). The oversize build is 19,192 bytes (18.7 kB) over the chip's flash; it links only into a pretend 512 kB flash for measuring and must not be flashed.

The patch assumes the demo draws no images, shadows or layers. A widget that needs one draws nothing for that part, and the image decoder is not initialized, so code that opens an image would misbehave. None of this demo's widgets do; the Mac render shows the screen drawing correctly. Rounded corners, the arc and the chart all stayed in. Other font sizes were not run.

`-flto` was accepted and the output was byte for byte identical. TI Arm Clang may ignore `-flto` for this link. Its documentation was not checked.

## What was not checked

- The screen by eye: display init, colors, orientation. Only serial numbers were read.
- Touch through LVGL: the saved runs show `touch_err=0` and `presses=0` on all 38 lines, which means the touch chip answered every read, but nobody touched the screen. Coordinates, orientation and what a finger does to the button and slider were not run by a person.
- LED polarity (PA0 assumed active high) and the slider's effect on brightness.
- DMA through LVGL's buffer: the flush time was measured, but the pixels were not compared.
- The `lvgl_80mhz_spi20` build: built, not run.
- Software I2C timing at 80 MHz: its delay scales with the CPU clock, so it should still be about 100 kHz; not measured.
- `-Os` after the size patch: only `-Oz` was built with it.

## Mistakes to avoid

- Do not start with a 2 kB stack. Use 6 kB. A 2 kB stack ran out (`stack_free=0`, no untouched bytes), and a 4 kB stack left 152 bytes at the measured peak of 3.9 kB, so 2 kB was overrun by about 1.9 kB.
- Do not size the heap from a 64-bit Mac run. The measured peak on the chip is 8.5 kB, so 12 kB is enough here.
- Keep the serial stats buffer at 256 bytes. The stats line is 176 to 179 characters, so a 160-byte buffer cuts every line in the demo's own code (`static char line[256]` in `src/app.c`). The serial link is not the cause.
