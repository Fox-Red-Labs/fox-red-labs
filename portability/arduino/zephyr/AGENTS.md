# AI assistant notes for Arduino sketches on Zephyr on TI LaunchPad kits

That the sketch stays as Arduino wrote it, and everything board-specific sits in one devicetree overlay and one small config file. This file is the short list, each item from runs on the LP-EM-CC2340R5 and the LP-MSPM0G3519, one board each.

Terms: [docs/vocab.md](../../../docs/vocab.md) and the [FAQ](FAQ.md) in the Fox Red Labs repo. In short: **ArduinoCore-zephyr** is the Arduino core that Zephyr compiles into your image as an extra module; a **devicetree overlay** is a text file that tells Zephyr which chip pin is which Arduino pin; **west** is Zephyr's build front end; **DSLite** is TI's command-line flasher (UniFlash 9.6.0); **nRST** is the chip's reset pin. 1 kB = 1,024 bytes. Checked 2026-10-24 and 2026-10-25 on an Intel Mac, macOS 15.7.9, with ArduinoCore-zephyr `af5b8610f9be`, ArduinoCore-API 1.5.2, Zephyr 4.4.2 (CC2340R5), Zephyr main `1405adc9a` (G3519) and Zephyr SDK 0.17.4 GCC 12.2.0.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths below in backticks starting `portability/` are in the Fox Red Labs repo, where the evidence for each line lives; they are not in your copy, and links in this file only work inside that repo. The files to copy are listed in "Take it with you" in `portability/arduino/zephyr/README.md`.

## The layer

| Part | Fact |
|---|---|
| What it is | An unchanged `.ino` file included after `#include <Arduino.h>` by `app/CMakeLists.txt`, built by `west build` with ArduinoCore-zephyr as `-DZEPHYR_EXTRA_MODULES` (`portability/arduino/zephyr/README.md`) |
| Board-specific files | One overlay (`variants/<board>/<board>.overlay`, about 40 lines for the pin map in its `zephyr,user` node) and, for the CC2340R5, `app/boards/lp_em_cc2340r5.conf` (turns on PWM, SPI and I2C) (`portability/arduino/zephyr/variants/`) |
| Sketch edits | None. Blink, AnalogReadSerial and the interrupt, PWM, SPI and Wire sketches built as written (`portability/arduino/zephyr/README.md`) |
| Boards proven | LP-EM-CC2340R5 on Zephyr 4.4.2; LP-MSPM0G3519 on Zephyr main with the MSPM0 I2C pull request for Wire, SPI, PWM and interrupts |
| Not covered | The LP-MSPM0G3507 (use this repo's own Arduino layer, `portability/arduino/README.md`); LLEXT; any board without its overlay (`portability/arduino/zephyr/FAQ.md`) |

## Start here

| Step | This port |
|---|---|
| Tools | Zephyr workspace `<zephyr workspace>` (4.4.2 for the CC2340R5), Zephyr SDK 0.17.4 GCC 12.2.0 as a cross compiler (`ZEPHYR_TOOLCHAIN_VARIANT=cross-compile`, `CROSS_COMPILE=<toolchain>/arm-zephyr-eabi/bin/arm-zephyr-eabi-`), `west`, `ninja`, `cmake`, picolibc (`portability/arduino/zephyr/README.md`) |
| Core | `git init <AM> && git -C <AM> fetch --depth 1 https://github.com/zephyrproject-rtos/ArduinoCore-zephyr.git af5b8610f9be6d7b962e2ba6dd8e6859697c44ff`, check it out, then put ArduinoCore-API 1.5.2's `api` folder in `<AM>/zephyr/blobs/ArduinoCore-API/api` (`portability/arduino/zephyr/README.md`) |
| Patch (CC2340R5 only) | `git -C <AM> apply serial-blocking-write.patch` so `Serial` stops dropping bytes (`portability/arduino/zephyr/FAQ.md`) |
| Build | `cd <zephyr workspace>/zephyr && west build -p always -b lp_em_cc2340r5 <port folder>/app -d <build> -- -DSKETCH=Blink -DSKETCH_DIR=<port folder>/sketches -DZEPHYR_EXTRA_MODULES=<AM> -DDTC_OVERLAY_FILE=<port folder>/variants/lp_em_cc2340r5/lp_em_cc2340r5.overlay` (about 80 s) |
| Probe serial | `xdsdfu -e` from CCStudio's `ccs_base/common/uscif/xds110/` folder |
| Back up | CC2340R5: read main flash `0x0,0x20000` and CCFG `0x4E020000,0x200` with DSLite `memory -r` before the first write; G3519: main flash and NONMAIN (`portability/arduino/zephyr/README.md`) |
| Flash | CC2340R5: `DSLite flash -c <ccxml> -e -f -u <build>/zephyr/zephyr_crc32.hex`, then `xds110reset -s <probe serial>`. G3519: pulse nRST, flash a `.bin` padded to a multiple of 8 bytes, pulse nRST again (`portability/zephyr-mspm0/flash.sh`) |
| Check | Serial at 9600 baud on `/dev/cu.usbmodem<probe serial>1`; the first line is Zephyr's 115200 banner and shows as noise (`portability/arduino/zephyr/FAQ.md`) |
| Restore | DSLite `flash` of the backup, then read both areas again and compare |

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| `'isnan' was not declared`, `'atol' was not declared` or `'rand' was not declared` | The Zephyr checkout has no picolibc module, so the minimal C library is used | Add the picolibc module (`west update picolibc`) | Sketch compiles (`portability/arduino/zephyr/FAQ.md`) |
| `undeclared identifier 'A0'` | This repo's own Arduino layer has no `analogRead`; you built with it, not with this port | Build through ArduinoCore-zephyr | `A0` resolves (`portability/arduino/zephyr/FAQ.md`) |
| `SPI.transfer(buf, n)` returns zeros or stale bytes | The core runs `spi_write` then a separate `spi_read` | Use `SPI.transfer(byte)` in a loop | Bytes match (`portability/arduino/zephyr/FAQ.md`) |
| AnalogReadSerial prints digits with most newlines missing | `ZephyrSerial::write()` drops what does not fit its 64-byte ring buffer | Apply `serial-blocking-write.patch` to the core | 185 whole value lines in 1.5 kB |
| `attachInterrupt` stops firing after a later `pinMode()` on the same pin (CC2340R5) | Zephyr 4.4.2's `gpio_cc23x0` rewrites the whole IOC register and drops the edge-detect bits | Call `attachInterrupt` again after any `pinMode` on that pin | The interrupt fires (`portability/arduino/zephyr/FAQ.md`) |
| `analogWrite` on the red LED does nothing (CC2340R5) | The overlay copied the board file's `<&pwm5 0 ...>`; the pin is on LGPT1 channel 2 | Use `<&pwm5 2 ...>` | LGPT1 compare changes (`portability/arduino/zephyr/FAQ.md`) |
| An I2C scan finds nothing but a one-byte register read works (CC2340R5) | The empty write of the scan fails on Zephyr 4.4.2's CC23x0 I2C driver (not traced) | Probe with a one-byte register read | The device answers (`portability/arduino/zephyr/FAQ.md`) |
| Image flashed, nothing blinks, program counter at `z_SysNmiOnReset`, `SYSCTL.NMIIIDX` reads 6 (G3519) | Started right after the DSLite flash; cause not found | Pulse nRST after the flash | LED toggles (`portability/arduino/zephyr/FAQ.md`) |
| DSLite `Connection to MSPM0 core failed` (G3519) | A running Zephyr image hides the core | Pulse nRST first, then flash | DSLite connects (`portability/arduino/zephyr/FAQ.md`) |
| `attachInterrupt(pin, f, FALLING)` after a `RISING` on the same pin fires on both edges (G3519) | `gpio_mspm0` ORs the new edge into `POLARITY15_0` and never clears the old bits | Clear the pin's two bits before attaching | 10 of 10 (`portability/arduino/zephyr/FAQ.md`) |
| `analogWrite` brightness is inverted (G3519) | `pwm_mspm0` writes the pulse as the low time | Write `255 - value`, or fix the driver | Duty matches (`portability/arduino/zephyr/FAQ.md`) |
| `analogWrite` on PB27 leaves the pin low (G3519) | Zephyr's shared pin file gives `tima1_ccp1_pb27` function 6, which is COMP2.OUT on the G351x | Define the group with `MSP_PINMUX(58, MSPM0_PIN_FUNCTION_9)` | Pin toggles |

## Rules for the assistant

1. **Do not edit the sketch.** A port that needs a pin map changes the overlay, a port that needs a driver feature changes the config file (`portability/arduino/zephyr/README.md`).
2. **For a new board, copy an existing overlay and rename it to the Zephyr board name**, then change only the pin lists and the ADC channel nodes inside `zephyr,user` (`portability/arduino/zephyr/variants/lp_em_cc2340r5/lp_em_cc2340r5.overlay`).
3. **Check every MSPM0 pin group against SysConfig's `MSPM0G351X.json`**, not against Zephyr's shared pin file.
4. **Back up before the first write and restore at the end**, then read the backup back and compare (`portability/arduino/zephyr/README.md`).
5. **Read "Limits" in `portability/arduino/zephyr/README.md`** before promising a feature.
