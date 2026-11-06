# Community Arduino core for TI MSPM0 on the LP-MSPM0G3507

Partly. The core (`tako0910/arduino-mspm0` 0.1.3) installs through Arduino's normal Boards Manager, and a serial sketch and the unmodified Arduino_GFX graphics benchmark ran on the board. But its default build drops the C library's system calls, `analogRead` does not link, and ArduinoJson does not compile, so a beginner hits a wall on three common sketches.

Terms: [vocab](../../docs/vocab.md). Questions and fixes: [FAQ](../FAQ.md). Checked 2026-10-06 on an Intel Mac, macOS 15.7, arduino-cli 1.5.1 (Homebrew), core `ti:mspm0` 0.1.3 with its own GCC 15.2.1 (arm-none-eabi). Board: LP-MSPM0G3507 with the Waveshare 3.5inch Capacitive Touch LCD wired as in [display-test](../display-test/README.md). 1 kB = 1,024 bytes.

## How to install it

The core's Boards Manager URL:

```text
https://raw.githubusercontent.com/tako0910/arduino-mspm0/master/package_ti_mspm0_index.json
```

Add it under Arduino IDE Settings, "Additional boards manager URLs", then install "Texas Instruments MSPM0 Arduino Core" in Boards Manager. From a terminal:

```text
arduino-cli core update-index --additional-urls https://raw.githubusercontent.com/tako0910/arduino-mspm0/master/package_ti_mspm0_index.json
```

```text
arduino-cli core install ti:mspm0 --additional-urls https://raw.githubusercontent.com/tako0910/arduino-mspm0/master/package_ti_mspm0_index.json
```

Board name: `ti:mspm0:lp_mspm0g3507` (the LP-MSPM0C1104 is also listed). The core with its compiler took 1.1 GB on disk. Install time was not measured.

## What compiled and what ran

| Sketch | Compiles | Flash, RAM | On the board |
|---|---|---|---|
| `serial_hello` (this repo's) | Yes | 4.8 kB, 112 B | **Runs**: printed a line every second, `millis` steps of about 1,002 ms per line (includes print time) |
| Blink (core example) | Yes | not recorded | Not flashed |
| `light_sensor` (this repo's, `analogRead`) | **No**, link error | | |
| ArduinoJson 7.4.3 JsonParserExample, JsonGeneratorExample | **No** | | |
| OneButton SimpleOneButton | **No** (example has no pin for this board) | | |
| Adafruit FT6206 CapTouchPaint (with Adafruit ILI9341) | Yes | 15.2 kB | Not flashed (ILI9341 screen, not the one used here) |
| Adafruit GFX GFXcanvas | Yes | 16.7 kB | Not flashed |
| Adafruit ILI9341 graphicstest | Yes | 23.1 kB | Not flashed |
| Arduino_GFX PDQgraphicstest, ST7796 pins (this repo's: `sketches/pdqgfx_st7796`) | **Only with LTO off** | 61.3 kB, 784 B | **Runs, unmodified library**: drew on the screen and printed timings |

The benchmark was still running when the 70 s serial capture ended, so these lines finished: screen fill 10.32 s (five fills, so about 2.06 s each), text 0.51 s, pixels 11.7 s, lines 11.4 s. This repo's Arduino layer ran the same benchmark's screen fill in 2.55 s (0.51 s each, see [Arduino layer](../../portability/arduino/README.md)). **Unknown (suspected: Third-party behavior).** The core's SPI clock or transfer loop is likely slower. Not checked: which.

## Problems, with their causes

**The default build fails to link anything that uses stdio.** **Third-party behavior.** The core builds with `-flto`, and link-time optimization discards the core's own `_read`, `_write`, `_close`, `_fstat`, `_isatty` and `_lseek` (in `variants/LP_MSPM0G3507/variant_syscalls.c`). The link then fails with `undefined reference to '_write'` and `dangerous relocation: unsupported relocation`. Fix that worked: build with LTO off by passing the core's own flags without `-flto`:

```text
arduino-cli compile --fqbn ti:mspm0:lp_mspm0g3507 --build-property "build.flags.common=<the core's flags without -flto>" --build-property "build.flags.ld=<the core's link flags without -flto>" <sketch>
```

Copy the two flag lines from the core's `platform.txt` (`build.flags.common` and `build.flags.ld`) and delete `-flto`. Adding system-call stubs of this repo's to the sketch instead failed with `multiple definition of '_read'` once LTO stopped dropping the core's, so do not add stubs.

**`analogRead` does not link.** **Third-party behavior.** The error is `undefined reference to 'DL_ADC12_setClockConfig'` from the core's `MspM0Platform.c`, with or without LTO. The core wraps TI's DriverLib for I2C, SPI, UART, timers and others (`cores/arduino/dl_*_wrap.c`) but has no ADC wrapper, so TI's ADC driver is never built. Not fixed here.

**ArduinoJson does not compile.** **Third-party behavior.** `error: 'Printable' in namespace '::' does not name a type`: the core's Arduino API does not provide the `Printable` class that ArduinoJson expects.

**OneButton's example does not compile.** **Setup or handling.** `error: 'PIN_INPUT' was not declared in this scope`: the example defines its button pin only for boards it lists. Adding `-DPIN_INPUT` through `compiler.cpp.extra_flags` did not reach the sketch with this core. The right property was not found. Editing the example to define `PIN_INPUT 18` (S1, PA18) is the expected user step.

## How this compares with this repo's Arduino layer

The [Arduino layer](../../portability/arduino/README.md) of this repo is built with TI's compiler and ran the same Arduino_GFX benchmark about four times faster on screen fill, but it is a project folder, not something a user installs from Boards Manager. The community core is the real Arduino experience, and with the LTO fix it ran an unmodified popular graphics library on a TI chip.
