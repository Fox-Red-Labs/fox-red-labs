# BOOSTXL-EDUMKII on an MSPM0 or MSPM33 LaunchPad: what an AI coding assistant needs to know

That the BoosterPack's header pins land on different chip pins on every LaunchPad, and some clashes only show up in the LaunchPad schematic. This file is the short list; each line cites a run or a schematic. Terms: [docs/vocab.md](../../../docs/vocab.md). In short: **SysConfig** is TI's tool that writes a chip's pin and clock setup code; **DriverLib** is TI's C peripheral library; **DMA** moves data without the CPU. 1 kB = 1,024 bytes. Checked 2026-10-20 on an Intel Mac, macOS 15.7, MSPM0 SDK 2.11.00.07, MSPM33 SDK 1.04.00.00, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0.

**How to use it.** Copy this file into your project as `AGENTS.md` (an assistant that reads `CLAUDE.md` needs the line `@AGENTS.md` there). Paths in backticks are in the Fox Red Labs repository, where the evidence lives.

## Start here

| Step | Command |
|---|---|
| Host test of the drawing and font code | `portability/boosterpacks/edumkii/test/run.sh` (needs only `cc`) |
| Build the hello example | `portability/boosterpacks/edumkii/build.sh mspm0l1306` (also `mspm33c321a`, `mspm0g3519`, `mspm0g3507`, `mspm0g5187`) |
| Pin table for your board | `portability/boosterpacks/edumkii/boards/<board>/PINS.md`, generated from `pins.json` |
| New board | Copy a `pins.json`, fill in the BoosterPack pins, run `tools/gen_board.py <board> <out>` |

## Port interface

One struct, `edumkii_port_t` in `include/edumkii.h`. Required: `delay_us`, `delay_ms`. Optional and checked at the call: GPIO, SPI write, SPI bulk write (DMA), ADC, PWM, I2C. The core drives chip select and data/command itself, so SPI must be mode 0, 8-bit, blocking (`portability/boosterpacks/edumkii/test/test_gfx.c` checks no byte goes out with select high).

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| Joystick X reads near 0 and never moves on the LP-MSPM0L1306 | The LaunchPad kit's J4 ties its light-sensor amplifier to PA25 (BoosterPack pin 2). Expected cause; J4 not pulled in the run | Pull J4 | The reading follows the stick |
| Joystick press always reads pressed on the LP-MSPM33C321A | PA21 (pin 5) is the LaunchPad kit's VREF- pin and R10 ties it to ground. A reading of the schematics, not a measurement | Read PA21 as ADC1_7 (expect near 0 V); lift R10 or move the press to another pin | The line reads high at rest |
| SysConfig says `No option named PA24` on the LP-MSPM0L1306 | PA24 is not a GPIO in the VQFN-32 package | Read BoosterPack pin 5 as analog channel 3 | The press reads under 2048 counts when pressed (`boards/mspm0l1306/pins.json`) |
| SysConfig says `No option named SPI2` or `UART0` on the LP-MSPM0G5187 | The chip's serial blocks are named UC0 to UC3 | Leave the peripheral unassigned and assign the pins | The generated header shows the block (`boards/mspm0g5187/pins.json`) |
| I2C scan finds nothing on the LP-MSPM33C321A | SDK READMEs list PB0 and PB1 for the BoosterPack I2C pins | Use SCL PB13 (pin 9) and SDA PB12 (pin 10) | The light sensor answers at 0x44 (`mspm33c321a/AGENTS.md`) |
| A GPIO set as output reads back 0 | The input stage is off | Call `DL_GPIO_initDigitalInputFeatures` | The read follows the pin (`boards/common/port_dl.c`) |
| Screen blank on the LP-MSPM0L1306 | Select is SWCLK, reset is SWDIO, data/command has no pin | Use the screen path with wires from pin 18 to 13 and pin 19 to 31, and software reset | `build.sh mspm0l1306 --lcd` builds it; not run |
| Pin 39 lights the red LED and the screen stays dark | The BoosterPack's J5 is on 1-2 (shipped) | Set J5 to 2-3 for the backlight | Screen lights (SLAU599B section 2.1) |

## What ran

- The drawing, font and init code against a model of the ST7735S RAM, on the Mac: all checks pass (`portability/boosterpacks/edumkii/test/test_gfx.c`).
- The hello example on the LP-MSPM0L1306: light sensor IDs and lux, joystick Y, accelerometer X and Y, microphone level (readings in `portability/boosterpacks/edumkii/README.md`, "What ran").
- Built, not run: the same example on the LP-MSPM33C321A, LP-MSPM0G3519, LP-MSPM0G3507 and LP-MSPM0G5187, and the screen path on the LP-MSPM0L1306.
- Not run on any board with this driver: the screen.
