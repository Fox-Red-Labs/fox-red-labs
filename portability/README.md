# Code portability across TI families

Can code written for one TI chip family build and run on another? This folder answers that cell by cell, from runs, not from reading.

Terms: [docs/vocab.md](../docs/vocab.md). 1 kB = 1,024 bytes.

Every example in every SDK on this Mac against every board on the bench, with a verdict per cell before anything is built, and the queue of cross-builds that turns those verdicts into rows here: [everywhere/README.md](everywhere/README.md).

The record is `matrix.csv`. One row per test. `render.py` turns it into a page (`site/portability.html`).

How to port into and across one family (MSPM0, MSPM33, AM13), with a part-to-part map, a driver-and-function map per layer and a generated evidence file per family: [families/README.md](families/README.md).

## Summary (2026-10-05)

- **The TI Drivers API is the same on MSPM0 and the wireless parts.** GPIO, SPI, ADC and I2C target have the same function names in the MSPM0 SDK and the F3 SDK (CC23xx, CC27xx). I2C has one extra on F3. UART is the same 13 calls, renamed `UART2_*` on F3.
- **The examples around it are not portable as shipped.** MSPM0 app code built for the CC2340R5: 0 of 10 unchanged, 10 of 10 with a 47-line shim and no app edits (5 of them ran on the CC2340R5). CC2340R5 app code built for the G3507: 5 of 10 unchanged, 10 of 10 with a 40-line shim.
- **The biggest single gap is in MSPM0's GPIO call.** MSPM0 apps OR a pin-mux index into the `GPIO_setConfig` flags (`CONFIG_GPIO_LED_0_IOMUX`). That is chip detail inside the one call that is meant to be portable. F3 apps do not need it.
- **MSPM0's TI Drivers board config is hand-written C, not SysConfig.** No MSPM0 TI Drivers example has a `.syscfg` file. Its config defines app constants (`CONFIG_UART_BAUD_RATE`) the F3 config does not, and F3 names (`CONFIG_UART2_0`, `CONFIG_GPIO_LED_ON`) are missing on MSPM0.
- **The MSPM0 FreeRTOS examples do not build from a terminal as shipped.** All 48 makefiles link FreeRTOS from a CCStudio workspace with linker syntax tiarmclang rejects, and the kernel makefile they point at does not exist. `crossbuild/build_mspm0_freertos.sh` and `patch_mspm0_rtos_makefile.py` work around both.
- **MSPM33 has no TI Drivers at all.** Its DriverLib shares every GPIO, UART and timer function name with MSPM0, but SPI, I2C and ADC are new blocks (UNICOMM, HSADC).
- **The AM13E SDK ships an STM32G4 HAL compatibility layer** (GPIO, SPI, UART, DMA). It is the only cross-vendor layer found in any TI SDK on this Mac.
- **Inside one SDK it already works.** All 33 driver examples the CC2340R5 and the CC2755P10 both ship build for either board, and the app source is byte-identical; only SysConfig and the linker file differ. That is the bar for MSPM0 to F3.
- **MSPM0 up and down the family (2026-10-06, LP-MSPM0L1306).** Changing only the device in SysConfig, 101 of 170 L1306 examples build for the G3507, but 22 of 270 G3507 examples build for the L1306; one G-series SysConfig property stops 136 of them. All 133 ported images that built (82 from the G3507, 51 from the C1104) ran on the L1306 with no crash. Prebuilt G3507 model libraries ran unchanged with the same answers. Detail: the [LP-MSPM0L1306 README](../mspm0l1306/README.md).
- **Plain C and Zephyr apps already move.** The mcu-bench suite ran unchanged in its maths on the G3507 and the CC2340R5; only the timer, LED pin and one interrupt number needed board code.

**Ported MSPM0 code runs on the CC2340R5.** The 5 G3507 apps that built with the shim were flashed to the CC2340R5 on 2026-10-06, each right after TI's own CC2340R5 build of the same example. All 5 started, printed their MSPM0 messages over UART (`I2C Initialized!`, `Controller SPI initialized`, which exist only in the MSPM0 source), reached the same waiting point as TI's build, and left every GPIO output and output-enable bit the same, except one LED that the two sources set differently on purpose. Behavior verified at the level these examples can show without a second board or sensor.

## Method

### What a test is

A test moves one piece of code from a **source** chip to a **target** chip at one **layer**:

| Layer | What moves | Example |
|---|---|---|
| Plain C | Code with no TI headers | CoreMark, a filter, a parser |
| Zephyr app | Code written to Zephyr's APIs | mcu-bench |
| TI Drivers | Code written to `ti/drivers/*.h` | SDK driver examples |
| DriverLib | Code written to `DL_*` register-level calls | MSPM0 bare-metal examples |
| Vendor HAL | Code written for another vendor | STM32 HAL on AM13E |
| Radio stacks | BLE, Zigbee, Thread apps | Only between radio parts |

Start from TI's own examples wherever a twin exists in both SDKs. They are TI's code, so a failure is a gap in TI's portability, not in this repo's.

### The ladder (each test climbs as far as it can)

1. **Inspected.** Header and folder comparison only. Says what should be possible.
2. **Unchanged build.** Source file dropped into the target's twin project, no edits.
3. **Shim build.** Same, plus a force-included header and stub headers in `shim/`. No edits to the app. Every shim line is one named gap.
4. **Edited build.** Smallest app or config edits that make it build. Count the lines.
5. **Ran.** Flashed to the target. Did it start, did it crash?
6. **Behavior verified.** Same output as the target's own example (UART text, LED, pin capture).

The matrix records the highest rung reached and the count at each rung. Its `with_glue` column is rung 3 for any glue that leaves the app untouched: a shim header, or an automatic config fix such as unlocking pin assignments (the C1104 rows).

### Verdict words

Four verdicts, plus two for cells nobody can fill:

| Verdict | Meaning |
|---|---|
| **Works** | Built and ran on the target with no edits, or with edits a beginner would make. |
| **Should work** | Same API on both sides; only naming, packaging or build glue is in the way. A small TI fix would make it Works. |
| **Could work** | Real porting effort: a missing peripheral, a different block, a rewrite of one module. |
| **Does not make sense** | The target lacks the hardware or the purpose (a radio app on a chip with no radio). |
| **Does not exist** | The layer is missing on one side, so there is nothing to port to. |
| **To test** | Known to exist, not run. |

Every Should work and Could work row names its blocker.

### Rules

- Build with the same tools as every other run here: TI Arm Clang 5.1.1 from CCStudio 21.0.1, SysConfig 1.26.3, the SDKs in `/Applications/ti`. Never write into `/Applications/ti`; copy examples to scratch first.
- Build both twins unchanged first (the baseline). A pair only counts if the target's own example builds.
- Shims hold renames and empty stubs only. No logic. If logic is needed, it is an edit, and it is counted.
- Each build is one line of raw data; the matrix row carries the counts.

## How to run it

```bash
cd portability/crossbuild
SP=<scratch dir>
./m0_to_f3.sh  $SP/m0f3 data/m0_to_f3.jsonl                 # MSPM0 apps on CC2340R5 (baseline, swapped, shim)
./build_mspm0_freertos.sh $SP/m0rtos                          # MSPM0 FreeRTOS library, once
./f3_to_m0.sh  $SP/f3m0 data/f3_to_m0.jsonl $SP/m0rtos/freertos.lib   # CC2340R5 apps on G3507
./f3_to_f3.sh  $SP/r5p10 data/r5_to_p10.jsonl                 # CC2340R5 apps on CC2755P10, same SDK
python3 ../render.py                                          # matrix.csv to site/portability.html
```

The ten twin pairs: `adc_singlechannel`, `gpio_interrupt`, `i2c_controller`, `i2c_target`, `i2c_tmp`, `spi_controller`, `spi_peripheral`, `uart_callback`, `uart_echo`, `empty_freertos` on MSPM0, against `adcsinglechannel`, `gpiointerrupt`, `i2ccontroller`, `i2ctarget`, `i2ctmp`, `spicontroller`, `spiperipheral`, `uart2callback`, `uart2echo`, `empty` on F3. Every MSPM0 driver example has an F3 twin, under a different folder name.

## Results

### TI Drivers, MSPM0 G3507 and CC2340R5

| Example | G3507 code on CC2340R5: unchanged / shim | CC2340R5 code on G3507: unchanged / shim |
|---|---|---|
| empty | no / **yes** | **yes** / yes |
| gpio interrupt | no / **yes** | **yes** / yes |
| ADC single channel | no / no (LED name) | **yes** / yes |
| I2C controller | no / **yes** | **yes** / yes |
| I2C TMP sensor | no / no (UART buffer constant) | **yes** / yes |
| I2C target | no / no (different app design) | no / no (I2C and LED names) |
| SPI controller | no / **yes** | no / no (second LED not in MSPM0 config) |
| SPI peripheral | no / **yes** | no / no (second LED) |
| UART callback | no / no (config constants, DriverLib call) | no / no (UART and LED names) |
| UART echo | no / no (config constants, DriverLib call) | no / no (UART and LED names) |
| **Builds** | **0 / 5** | **5 / 5** |

What the shim had to cover (`shim/mspm0_on_f3/fr_mspm0_on_f3.h`, one gap per line):

- 19 UART renames (`UART_*` to `UART2_*`)
- 4 SPI frame-format renames (`SPI_MOTO4_POL0_PHA0` to `SPI_POL0_PHA0`). Same names mean different numbers: MSPM0's is 4, F3's is 0, and MSPM0 also has 3-wire modes F3 lacks. Source can move; libraries and binaries cannot.
- 2 GPIO flags MSPM0 has and F3 does not (`GPIO_CFG_HIZ_ON/OFF`)
- Stub headers: `ti/display/DisplayUart.h` (F3 calls it `DisplayUart2.h`) and `ti_msp_dl_config.h` (MSPM0 DriverLib config, included by MSPM0 TI Drivers examples)
- Every `CONFIG_*_IOMUX` name set to 0, generated per file

The reverse shim (`shim/f3_on_mspm0/`) is the mirror image. It rescued nothing extra: the five F3 apps that fail on MSPM0 fail on board config names, which a shim should not paper over.

Image sizes are close. The same CC2340R5 app source is 13 to 27 kB of flash on either chip under FreeRTOS, within 10 percent either way: the G3507 build is larger for the ADC and I2C apps (6 to 9 percent) and smaller for the GPIO and empty apps (4 to 6 percent).

### On silicon: G3507 code on the CC2340R5

`crossbuild/run_cc23.py` backs up the whole flash and CCFG (outside the repo: it holds the Zigbee network key), flashes TI's native build then the ported build of each example while listening on the backchannel UART, reads the GPIO output (DOUT) and output-enable (DOE) registers, then writes the backup back. The backup round trip was proven first: read back after restore matched byte for byte, apart from 72 bytes the Zigbee firmware wrote into blank settings storage after it rebooted.

| Example | Native CC2340R5 build | Ported G3507 source | Same? |
|---|---|---|---|
| empty | DOUT 40600000, DOE 00400000 | identical | yes |
| gpiointerrupt | DOUT 40600000 | DOUT 40200000 (LED 0 off) | as written: the MSPM0 source turns LED 0 off at start, the F3 source turns it on |
| i2ccontroller | "Starting the i2ccontroller example", "Sending GET_STATUS until target responds" | same, plus MSPM0's "I2C Initialized!" | yes; both wait for an I2C target |
| spicontroller | "Starting the SPI controller example", wiring note | "Starting...", "Controller SPI initialized" | yes; both wait for the second board |
| spiperipheral | "Starting the SPI peripheral example", wiring note | "Starting the SPI peripheral example" | yes |

Each flash took about 20 s with DSLite. Two DSLite traps cost time: its memory read length is in 32-bit words, not bytes, and `objcopy` cannot place a raw binary at the CCFG address, so the script writes the restore HEX itself.

### TI Drivers inside the F3 SDK, CC2340R5 and CC2755P10

33 examples exist for both boards. All 33 build for the CC2755P10 from the CC2340R5's app file, because the app files are identical: TI keeps board differences in SysConfig. The CC2755P10 images are 2.5 kB larger for the same app (9 to 18 percent).

### DriverLib, MSPM0 C1104 and G3507

From the example sweep: C1104 code moves up to the G3507 in 65 of 91 examples once pins are unlocked. G3507 code moves down in 48 of 270, 32 of those only after SysConfig lines are dropped, some silently.

### Across the wider family, by inspection

| From MSPM0 to | TI Drivers | DriverLib GPIO | DriverLib UART | DriverLib timers | SPI, I2C, ADC |
|---|---|---|---|---|---|
| MSPM33 (SDK 1.04) | none in SDK | 296 of 296 names | 189 of 189 | 294 of 294 | new blocks (UNICOMM, HSADC) |
| AM13E230 (SDK 26.01) | none in SDK | 51 of 70 names | not compared | not compared | not compared |
| CC23xx, CC27xx (F3 SDK) | 5 of 6 drivers identical, UART renamed | DriverLib differs entirely | | | |

### DriverLib, MSPM0G3519 and MSPM33C321A on silicon, both ways (2026-10-24)

GPIO, SysTick, timer, PWM, SPI (internal loopback), UART (internal loopback) and CRC examples run on the other chip with the app files untouched: 7 checked of 8 run on the LP-MSPM33C321A, 9 of 9 on the LP-MSPM0G3519 (one needed 5 app lines). I2C and ADC stop at build because the MSPM33 has the UNICOMM and HSADC blocks. Migration notes with the run behind each line: [everywhere/PORTING.md section 7](everywhere/PORTING.md).

## The radio version: three layers per demo

The same seam idea applied to the Bluetooth ports, measured. The format in one vendor-free header with a host test, four verbs of vendor glue in one file per family, configuration as a 9 to 12 line text diff of the vendor's own example.

## Zephyr, Arduino and FSP ports (folders)

Runs that put another software layer on a TI board. Each README has a "Set up once" table, the build and flash commands, the results with their logs, and what went wrong.

| Folder | What it holds | Boards |
|---|---|---|
| `arduino/zephyr/` | Arduino sketches (Blink, AnalogReadSerial, interrupts, PWM, SPI, Wire) through ArduinoCore-zephyr, one overlay per board | LP-EM-CC2340R5, LP-MSPM0G3519 |
| `zephyr-mspm0/` | Zephyr main with the MSPM0 I2C pull request and a flash driver; test apps, run scripts, prepared upstream pull requests | LP-MSPM0G3519 |
| `fsp-on-mspm0/` | A 304-line layer that maps Renesas FSP calls to MSPM0 DriverLib; results in `../ports/renesas-ra-fsp/` | LP-MSPM0G3519 |

MicroPython on the CC2340R5 through Zephyr is in `../ports/micropython/zephyr-cc2340r5/`.

## Take a port into your own project

Each row is a layer you can lift out of this repo: copy the files named in the folder's "Take it with you" section, or hand the folder to your coding assistant together with its `AGENTS.md`. Paths are from the repo root. The build command for each, run from a fresh folder, is in the same section.

| Layer | What it gives you | Files to copy | AGENTS.md | Proven on |
|---|---|---|---|---|
| Arduino sketches on Zephyr | Unchanged `.ino` files (Blink, AnalogReadSerial, interrupts, PWM, SPI, Wire) built through ArduinoCore-zephyr | `portability/arduino/zephyr/`: `app/`, `variants/<board>/`, `sketches/`, and for the CC2340R5 `serial-blocking-write.patch` | `portability/arduino/zephyr/AGENTS.md` | LP-EM-CC2340R5, LP-MSPM0G3519 |
| FSP calls on MSPM0 DriverLib | Renesas `ra-fsp-examples` source, unchanged, on TI DriverLib | `portability/fsp-on-mspm0/` (`include/`, `src/`, `boards/`) and the example's `third-party/renesas/<example>/src/` | `portability/fsp-on-mspm0/AGENTS.md` | LP-MSPM0G3519 |
| Zephyr MSPM0 I2C and flash drivers | The open I2C pull request and a flash driver, with three test apps | `portability/zephyr-mspm0/`: `patches/`, `drivers/`, `apps/<app>/` | `portability/zephyr-mspm0/AGENTS.md` | LP-MSPM0G3519 |
| MicroPython on Zephyr | MicroPython 1.29.0 trimmed to fit 36 kB of RAM | `ports/micropython/zephyr-cc2340r5/`: `prj_cc2340r5.conf`, `lp_em_cc2340r5.overlay`, optionally `pwm-duty-u16-overflow.patch` | `ports/micropython/zephyr-cc2340r5/AGENTS.md` | LP-EM-CC2340R5 |
| Nordic phone-app services | Nordic LED Button and UART services on TI's `basic_ble` | `ports/nordic-apps/`: `build.sh`, `basic_ble.patch`, `app/`, `src/`, `services/` | `ports/nordic-apps/AGENTS.md` | LP-EM-CC2340R5, from a Mac only |
| Wi-Fi setup over Bluetooth | The network name and password written over Bluetooth LE to a CC35X1 | `ports/wifi-provisioning-ble/provision.py`; the image from `cc35x1/chained-demo/` and `portability/iot-chain/` | `ports/wifi-provisioning-ble/AGENTS.md` | LP-EM-CC35X1 (boot and fallback only; the join has not run) |


## How it is shown

- **This folder** is the engineer's record: scripts, raw data, shims.
- **`site/portability.html`** is the one-page view: a grid of families by layer, each cell colored by verdict, with the blocker on each card. Generated from `matrix.csv` by `render.py`, never hand-edited.
