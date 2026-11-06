# Port drivers and code to the CC35xx Wi-Fi family (LP-EM-CC35X1) and between it and the CC23xx and CC27xx boards

TI Drivers app source moves between the Wi-Fi SDK and the F3 SDK unchanged: 11 of 11 same-name driver examples built and ran on the LP-EM-CC35X1 from the CC2340R5 side, and 11 of 11 ran on the CC2340R5 from the Wi-Fi side (matrix X26 and X27). The shared 81-line app runs on the LP-EM-CC35X1 with one 57-line TI Drivers glue file. What does not move: there is no Zephyr board in 4.4.2, no Arduino core, MicroPython port, PlatformIO platform or Rust crate for it, no Thread (the chip has no 802.15.4), no IAR or Keil support, and the program in its external flash cannot be backed up.

Terms: [docs/vocab.md](../../../docs/vocab.md) and this family's [FAQ](FAQ.md). **TI Drivers** is TI's driver layer shared by the F3 and Wi-Fi SDKs; **activation** is the one-time step that burns a security key into the chip's fuses; the **Toolbox** is TI's SimpleLink Wi-Fi Toolbox, the command-line tool that activates, signs and programs; **SysConfig** writes pin and driver code from a `.syscfg` file; **OTA** is an over-the-air update; **MQTT** is the publish and subscribe message protocol. 1 kB = 1,024 bytes. **X26** and the other X numbers are row ids in `portability/matrix.csv`. The generated record: [EVIDENCE.md](EVIDENCE.md). The assistant's copy: [AGENTS.md](AGENTS.md).

Contents: the part and its neighbors, a ten-minute path, the one-command build, the driver and function map, how to port into the family, moving inside it, moving to and from other families, what was measured, errors, sources.

## The board (one part held)

| Part | Kit | Core and clock | Flash | RAM | On the board | Source |
|---|---|---|---|---|---|---|
| CC3551E | LP-EM-CC35X1 Rev A | Cortex-M33 at 160 MHz; Wi-Fi 6 on 2.4 and 5 GHz; Bluetooth LE 5.4 | 8 MB external (IS25WJ064F) | 1.1 MB | RGB LED, two buttons, temperature sensor, accelerometer, chip antenna, 40 header pins; no debugger, so an LP-XDS110ET or LP-XDS110 plus a second USB-C cable | `cc35x1/AGENTS.md` |

Board folder: [`cc35x1/`](../../../cc35x1/AGENTS.md) (`README.md`, `AGENTS.md`, `FAQ.md`). No other CC35xx part is held.

### Part-to-part map: the CC3551E against the F3 radio parts

The F3 parts are the nearest neighbors: same TI Drivers names, same FreeRTOS. A dash means nothing is on record.

| | CC3551E (LP-EM-CC35X1) | CC2340R5 | CC2755P10 |
|---|---|---|---|
| Core, clock | Cortex-M33, 160 MHz | Cortex-M0+, 48 MHz | Cortex-M33, 96 MHz |
| RAM | 1.1 MB | 36 kB | 162 kB |
| Program storage | 8 MB external QSPI flash behind the chip's security | 512 kB internal | 1,024 kB internal |
| SDK | Wi-Fi SDK 10.20.00.39, Toolbox 4.3.23, SysConfig 1.28.0 | F3 SDK 9.21.00.36 | F3 SDK 9.21.00.36 |
| First use | Activation, one time, cannot be undone | none | none |
| Backup | none possible; the Toolbox has no read-back | DSLite reads | DSLite reads |
| Program and reset | Toolbox `programming`; it does not reset the board, press S1 or run `xds110reset` | DSLite flash | DSLite flash |
| Radio | Wi-Fi and Bluetooth LE; no 802.15.4, so no Thread or Zigbee | Bluetooth LE and 802.15.4 | 2.4 GHz radio |
| `DWT->CYCCNT` | reads 0 in programs loaded this way; time with the 1 ms FreeRTOS tick | - | - |
| CoreMark, bare metal, -O3 | 718.91 (4.49 per MHz); cause of the high figure unexplained | 111.09 (2.31 per MHz) | 426.31 (4.44 per MHz) |

Sources: `cc35x1/AGENTS.md`, `cc35x1/RESULTS-cc35x1.md`, `cc2340r5/AGENTS.md`, `cc2755p10/AGENTS.md`, `portability/families/radio/README.md`.

## Path: carry one F3 TI Drivers example into a Wi-Fi SDK project

Run it on the LP-EM-CC35X1. The commands are the ones recorded in `cc35x1/AGENTS.md` and the 2026-10-24 twin run, put in order; the path as a whole was not timed.

1. Terminal, board and probe plugged in, second USB-C cable in the board. Check activation without changing anything (from the Toolbox folder):

   ```bash
   ./simplelink-wifi-toolbox programmer -i XDS110 -param1 <probe serial> get_fuse_data --flash_type IS25WJ064F --activation_type sdk_example_key --verbose
   ```

   If it prints `Life cycle OPERATIONAL is not valid`, the chip is not activated: stop and ask before running the one-way `custom_activation` step in `cc35x1/README.md`.
2. No backup is possible. Save the output of the command above instead.
3. Terminal, no board. Copy `<SDK>/examples/rtos/LP_EM_CC35X1/drivers/empty`, overwrite its `.c` file with the F3 example's, and build in `freertos/ticlang` with four paths on the `make` line (`cc35x1/AGENTS.md` "Start here"; `GCC_ARMCOMPILER=` stays empty).
4. Terminal. Sign the images with the Toolbox image builder, then program with absolute paths in `tool_settings.json`:

   ```bash
   ./simplelink-wifi-toolbox programmer -i XDS110 -param1 <probe serial> programming --tool_settings <absolute path to tool_settings.json>
   ```

   The Toolbox does not reset the board; press S1 or run `xds110reset -s <probe serial>`. You should see the red LED toggle once a second.
5. Restore by reprogramming TI's prebuilt Network Terminal, `programmer/binaries/pre_complied_application/vendor_app_no_ota/vendor_app.CC35XXE.out` in the Toolbox folder. It is not a byte-exact image of what shipped.

## One-command build

```bash
portability/three-ways/tidrivers/build_cc35x1.sh
```

It copies the Wi-Fi SDK's `drivers/empty` example for the LP-EM-CC35X1, swaps in the shared `app.syscfg` and the 57-line glue file, adds the 81-line shared `app.c`, and builds: 13.1 s clean. It does not sign or flash.

## Driver and function map

Columns: TI Drivers (Wi-Fi SDK examples), F3 TI Drivers code carried over, and the other layers. A dash means nothing is on record.

| Function | TI Drivers (Wi-Fi SDK examples) | F3 code carried over | Zephyr | MicroPython | Arduino |
|---|---|---|---|---|---|
| GPIO | The LP-EM-CC35X1 `empty` twin ran; its LED pins were not observed (X26) | Works: LED pin toggling is among the 7 of 11 that show source behavior on the CC2340R5; `gpiointerrupt` only starts up (X27) | TI's fork links `blinky` (65 kB flash); not flashed, because the signed-image step fails on Toolbox 4.3.23 | Not for this chip: no port found | Not for this chip: no core found 17 Oct 2026 |
| UART | Works: `uart2echo` echoes; SDK app UART at 115200 | Works: UART echo among the 7 that show behavior (X27) | `hello_world` links | Not for this chip | Not for this chip |
| ADC | Works: `adcsinglechannel` prints values; the shared app reads the pin assigned to `boosterpack.26`, 1100 to 1212 counts (`portability/three-ways/tidrivers/cc35x1/app.syscfg`) | Works: ADC values among the 7 (X27) | - | Not for this chip | Not for this chip |
| I2C | `i2ccontroller` waits for a device (no sensor) | I2C wait (X27) | - | Not for this chip | Not for this chip |
| SPI | `spicontroller`, `spiperipheral` print wire lines; no peripheral (X26) | `spicontroller` and `spiperipheral` only start up (X27) | - | Not for this chip | Not for this chip |
| PWM and timers | `pwmled1` fade not observed | fade not observed (X27) | - | Not for this chip | Not for this chip |
| Crypto | Works: `sha2hash` prints the exact SHA-256 of `abc` | Works: SHA-256 of `abc` (X27) | - | Not for this chip | Not for this chip |
| Flash and storage | Not for this chip as a twin: the Wi-Fi SDK ships no NVS, AES or display driver example | - | - | Not for this chip | Not for this chip |
| Wi-Fi | Works: scan of 18 access points; WPA2 join on 5 GHz in 5.74 s (ten runs); TCP 12.1 and UDP 16.0 Mbit/s to a Mac | bare metal | - | Not for this chip | Not for this chip |
| Bluetooth LE | Works: advertises at 100 ms; a Mac connects in 0.52 s; Wi-Fi login given over Bluetooth, join in 7.13 s | bare metal | - | Not for this chip | Not for this chip |
| RTOS | Works: TI's FreeRTOS blink and CoreMark in a FreeRTOS thread at 160 MHz | bare metal | see GPIO | Not for this chip | Not for this chip |

Sources: matrix X26, X27, X53; `cc35x1/AGENTS.md`; `portability/everywhere/README.md`; `data/support-matrix.json` (Zephyr, FreeRTOS, MicroPython, Arduino cells); `data/results/cc35x1.csv`.

## How to port into this family

1. **Pick the layer.** TI Drivers, with the same names in the F3 and Wi-Fi SDKs (matrix X26). MSPM0 DriverLib code has no landing here (`portability/everywhere/README.md`).
2. **Start from the Wi-Fi SDK's example of the same name** and overwrite its `.c` files; 11 same-name examples exist: `adcsinglechannel`, `empty`, `exception`, `gpiointerrupt`, `i2ccontroller`, `pwmled1`, `sha2hash`, `spicontroller`, `spiperipheral`, `uart2echo`, `watchdog` (`portability/everywhere/README.md`). The F3 SDK's AES, NVS, SD, display, log and I2C target examples have no Wi-Fi twin.
3. **Use SysConfig 1.28.0 for the Wi-Fi SDK**, and pass four paths to `make` because `imports.mak` names folders a Mac lacks (`cc35x1/AGENTS.md`).
4. **Sign every image** with the Toolbox image builder; use absolute paths in `tool_settings.json` (`cc35x1/AGENTS.md`).
5. **Time with the FreeRTOS tick**, not `DWT->CYCCNT` (`cc35x1/AGENTS.md`).
6. **Keep disk free while building in a batch.** The Toolbox and linker die without a message when the disk is down to a few hundred MB.
7. **Read the console at the image's rate.** TI's examples print at 115200; the Wi-Fi CSI program at 921600 (`cc35x1/AGENTS.md`).
8. **Reuse TI's own provisioning, MQTT and storage pieces** instead of another vendor's: TI's `ble_wifi_provisioning` service already does what the NXP `dm-wifi-provisioning-using-ble` demo does (`ports/wifi-provisioning-ble/README.md`).

## Moving inside the family

Only one CC35xx part is held, so no move between CC35xx parts has run. The moves onto and off this part are in the next section; every row is in [EVIDENCE.md](EVIDENCE.md).

## Moving to and from the other families

| To or from | Route | Measured | Rows and files |
|---|---|---|---|
| CC2340R5 to CC35X1, TI Drivers | F3 app source in the Wi-Fi SDK project | 11 tried: 11 unchanged; all programmed, 6 print the text the source defines, 5 are silent by design | X26 |
| CC35X1 to CC2340R5, TI Drivers | Wi-Fi SDK app source in the F3 project | 11 tried: 11 unchanged; 7 show the source behavior, 3 only start up | X27, `docs/experiments/2026-10-24-cc2340r5-cc35x1-driver-twins.md` |
| One app on 11 boards | Shared `app.c` | `app.c` byte-identical; CC35X1, CC2755P10 and CC2340R5 share one TI Drivers glue file (48 lines, config 14); serial check passed | X53 |
| ESPHome on the CC35X1 | `cc35xx` platform | repeat compile 23 s; 20 of 20 switch commands confirmed, median 120 ms | `docs/experiments/2026-10-13-esphome-cc35xx-platform.md` |
| Radio family guide | | | [Radio family guide](../radio/README.md) |
| MSPM0 guide | | | [MSPM0 family guide](../mspm0/README.md) |

## What was measured

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| Same-name driver examples moved CC2340R5 to CC35X1 and back | 11 of 11 each way | F3 SDK 9.21.00.36, Wi-Fi SDK 10.20.00.39, ran 2026-10-24 | measured | `portability/matrix.csv` X26, X27 |
| Shared app, TI Drivers with FreeRTOS | 20.7 kB of flash (linker map, FLASH region) | 57-line glue, 14 config lines, 13.1 s clean build | measured | X53 |
| Wi-Fi join, WPA2 on 5 GHz | 5.74 s (ten runs) | LP-EM-CC35X1, TI Network Terminal | measured | `cc35x1/AGENTS.md` |
| MQTT through a throwaway broker | 720 of 720 requests answered over an hour, median 167 ms | | measured | `cc35x1/AGENTS.md` |
| DNS ad blocker | 6 ms median for a blocked name, 70 ms forwarded | 71,492 blocked names in a 357 kB table | measured | `docs/experiments/2026-10-20-cc35x1-dns-sinkhole.md` |
| ESPHome Wi-Fi device soak | 5,616 of 5,620 commands, 22 access-point drops | 24 hours | measured | `docs/experiments/2026-10-13-esphome-cc35xx-platform.md` |
| CoreMark, bare metal, -O3 | 718.91 | 160 MHz | measured | `cc35x1/RESULTS-cc35x1.md` |
| SDK examples | 31 of 32 build; 29 of 31 booted | Wi-Fi SDK 10.20.00.39 | measured | `cc35x1/SWEEP.md` |
| Update over HTTPS, 1.29 MB image | 12 to 28 s per update | four rounds | measured | `cc35x1/AGENTS.md` |

## If you see this

- If the Toolbox prints `Process is stopped. Life cycle OPERATIONAL is not valid. Get fuse data failed`, the chip is not activated: ask before running the one-way activation step (`cc35x1/AGENTS.md`).
- If it prints `Path to programming_instructions after path correction couldn't be found`, rebuild with an absolute build folder or make the paths in `tool_settings.json` absolute (`cc35x1/AGENTS.md`).
- If a freshly programmed image is silent after the Wi-Fi CSI program, run `full_flash_erase` (about 16 s) and program again: the CSI program lays the flash out differently (`cc35x1/AGENTS.md`).

The rest, with causes: [FAQ.md](FAQ.md).

## Where these facts come from

SimpleLink Wi-Fi SDK 10.20.00.39, Wi-Fi Toolbox 4.3.23, SysConfig 1.28.0, TI Arm Clang 5.1.1.LTS, F3 SDK 9.21.00.36 for the CC2340R5 and CC2755P10 sides, on an Intel Mac with macOS 15.7.x; runs dated 2026-10-08 to 2026-10-25 in the files named. Data: `portability/matrix.csv`, `data/results/cc35x1.csv`, `site/data/ports.json`, `data/support-matrix.json`. The generated record: `portability/families/wifi/EVIDENCE.md`. Checked 2026-10-25.
