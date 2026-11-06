# BOOSTXL-EDUMKII Educational BoosterPack: one portable driver for five LaunchPad kits

The BOOSTXL-EDUMKII has a 128 x 128 color screen, a joystick with a press switch, two buttons, an RGB LED, a buzzer, a 3-axis accelerometer, a microphone and a light sensor. TI ships examples for it only on the MSP432. This folder is one plain-C driver for the whole board, plus the pin map and a hello example for five MSPM0 and MSPM33 LaunchPad kits.

Wiring is "plug on": stack the BoosterPack on the LaunchPad with pin 1 on pin 1 and set the jumpers the board's `PINS.md` lists. Nothing is wired by hand except on the LP-MSPM0L1306 screen path, which is a build check only.

Terms: [docs/vocab.md](../../../docs/vocab.md). In short: **SysConfig** is TI's tool that writes a chip's pin and clock setup code; **DriverLib** is TI's C peripheral library for these chips; **DMA** moves data without the CPU. 1 kB = 1,024 bytes.

## What is here

| Part | What it is |
|---|---|
| `include/edumkii.h`, `src/` | The core: ST7735S screen (fill, rectangle, outline, pixel, line, blit, 5 x 7 text, numbers), joystick, buttons, buzzer, RGB LED, accelerometer, microphone level, OPT3001 light sensor. No vendor includes. The host supplies a port: SPI write, optional DMA bulk write, GPIO, ADC, I2C, delays |
| `test/run.sh` | Host unit test of the drawing and font code. A model of the ST7735S RAM sits behind the port, so fills, clipping, blits, glyph shapes and the init sequence are checked on the Mac with `cc` |
| `boards/<board>/pins.json` | The one pin map per board. `tools/gen_board.py` turns it into the SysConfig file, a C header and `PINS.md`, so the code and the table cannot disagree |
| `boards/common/port_dl.c` | The DriverLib port, one file for MSPM0 and MSPM33 |
| `examples/hello/hello.c` | Test pattern, title, live joystick and button readout, light and sound levels, LED colors, beeps, and one console line a second |
| `examples/probe/probe.c` | Stack check, one line a second: I2C addresses that answer (0x44 on a seated pack), microphone pin mean and peak to peak, joystick, accelerometer, S1 and S2; `APP=probe EDU_CLK160=1 ./build.sh mspm33c321a --lcd`. On 2026-10-30 an LP-MSPM33C321A showed no I2C answer and a microphone pin that ignored speech (`mspm33c321a/FAQ.md`). Apps from other folders build through `APP_HOOK=<script>`; `EDU_UART_RX=1` adds console receive (S1's pin) and `EDU_UART_BAUD` sets the rate |
| `examples/llama2c/llama2c.c` | A language model (TinyStories 260K, `ports/llama2c`) writes its story on the screen, word wrapped and scrolling, with tokens per second in a header; `APP=llama2c EDU_CLK160=1 ./build.sh mspm33c321a` (567.3 kB of flash, 197.4 kB of RAM; 19.1 tokens a second with the display on, 21.1 with the drawing time taken out) |
| `examples/gameboy/gb_app.cpp` | Peanut-GB (`ports/peanut-gb`) on the screen; the 160 MHz frame-skip image `APP=gameboy EDU_CLK160=1 GB_FLAGS=-DPGB_FRAME_SKIP EDU_TAG=skip ./build.sh mspm33c321a` ran with 0 late frames in 8,167 at the console's 59.7 frames a second |
| `build.sh` | One command per board |
| `RESULTS-ROWS.csv` | Result rows for each board |

## Per board

Built with MSPM0 SDK 2.11.00.07 and MSPM33 SDK 1.04.00.00, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0, `-Os`, 32 MHz. Flash and RAM come from each linker map; the stack reserve is not counted.

| LaunchPad | State | Flash, RAM | What limits it | Command |
|---|---|---|---|---|
| LP-MSPM0L1306 | Works, subset | 7.1 kB, 380 B of 4 kB | No screen: BoosterPack pins 13 (SWCLK), 17 (SWDIO) and 31 (no pin). No S1, S2 (pins 32, 33 have no pin) and no accelerometer Z (pin 25 is an op-amp input). Joystick press is read as analog because PA24 is not a GPIO in this package | `./build.sh mspm0l1306` |
| LP-MSPM0L1306 screen path | Should work | 13.8 kB, 892 B | Built, not run on the board. Needs jumper wires from pin 18 to 13 and pin 19 to 31; SWD pins stay unused | `./build.sh mspm0l1306 --lcd` |
| LP-MSPM33C321A | Works | 16.5 kB, 896 B | Screen at 19.5 ms a frame with DMA at 16 MHz; Snake, TileWorld and Crate Confusion at about 31 fps, Game Boy at full console speed with the 160 MHz frame-skip image ([ports/peanut-gb](../../../ports/peanut-gb/README.md)). Joystick press cannot read high (PA21 is the LaunchPad kit's VREF- pin; see below). DMA screen path | `./build.sh mspm33c321a` |
| LP-MSPM0G3519 | Should work | 13.8 kB, 932 B | Built, not run on the board. S1 and the console share PA10 and PA11 with the probe's serial line | `./build.sh mspm0g3519` |
| LP-MSPM0G3507 | Should work | 13.9 kB, 1.2 kB | Built, not run on the board. Jumpers J8, J9, J16, J17, J18 and J22 must change; 32 kB RAM is enough for the core but not for a full frame buffer | `./build.sh mspm0g3507` |
| LP-MSPM0G5187 | Should work | 14.0 kB, 932 B | Built, not run on the board. RGB LED jumper J12 on the screen data line; no isolation for the probe serial pins; pin 26 shares the audio codec clock net | `./build.sh mspm0g5187` |

"Works" means the hello example ran on the board with the BoosterPack stacked. "Should work" means it compiled and linked; it has not run on that board with this driver. The older bench code on the LP-MSPM33C321A (51 frames a second with DMA at a 16 MHz SPI clock) used a different driver and is not a result for this one.

Also built for the LP-MSPM33C321A, not run: `APP=framebench ./build.sh mspm33c321a` (full-frame time, polled and DMA, 16 and 8 MHz) and `APP=snake AUTOSTART=1 ./build.sh mspm33c321a` (Snake from `ports/arduboy` on this core; 20.7 kB flash, 6.8 kB RAM).

## What ran

On the LP-MSPM0L1306, with the jumpers as found: the light sensor answered with its two ID registers and read about 2,270 lux; joystick Y sat at mid-scale (2,033 to 2,041 counts); accelerometer X and Y read 2,009 to 2,014 and 2,034 to 2,046 counts; the microphone showed 8 to 16 counts peak to peak in a quiet room. Joystick X read about 3 counts and did not move. The LaunchPad kit's J4 connects the light-sensor amplifier to that pin; pull J4 to confirm. The LED, the buzzer and the press switch were driven or read but nobody watched or pressed them.

## Joystick press on the LP-MSPM33C321A

**Warning: never drive PA21.** On the LP-MSPM33C321A, BoosterPack pin 5 has no chip connection, and PA21 is the VREF- pin, tied to ground through R10. Do not configure PA21 as a high output. A BoosterPack microphone that takes its power from this pin (the Audio BoosterPack) needs a jumper from pin 8 to pin 5.

The press line (BoosterPack pin 5, PA21) has a 10 kOhm pull-up to 3.3 V on the BoosterPack (R19, schematic in SLAU599B). On the LaunchPad the same pin is the VREF- pin, and R10 ties that net to the reference ground (SLAU969 schematic). A line tied to ground reads low at every internal resistor setting, which is what the earlier bench run saw. This is a reading of the schematics, not a measurement. To confirm, read PA21 as analog channel ADC1_7 (expect near 0 V) and lift R10.

## Use it

1. `cd portability/boosterpacks/edumkii && ./test/run.sh` runs the host test.
2. `./build.sh <board>` builds the hello example into the output folder set at the top of `build.sh` (pass a folder as a last argument to change it).
3. Flash the `.out` file in `<that folder>/ticlang/` with CCStudio or DSLite. The console line shows the readouts at 115200 baud (`EDUMKII hello on ...`).

To add a board, copy a `boards/<board>/pins.json`, fill in the BoosterPack pins from the LaunchPad kit's user guide, and run `tools/gen_board.py`. To use another SDK, fill in `edumkii_port_t` with six functions; the core needs nothing else.

## License and sources

BSD-3-Clause, like the rest of the repository. The driver, and the 5 x 7 glyphs were written for this project from the ST7735S command set, the BoosterPack user guide (SLAU599B) and the OPT3001 registers. No vendor driver or font file was copied. Zephyr's `display_st7735r.c` (Apache-2.0) was read for command names only.
