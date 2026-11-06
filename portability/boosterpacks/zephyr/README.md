# BoosterPack shields for Zephyr: BOOSTXL-EDUMKII, BOOSTXL-SHARP128 and BOOSTXL-AUDIO

Three TI BoosterPack plug-in modules as Zephyr shields, with the connector maps for four LaunchPad kits: BOOSTXL-EDUMKII (screen, buttons, joystick), BOOSTXL-SHARP128 (memory display) and BOOSTXL-AUDIO (DAC and speaker amplifier).

**Status: Should work; the shields build and none has been flashed.** On Zephyr main the screen, input and DAC samples build for the LP-MSPM0G3519 and LP-MSPM0G3507. The LP-MSPM33C321A stops in devicetree (no SPI node, no GPIO driver for it). The LP-EM-CC2340R5 builds just the audio shield, because the other two need header pins that LaunchPad does not connect. Zephyr 4.4.2 builds none of the MSPM0 cases: it has no MSPM0 SPI driver.

Terms used here are in [the vocabulary](../../../docs/vocab.md).

## Use it

Add this folder as an extra module and name the shield:

```bash
west build -b lp_mspm0g3519 samples/drivers/display -- \
  -DZEPHYR_EXTRA_MODULES=/path/to/portability/boosterpacks/zephyr \
  -DSHIELD=boostxl_edumkii
```

Shield names: `boostxl_edumkii`, `boostxl_sharp128`, `boostxl_audio`, and `boostxl_edumkii_gpiospi` (G3507 only, screen bus on GPIO pins, for Zephyr releases without an MSPM0 SPI driver). One BoosterPack shield per build: each one brings its own copy of the LaunchPad kit's connector map.

| Shield | Sample to start with | What it gives |
|---|---|---|
| `boostxl_edumkii` | `samples/drivers/display`, `samples/subsys/input/input_dump` | 128 x 128 ST7735S screen, backlight, buttons S1 and S2, joystick press and (G3519, G3507) the two joystick axes as input events |
| `boostxl_sharp128` | `samples/drivers/display` | 128 x 128 one-bit Sharp memory display |
| `boostxl_audio` | `samples/drivers/dac` | DAC8311 14-bit DAC, amplifier and microphone supply switched on |

Boards with a shield overlay: `lp_mspm0g3519`, `lp_mspm0g3507`, `lp_mspm33c321a`, `lp_em_cc2340r5`. The connector maps live in `dts/boosterpack/`; each shield's `boards/<board>.overlay` includes one.

## Build results

`build_matrix.sh` builds every shield, board and sample and prints one line each. Zephyr main is 1405adc9a (4.5.0-rc2, 2026-10-15) with hal_ti 53c80140. The release is Zephyr 4.4.2. GCC 12.2, `-Os`. Flash and RAM are from the linker summary. Every row is in `RESULTS-ROWS.csv`.

| Board | Shield and sample | Zephyr main | Zephyr 4.4.2 |
|---|---|---|---|
| LP-MSPM0G3519 | EDUMKII, display | builds, 32.0 kB flash, 39.4 kB RAM | no `spi1` node |
| LP-MSPM0G3519 | EDUMKII, input_dump | builds, 20.8 kB, 8.2 kB | no `spi1` node |
| LP-MSPM0G3519 | SHARP128, display | builds, 31.4 kB, 40.0 kB | no `spi1` node |
| LP-MSPM0G3519 | AUDIO, dac | builds, 18.8 kB, 4.4 kB | no `spi1` node |
| LP-MSPM0G3507 | EDUMKII, display | builds, 31.9 kB, 22.9 kB | no `spi1` node |
| LP-MSPM0G3507 | EDUMKII, input_dump | builds, 20.9 kB, 9.2 kB | no `spi1` node |
| LP-MSPM0G3507 | SHARP128, display | builds, 31.3 kB, 23.6 kB | no `spi1` node |
| LP-MSPM0G3507 | AUDIO, dac | builds, 18.7 kB, 4.3 kB | no `spi1` node |
| LP-MSPM0G3507 | EDUMKII over GPIO SPI, display | builds, 34.1 kB, 23.1 kB | links 39 kB over the 32 kB of RAM |
| LP-MSPM33C321A | all four | stops in devicetree | board not in 4.4.2 |
| LP-EM-CC2340R5 | EDUMKII, SHARP128 | stops in devicetree (pins not connected) | same |
| LP-EM-CC2340R5 | AUDIO, dac | builds, 17.7 kB, 4.5 kB | builds, 14.7 kB, 4.3 kB |

The flash and RAM totals in Zephyr's summary come from the board files, which describe the first bank only: 256 kB and 64 kB for the G3519.

## Limits

- **SPI driver.** Zephyr main has `drivers/spi/spi_mspm0.c` (merged 2026-04-21, commit 7e95d5f5d2) and a `ti,mspm0-spi` binding. Zephyr 4.4.2 has neither.
- **I2C.** No MSPM0 I2C driver is in main (pull request 117311 is open), so the OPT3001 light sensor on the EDUMKII has no driver path.
- **GPIO tables.** The MSPM0 GPIO driver in main has a pin table for 28 pins on port B of the G series and none for port C. BoosterPack pins on PB29 to PB31 and on port C (G3519 pins 5, 7, 8, 9, 10, 37, 38) cannot be driven as GPIO until the table is extended. On the G3519 that includes pin 5, the Sharp display's DISP line. The shield builds; the run will show whether the pin call returns an error.
- **Pin numbers on the G3519.** The driver's port A and port B tables were checked against a DriverLib display project for the LP-MSPM0G3519 for PA10, PA11, PA14, PB7, PB8, PB9, PB11, PB12 and PB14: all agree. The other entries are unchecked.
- **Analog.** The joystick uses Zephyr's analog-axis input driver on the MSPM0 ADC. The accelerometer and microphone are not wired in.
- **Pin 39.** The EDUMKII backlight and the red LED share BoosterPack pin 39 through jumper J5. The shield assumes J5 on 2-3 (backlight).
- **Pins 33 and 34.** On the G3519 and G3507 these are the debug probe's serial lines until the LaunchPad kit's UART jumpers move. Buttons S1 and S2 need that move.
- **Audio.** The amplifier line (pin 13) is assumed active high and the DAC chip select (pin 12) active low. Neither is confirmed by a run.
- **LVGL.** The shields set no LVGL options because the option names changed between releases. Use `CONFIG_LV_Z_BITS_PER_PIXEL=16` for the EDUMKII and a one-bit color format for the SHARP128 as the release documents it.

## Suggested run order

Start with the SHARP128 on the G3507 or G3519 because it needs the fewest pins, then the EDUMKII screen, then its inputs. Pin maps and jumpers are in [PINS.md](../PINS.md).

## Files

| Path | What it is |
|---|---|
| `zephyr/module.yml` | Registers the folder as a Zephyr module with board and devicetree roots |
| `dts/boosterpack/*.dtsi` | BoosterPack header maps (pin 1 to 40 to GPIO) and SPI labels per LaunchPad |
| `boards/shields/<shield>/` | Shield overlays, Kconfig and per-board overlays |
| `build_matrix.sh` | Builds every combination; touches no hardware |
| `RESULTS-ROWS.csv` | Build rows in the results-store format |

License: Apache-2.0, like Zephyr. The overlays use register values and pin maps from TI user guides (SLAU599B, SLAU786, SLAU670A, SLAU947, SLAU969), Zephyr's ST7735R and LS0xx drivers and the public Zephyr shield examples as the reference.
