# Pins and jumpers each BoosterPack (EDUMKII, SHARP128, AUDIO, BASSENSORSMKII) needs on each MSPM0 or MSPM33 LaunchPad

Every LaunchPad-by-pack status with its reason, where each pack's pin map lives, how the drivers are layered, how to add a LaunchPad, and what the Zephyr shields cover. The one-screen summary is the [BoosterPack guide](README.md); fixes for errors are in [TRAPS.md](TRAPS.md).

Terms: [docs/vocab.md](../../docs/vocab.md). Checked 2026-10-20 with MSPM0 SDK 2.11.00.07, MSPM33 SDK 1.04.00.00 and TI Arm Clang 5.1.1.LTS, except the EDUMKII on the LP-MSPM33C321A, checked 2026-10-21. 1 kB = 1,024 bytes.

- Pin numbers are BoosterPack header pins 1 to 40 (TI SLAA542, Figure 4). SWCLK and SWDIO are the two debug-probe lines. SysConfig is the pin-setup tool in CCStudio. DSLite and OpenOCD are flashing tools; NNC is the compiler for the G5187 NPU.
- **Hard** pin: a header pin the LaunchPad ties to something a jumper cannot free ("no pin" means the header pin is not connected; debug pin, probe serial line, or a part you cannot remove with a shunt). **Jumper** pin: a shunt or jumper on the LaunchPad frees it.
- The clash matrix (the table of which header pins each LaunchPad ties down; [data/boosterpacks.json](../../data/boosterpacks.json), built from the LaunchPad user guides) marks the G3507 pin 6 hard, but pulling J9 frees it, so the tables say "pull J9".

## BOOSTXL-EDUMKII per LaunchPad

Code, pins and a hello example: [edumkii/](edumkii/README.md).

| LaunchPad | Status | Why |
|---|---|---|
| LP-MSPM33C321A | Works | The screen, three Arduboy games (Snake, TileWorld, Crate Confusion) and the Game Boy emulator at the console's own speed (160 MHz, 8,167 frames in about two minutes, none late; details in [ports/peanut-gb](../../ports/peanut-gb/README.md)). Joystick press reads pressed (see [TRAPS.md](TRAPS.md)) |
| LP-MSPM0L1306 | Works, subset | Ran: light sensor, joystick Y, accelerometer X and Y, microphone. No screen: pins 13 and 17 are SWCLK and SWDIO, and pin 31 has no pin. No S1 or S2 (pins 32, 33 have no pin). No accelerometer Z (pin 25 is an op-amp input). The screen path builds with two jumper wires, not run. Pin 34 is also hard in the clash matrix; the reason is not recorded, and the sensors that ran do not use it |
| LP-MSPM0G3519 | Should work | Built, not run on the board. S1 and the console share PA10 and PA11 with the probe serial line |
| LP-MSPM0G3507 | Should work | Built, not run on the board. Jumpers J8, J9, J16, J17, J18 and J22 must change. 32 kB of RAM fits the driver but not a 32 kB frame buffer |
| LP-MSPM0G5187 | Should work | Built, not run on the board. The J2 and J4 headers were not fitted on the unit, so pins 11 to 20 and 31 to 40 need wires or soldered headers |
| LP-EM-CC35X1 | Not for this chip | Eight pins (2, 5, 6, 23, 24, 32, 34, 39) cannot be freed with a jumper: joystick, microphone and accelerometer inputs among them |

## BOOSTXL-SHARP128 per LaunchPad

Code, sizes and measurements: [sharp128/](sharp128/README.md).

| LaunchPad | Status | Why |
|---|---|---|
| LP-MSPM0C1104 | Works | Hello ran with no frame buffer: 609 B of RAM, the picture drawn in bands. The microSD card was not tested |
| LP-MSPM0G3519 | Should work | Built, not run on the board, 12.5 kB flash, 5.5 kB RAM |
| LP-MSPM0G3507 | Should work | Built, not run on the board. Pin 6 (PB24) is also the thermistor through J9; pull the jumper |
| LP-MSPM0L1306 | Should work | Built, not run on the board, screen only: the port leaves the card select and card detect (pins 8, 17) unused, so no card |
| LP-MSPM33C321A | Should work | Built, not run on the board. Jumper J3 off, because the boot button shares PA18, the panel power |
| LP-EM-CC35X1 | Not for this chip | Pins 2, 5 and 6 (panel power, DISP, panel select) cannot be freed with a jumper |

## BOOSTXL-AUDIO per LaunchPad

Code, tune format and tests: [audio/](audio/README.md).

| LaunchPad | Status | Why |
|---|---|---|
| LP-AM13E230 | Works | A 1 kHz tone and a four-voice tune, checked with the pack's own microphone: 1000.0 Hz, 99.9 percent of the energy within 50 Hz |
| LP-MSPM0G3519 | Should work | Built, not run on the board, 8.9 kB flash, 4.4 kB RAM |
| LP-MSPM33C321A | Should work | Built, not run on the board, 10.3 kB flash, 4.4 kB RAM |
| LP-MSPM0L1306 | Not for this chip | Pin 13 (the amplifier line) is SWCLK and pin 30 has no pin |

## BP-BASSENSORSMKII per LaunchPad

Code, driver list and the 100 Hz stream: [bassensorsmkii/](bassensorsmkii/README.md). Pin by pin for the G5187: [COMPAT-G5187.md](bassensorsmkii/COMPAT-G5187.md).

| LaunchPad | Status | Why |
|---|---|---|
| LP-MSPM0G5187 | Works, subset | Humidity and temperature (HDC2080), light (OPT3001) and the magnet sensor (DRV5055) read. The TMP117 and the BMI160 motion sensor did not answer, cause not found. The two LEDs and INT2 (pins 13, 39, 40) are unreachable |
| LP-MSPM0G3519 | Should work | Built, not run on the board |
| LP-MSPM33C321A | Should work | Built, not run on the board. The DRV5055 read returns -1 (the HSADC, the M33's ADC, is not ported). In SysConfig, pins 24, 27 and 28 have no GPIO group. The clash matrix lists no hard pin here. The OPT3001, TMP117 and DRV5055 need wires for power instead |
| LP-EM-CC35X1 | Not for this chip | Eight pins (2, 5, 6, 23, 24, 27, 28, 39) cannot be freed with a jumper |

## Where the pin maps are

| BoosterPack | Pin map per LaunchPad |
|---|---|
| EDUMKII | `edumkii/boards/<board>/PINS.md`, generated from `pins.json` |
| SHARP128 | the "Pins" table in [sharp128/README.md](sharp128/README.md#pins); SysConfig files in `sharp128/boards/<board>/` |
| AUDIO | the "Pins" table in [audio/README.md](audio/README.md#pins-boosterpack-numbers-slau670a-figure-12) |
| BASSENSORSMKII | `bassensorsmkii/boards/<board>/bassens.syscfg`, written by `boards/gen_syscfg.py` |
| Zephyr shields | `zephyr/dts/boosterpack/<board>.dtsi` |
| Which pins clash | [data/boosterpacks.json](../../data/boosterpacks.json), `clash_matrix` |

Stack the pack with pin 1 on pin 1.

## How the drivers are layered

Each pack has the same three layers.

1. **Portable core.** Plain C, no vendor includes: the screen, the DAC frame, the sensor registers. It is tested on a Mac against a model of the part.
2. **Port interface.** A small struct of functions the core calls: SPI write, GPIO, ADC, I2C, delays. EDUMKII needs six (`edumkii_port_t`), BASSENSORSMKII five (`bs_port.h`), AUDIO eleven (`board_audio.h`).
3. **`boards/<board>/`.** One pin table per LaunchPad. A script turns it into the SysConfig file and a C header, so the code and the pin table cannot disagree.

## Add a LaunchPad to EDUMKII

1. Copy `edumkii/boards/mspm0g3519/` to `boards/<new>/`.
2. Open `pins.json` and replace each LaunchPad pin with the one the board's user guide gives for that BoosterPack pin.
3. Add the board's SDK folder to the `case` in `edumkii/build.sh`.
4. Run `portability/boosterpacks/edumkii/build.sh <new>`. The build runs `tools/gen_board.py`, which writes the SysConfig file, the header and `PINS.md`.
5. Read the generated `PINS.md` for the pins that clash, and note them next to the pin.

## Zephyr shields

[zephyr/](zephyr/README.md) holds shields for EDUMKII, SHARP128 and AUDIO. They build on Zephyr main (4.5.0-rc2) for the LP-MSPM0G3519 and LP-MSPM0G3507, and nothing has been flashed. Zephyr 4.4.2 has no MSPM0 SPI driver, so every MSPM0 shield stops there. The LP-MSPM33C321A stops in devicetree on main. The AUDIO shield assumes the amplifier line is active high, which the bare-metal run contradicts (low plays); the shield has not run. Add the folder with `-DZEPHYR_EXTRA_MODULES=` and name the shield with `-DSHIELD=`.

Sources: pack READMEs linked above, TI SLAA542, SLAU599B, SLAU670A, SLAU969.
