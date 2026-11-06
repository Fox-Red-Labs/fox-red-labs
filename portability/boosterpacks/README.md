# BoosterPacks (EDUMKII, SHARP128, AUDIO, BASSENSORSMKII) on MSPM0, MSPM33 and AM13 LaunchPad kits

All four have plain-C drivers that build for MSPM0 and MSPM33 LaunchPad kits, and each runs on at least one board. AUDIO also builds and runs on the LP-AM13E230; the other three are not run there. No example for them on MSPM0 or MSPM33 was found in the SDKs checked (searched 2026-10-20). The table below has the pairings; the best result is the EDUMKII on the LP-MSPM33C321A: screen, three Arduboy games and the Game Boy emulator at full console speed, with one fault, a joystick press that reads pressed all the time.

Contents: [what works where](#what-works-where), [three steps to use one](#use-one-in-three-steps), [the five traps](#the-five-traps-newcomers-hit-first), [questions](#questions).

Checked 2026-10-20 with MSPM0 SDK 2.11.00.07, MSPM33 SDK 1.04.00.00 and TI Arm Clang 5.1.1.LTS (EDUMKII on the M33: 2026-10-21), on a Mac. 1 kB = 1,024 bytes. Terms ([docs/vocab.md](../../docs/vocab.md) has the rest):

- **Clash matrix**: this repo's table of which header pins each LaunchPad ties down ([data/boosterpacks.json](../../data/boosterpacks.json)).
- **Hard pin**: one a jumper cannot free. **Shunt**: the removable jumper cap.
- **SWCLK** and **SWDIO**: the debug-probe lines. **SYNC**: the DAC's chip-select line. **DAC8311**: the speaker's 1-channel digital-to-analog converter.
- **SysTick**: the Arm core's built-in tick timer. **TIMG14**: a general-purpose timer on the C1104.
- Pin numbers are BoosterPack header pins 1 to 40.

## What works where

| BoosterPack | Works | Should work (built, not run on the board) | Not for this chip |
|---|---|---|---|
| BOOSTXL-EDUMKII (screen, joystick, sensors) | LP-MSPM33C321A, with one fault (the joystick press reads pressed all the time). LP-MSPM0L1306, subset: light sensor, joystick Y, accelerometer X and Y, microphone; no screen (screen path builds, not run) | LP-MSPM0G3519, G3507, G5187 | LP-EM-CC35X1: eight pins cannot be freed with a jumper |
| BOOSTXL-SHARP128 (1-bit screen, microSD) | LP-MSPM0C1104. The microSD card was not tested | LP-MSPM0G3519, G3507, L1306 (screen only), M33C321A | LP-EM-CC35X1: panel power and select pins are fixed |
| BOOSTXL-AUDIO (DAC, speaker, microphone) | LP-AM13E230 (not a MSPM0 or MSPM33 board): tone and tune | LP-MSPM0G3519, M33C321A | LP-MSPM0L1306: the amplifier line is a debug pin |
| BP-BASSENSORSMKII (six sensors) | LP-MSPM0G5187, subset: three of six sensors read | LP-MSPM0G3519, M33C321A | LP-EM-CC35X1: eight pins cannot be freed with a jumper |

"Works" means it ran with the pack stacked and did what it should. "Should work" means it builds and has not run on the board. "Not for this chip" means a pin the chip or LaunchPad fixes blocks it. Per-LaunchPad detail, with every jumper and pin, is there too.

## Use one in three steps

1. Stack the pack on the LaunchPad with pin 1 on pin 1. Set the jumpers the pack's pin map lists (EDUMKII on the G3507 needs J8, J9, J16, J17, J18 and J22 changed).
2. Open the pack's folder ([edumkii](edumkii/README.md), [sharp128](sharp128/README.md), [audio](audio/README.md), [bassensorsmkii](bassensorsmkii/README.md)) and run its build script from `portability/boosterpacks/`, for example:

```bash
cd portability/boosterpacks
edumkii/build.sh mspm33c321a
```

   Board ids are `mspm0c1104`, `mspm0l1306`, `mspm0g3507`, `mspm0g3519`, `mspm0g5187`, `mspm33c321a` and `am13e230`; each pack's README lists the ones it supports.
3. Flash the `.out` file with CCStudio (TI's IDE) or DSLite (its command-line flasher) and read the console line, the screen or the tone. Each folder's hello example prints one line a second at 115200 baud on the LaunchPad kit's USB serial port (on a Mac it shows up as `/dev/cu.usbmodem*`; `screen /dev/cu.usbmodemXXXX 115200`).

## The five traps newcomers hit first

| What you see | Do this | Cause |
|---|---|---|
| AUDIO is silent, or only clicks once | Write each DAC8311 sample as one 16-bit frame with SYNC low for all 16 clocks. Two 8-bit transfers with SYNC raised between them cancel every write | Setup or handling |
| AUDIO is still silent with a correct frame | Drive BoosterPack pin 13 low. Floating or high leaves the amplifier in shutdown | Setup or handling |
| EDUMKII screen stays dark and header pin 39 lights the red LED | Move the pack's J5 to 2-3 (backlight) | Setup or handling |
| EDUMKII screen blank on the LP-MSPM0L1306 | The screen select and reset land on the debug-probe lines SWCLK and SWDIO (header pins 13 and 17). Add jumper wires from header pin 18 to 13 and from 19 to 31, as the [edumkii README](edumkii/README.md) lists, and build with `--lcd`. Which signal each wire carries is not recorded; builds, not run | Chip limit |
| A pack that uses header pin 6 conflicts with the thermistor on the LP-MSPM0G3507 | Pull the LaunchPad shunt J9, which ties PB24 (pin 6) to the thermistor | Setup or handling |

All 22 fixes, with the exact error strings: [TRAPS.md](TRAPS.md).

## Questions

**Which LaunchPad runs your pack?** The table above. If your board is not in it, [PINS.md](PINS.md) has the clash rule: a pin is a problem when the LaunchPad ties it to a debug line, a missing pin or a part a shunt cannot remove.

**Does the EDUMKII run games on the LP-MSPM33C321A?** Yes. Snake, TileWorld and Crate Confusion from the Arduboy port run on the screen, and the Game Boy emulator runs at the console's own speed: the chip clock is 160 MHz, 8,167 frames (about 137 s at 59.7 frames/s) went by with none late, and every second frame was drawn ([ports/peanut-gb](../../ports/peanut-gb/README.md)). The joystick press reads pressed all the time. Suspected cause: R10 ties PA21 to ground (a reading of the schematic, not a measurement). Lift R10 to test it; steps in [TRAPS.md](TRAPS.md).

**Why is my EDUMKII screen blank or dark?** On the LP-MSPM0L1306 the screen lines land on the debug pins SWCLK and SWDIO, so add the jumper wires in the table above and build with `--lcd` (builds, not run). On any board, if header pin 39 lights the red LED and the screen stays dark, move the pack's J5 to 2-3.

**Why is AUDIO silent or only clicking?** Check two things in order. First, each sample is one 16-bit frame with SYNC low for all 16 clocks. Second, pin 13 is driven low, because floating or high leaves the amplifier in shutdown. Use pin 12 for SYNC and pin 7 for SCLK; the guide text gives other pins.

**Why does pin 6 read wrong on the LP-MSPM0G3507?** The LaunchPad ties PB24 (pin 6) to its thermistor through J9. Pull J9. The clash matrix calls the pin hard, but the shunt frees it.

**Why do the TMP117 and the BMI160 not answer on the LP-MSPM0G5187?** Cause not found (suspected: unfitted headers). The HDC2080, OPT3001 and DRV5055 read fine; the TMP117 and the BMI160 gave no I2C answer. Separately, the J2 and J4 headers were not fitted on the unit, so a dead header pin may be a missing header: solder one or wire from the pack's top socket ([COMPAT-G5187.md](bassensorsmkii/COMPAT-G5187.md)).

**Why does an EDUMKII I2C scan find nothing on the LP-MSPM33C321A?** The SDK READMEs list PB0 and PB1. Use SCL on PB13 (pin 9) and SDA on PB12 (pin 10).

**Why does every delay hang on the LP-MSPM0C1104 (SHARP128)?** SysTick does not run on that chip. Count milliseconds with a general-purpose timer (TIMG14 in the SHARP128 port).

**Why does the board vanish from USB, or the probe fail, with a pack stacked?** Remove the pack and replug. Seen on three boards; cause not confirmed (Unknown). A GPIO set as an output that always reads 0 is a separate case: SysConfig leaves the input stage off, so call `DL_GPIO_initDigitalInputFeatures`. More in [TRAPS.md](TRAPS.md).

**Why is the EDUMKII joystick X stuck near 0 on the LP-MSPM0L1306?** Suspected cause: J4 joins the light-sensor amplifier to that pin. Pull J4 (Unknown, suspected: J4).

**Why does SysConfig say `No option named PA24` on the LP-MSPM0L1306?** PA24 is not a GPIO in that package. Read the joystick press as analog channel 3 (Chip limit). More in [TRAPS.md](TRAPS.md).

**Why does DSLite stop near 91 percent when loading an ELF onto the LP-MSPM0G3519?** Flash a raw binary padded to 16 bytes at 0x0 instead. Details in [TRAPS.md](TRAPS.md).

**Can you stack a pack on a LaunchPad that is not in the table?** Yes if no needed pin clashes; [PINS.md](PINS.md) has the clash rule. Not run pairings such as the EDUMKII on the LP-AM13E230 are not shown in the table because nobody ran them.

**Do you need to solder headers on the LP-MSPM0G5187?** On the unit used here the J2 and J4 headers were not fitted, so check yours.

**Do the packs work with Zephyr?** The shields build on Zephyr main for the LP-MSPM0G3519 and LP-MSPM0G3507 and nothing has been flashed. Release 4.4.2 (checked 2026-10-20) stops with `undefined node label 'spi1'` because it has no MSPM0 SPI driver, so use main. The AUDIO shield assumes an active-high amplifier line while the bare-metal run plays with it low, and it has not run. Details: [PINS.md](PINS.md#zephyr-shields).

## More

- [TRAPS.md](TRAPS.md): every fix, including flashing with DSLite and OpenOCD, SysTick on the C1104, SysConfig `No option named PA24`, and Zephyr GPIO limits.
- [PINS.md](PINS.md): per-LaunchPad status and reasons, pin map locations, the three driver layers, how to add a LaunchPad, Zephyr shields.
- [Porting lessons](../../docs/guides/porting-lessons.md): what porting 20 projects to TI microcontrollers taught us, including the display, clock and flashing traps seen with these packs.
- [Chip capability map](../../docs/guides/chip-capability-map.md): which project ran on which chip, and what each chip's peripherals did in the ports.

Sources: pack READMEs linked above, TI SLAA542 (BoosterPack pinout), the clash matrix [data/boosterpacks.json](../../data/boosterpacks.json).
