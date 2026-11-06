# Fixes for an EDUMKII, SHARP128, AUDIO or BASSENSORSMKII BoosterPack that misbehaves on an MSPM0 or MSPM33 LaunchPad

Each row is a symptom, the fix and the cause (tags from the content standard: Setup or handling, Tool or docs, Chip limit, Third-party behavior, Unknown). The five that catch newcomers first are on the [BoosterPack guide](README.md#the-five-traps-newcomers-hit-first); pin maps and per-LaunchPad status are in [PINS.md](PINS.md).

Terms: [docs/vocab.md](../../docs/vocab.md). Checked 2026-10-20 with MSPM0 SDK 2.11.00.07, MSPM33 SDK 1.04.00.00 and TI Arm Clang 5.1.1.LTS.

## Fixes on the packs

| What you see | Do this | Cause | Source |
|---|---|---|---|
| AUDIO is silent, or only clicks once, with a correct SPI setup | Write each DAC8311 sample as one 16-bit frame with SYNC low for all 16 clocks. Two 8-bit transfers with SYNC raised between them cancel every write | Setup or handling | [audio README](audio/README.md) |
| AUDIO is silent with a correct frame | Drive BoosterPack pin 13 low. Floating or high leaves the amplifier in shutdown | Setup or handling | [audio README](audio/README.md) |
| The guide text gives other pins for AUDIO SYNC and SCLK | Use pin 12 (SYNC) and pin 7 (SCLK) from the schematic figure | Tool or docs | [audio AGENTS.md](audio/AGENTS.md) |
| EDUMKII screen blank on the LP-MSPM0L1306 | The screen select and reset land on SWCLK and SWDIO. Wire pin 18 to 13 and pin 19 to 31, build with `--lcd` (builds, not run) | Chip limit | [edumkii AGENTS.md](edumkii/AGENTS.md) |
| EDUMKII joystick press always reads pressed on the LP-MSPM33C321A | PA21 is the LaunchPad VREF- pin and R10 ties it to ground, a reading of the schematic and not a measurement. Read PA21 as analog (expect near 0 V) and lift R10 | Unknown (suspected: LaunchPad ties PA21 to ground) | [edumkii README](edumkii/README.md) |
| EDUMKII joystick X reads about 0 and never moves on the LP-MSPM0L1306 | Pull J4, which joins the light-sensor amplifier to that pin. Expected cause: J4 | Unknown (suspected: J4) | [edumkii README](edumkii/README.md) |
| EDUMKII I2C scan finds nothing on the LP-MSPM33C321A | The SDK READMEs list PB0 and PB1. Use SCL on PB13 (pin 9) and SDA on PB12 (pin 10) | Tool or docs | [edumkii AGENTS.md](edumkii/AGENTS.md) |
| SysConfig says `No option named PA24` on the LP-MSPM0L1306 | PA24 is not a GPIO in that package. Read the joystick press as analog channel 3 | Chip limit | [edumkii AGENTS.md](edumkii/AGENTS.md) |
| A GPIO set as an output always reads 0 | SysConfig leaves the input stage off. Call `DL_GPIO_initDigitalInputFeatures` | Tool or docs | [edumkii AGENTS.md](edumkii/AGENTS.md) |
| The screen stays dark and pin 39 lights the red LED | Move the BoosterPack's J5 to 2-3 (backlight) | Setup or handling | [edumkii AGENTS.md](edumkii/AGENTS.md) |
| Every delay hangs on the LP-MSPM0C1104 | SysTick does not run on that chip. Count milliseconds with a general-purpose timer (TIMG14 in the SHARP128 port) | Chip limit | [sharp128 README](sharp128/README.md) |
| A pack that uses header pin 6 conflicts with the thermistor on the LP-MSPM0G3507 | Pull the LaunchPad shunt J9, which ties PB24 (pin 6) to the thermistor | Setup or handling | [clash matrix, the table of which header pins each LaunchPad ties down](../../data/boosterpacks.json) |
| BASSENSORSMKII on the LP-MSPM0G5187: the TMP117 and BMI160 give no I2C answer | Humidity, light and magnet sensors work; for these two the cause was not found | Unknown (suspected: setup or handling) | [bassensorsmkii README](bassensorsmkii/README.md) |
| A header pin is dead on the LP-MSPM0G5187 | J2 and J4 were not fitted on the unit. Solder headers or wire from the pack's top socket | Setup or handling | [COMPAT-G5187.md](bassensorsmkii/COMPAT-G5187.md) |
| Zephyr stops with `undefined node label 'spi1'` (the pack README writes "no `spi1` node") | Use Zephyr main. Release 4.4.2 has no MSPM0 SPI driver | Third-party behavior | [zephyr README](zephyr/README.md) |
| A Zephyr GPIO call on a pin above PB27, or on port C, returns an error | The MSPM0 GPIO driver has a 28-entry port B table and no port C table for the G series. The pin cannot be driven until a table is added | Third-party behavior | [zephyr README](zephyr/README.md) |

## Related fixes on the same LaunchPad kits

Flashing with DSLite or OpenOCD, probes, and the NNC compiler for the NPU.

| What you see | Do this | Cause | Source |
|---|---|---|---|
| The LP-MSPM0C1104 shows a program a few milliseconds after reset when read in a second connection | A second OpenOCD attach resets the chip. Flash, run and read in one connection | Tool or docs | [sharp128 README](sharp128/README.md) |
| Loading an ELF onto the LP-MSPM0G3519 with DSLite stops near 91 percent with a verify mismatch | Flash a raw binary padded to 16 bytes at 0x0 instead | Tool or docs | [micropython README](../../ports/micropython/README.md) |
| OpenOCD reads device ID 0 on the LP-MSPM0G3519 after a sleeping image | Flash with DSLite; the first boot used it | Tool or docs | [micropython README](../../ports/micropython/README.md) |
| The LP-EM-CC35X1 console is silent after programming an image with a different flash layout | Run the Toolbox `full_flash_erase`, then program again. Also check the baud rate (115200 or 921600) | Setup or handling | [cc35x1 FAQ](../../cc35x1/FAQ.md) |
| A debug probe fails or the board vanishes from USB with a BoosterPack stacked | Remove the pack and replug. Seen on three boards; the cause is not confirmed | Unknown (suspected: setup or handling) | [experiment](../../docs/experiments/2026-10-20-boosterpack-on-at-plug-in.md) |
| NNC leaves a 1x1 layer on the CPU on the LP-MSPM0G5187 | A re-scaling shift of exactly 2^0 stops the match. Use 2^-1 and halve the result | Tool or docs | [npu-dsp README](../../mspm0g5187/npu-dsp/README.md) |
