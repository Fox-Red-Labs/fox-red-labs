# Port drivers and code to the CC23xx, CC27xx and CC13xx radio family (LP-EM-CC2340R5, LP-EM-CC2755P10, LP-CC1354P10-1) and between it and MSPM0

TI Drivers code moves between the three radio boards with the app files untouched: all 33 driver examples that the LP-EM-CC2340R5 and LP-EM-CC2755P10 both ship ran on either board (matrix X11), and all 8 examples shared with the LP-CC1354P10-1 ran (X12). MSPM0 code does not: 0 of 10 MSPM0 TI Drivers examples build unchanged for the CC2340R5, 10 of 10 build with a 47-line header and no app edits (X1), and MSPM0 DriverLib code has no landing on any radio chip. Upstream Zephyr 4.4.2 has a board for the CC2340R5 only, with no Bluetooth; there is no CC27xx or CC13x4 board in it.

Terms: [docs/vocab.md](../../../docs/vocab.md) and this family's [FAQ](FAQ.md). **TI Drivers** is TI's higher-level driver layer (`UART2_open`, `GPIO_setConfig`) that the F3 and F2 SDKs share; **F3 SDK** is the SimpleLink Low Power F3 SDK (CC23xx, CC27xx) and **F2 SDK** the Low Power F2 SDK (CC13xx, CC26xx); **SysConfig** writes a board's pin and driver code from a `.syscfg` file; **CCFG** and **SCFG** are the chip's boot configuration areas; **DMM** is TI's scheduler that shares one radio between two stacks; **RCP** is a radio-only co-processor image; **shim** is a header of renames and empty stubs that holds no logic. 1 kB = 1,024 bytes. **X11** and the other X numbers are row ids in `portability/matrix.csv`. The generated record: [EVIDENCE.md](EVIDENCE.md). The assistant's copy: [AGENTS.md](AGENTS.md).

Contents: the parts and their neighbors, a ten-minute path, the one-command build, the driver and function map, how to port into the family, moving inside it, moving to and from other families, what was measured, errors, sources.

## The board (three parts held)

| Part | Kit | Core and clock | Flash | RAM | On the board | Source |
|---|---|---|---|---|---|---|
| CC2340R52 | LP-EM-CC2340R5 | Cortex-M0+ at 48 MHz, Bluetooth LE and 802.15.4 | 512 kB | 36 kB | red LED DIO14, green DIO15; no USB port and no debugger, so an LP-XDS110ET or LP-XDS110 probe clips on | `cc2340r5/AGENTS.md` |
| CC2755P10 | LP-EM-CC2755P10 | Cortex-M33 at 96 MHz with FPU, math unit (APU), security unit (HSM), 2.4 GHz radio up to +20 dBm | 1,024 kB (928.0 kB linker region) | 162 kB (163.0 kB SRAM region) | red LED DIO16, green LED DIO12, two buttons, 1 MB SPI flash, no USB port; probe clips on | `cc2755p10/AGENTS.md` |
| CC1354P10 | LP-CC1354P10-1 | Cortex-M33 at 48 MHz, Sub-1 GHz and 2.4 GHz radios (Bluetooth LE, Zigbee, Thread, TI 15.4) | 1 MB | 296 kB in TI's count, 256 kB of SRAM in the linker | XDS110 on the board, USB | `cc1354p10/AGENTS.md` |

Board folders: [`cc2340r5/`](../../../cc2340r5/AGENTS.md), [`cc2755p10/`](../../../cc2755p10/AGENTS.md), [`cc1354p10/`](../../../cc1354p10/AGENTS.md) (each with `README.md`, `AGENTS.md`, `FAQ.md`). The 64 kB CC2340R53 is a different kit, not held (`cc2340r5/FAQ.md`).

### Part-to-part map: the three radio parts side by side

A dash means nothing is on record.

| | CC2340R5 | CC2755P10 | CC1354P10 |
|---|---|---|---|
| SDK | F3 SDK 9.21.00.36 | F3 SDK 9.21.00.36 | F2 SDK 8.33.00.16 |
| Debugger | external probe, SWD only | external probe, TGT VDD jumper on XDS | on the board; connects only in the first seconds after power-on |
| Serial | probe port ending in 1, 115200 | app UART 115200; Thread console 921600 | probe port ending in 1, 115200 |
| Zephyr | upstream 4.4.2 board `lp_em_cc2340r5`: GPIO and UART, no Bluetooth | no upstream board; TI's fork v3.7.0-ti-1.00.01_ga printed `hello_world` and toggled `blinky` | none: 4.4.2 has no CC13x4 support |
| I2C target role | present | present | absent (F2 SDK 8.33 has no I2CTarget driver, X14) |
| Matter | no build on record | TI's lighting app rebuilt as a smoke and CO alarm, 797 kB image; pairing not shown | not built; not on TI's supported list |
| DMM Zigbee plus Bluetooth | compiles, fails to link on the R52 (2.1 kB short of RAM); links on the R53 | links and starts, 57.4 kB of RAM left | 459 kB flash, 33.8 kB RAM, joined Home Assistant in about 6 s |
| Chip erase | not recorded as a hazard | closes the debug port until main flash, CCFG and SCFG are valid again | not recorded |
| CoreMark, bare metal, -O3 | 111.09 (2.31 per MHz) | 426.31 (4.44 per MHz) | 199.92 (4.17 per MHz) |
| Interrupt latency, cycles | 28 to 29 | 21 to 35 | 23 to 24 |
| FreeRTOS task switch | 10.6 µs | 3.5 µs, 238 tasks | 7.4 µs |

Sources: `cc2340r5/AGENTS.md`, `cc2755p10/AGENTS.md`, `cc1354p10/AGENTS.md`, `data/support-matrix.json` (Zephyr and FreeRTOS cells), `portability/matrix.csv` X14, `cc2755p10/recovery/` through the AGENTS recovery section.

## Path: carry one TI Drivers example from the LP-EM-CC2340R5 to the LP-EM-CC2755P10

The commands are the ones the 2026-10-24 twin run and the board's AGENTS file record, put in order; the path as a whole was not timed.

1. Terminal, no board. Copy the LP-EM-CC2755P10 project of the same example and overwrite its `.c` files with the CC2340R5 ones; only the SysConfig and linker files differ (X11). Build with three paths passed to `make` (`cc2755p10/AGENTS.md` "Start here"):

   ```bash
   make -C empty/ticlang SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR=<SDK> SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh TICLANG_ARMCOMPILER=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
   ```

   You should see `empty.hex` in `empty/ticlang/` (14 s for the stock example).
2. Terminal, board and probe plugged in. Back up main flash and the CCFG before the first flash (the two `dslite.sh --mode memory` reads in `cc2755p10/AGENTS.md`). Once the board is flashed the shipped image cannot be read back.
3. Terminal. Check the image: `python3 tools/preflight_image.py cc2755p10 <image>`.
4. Terminal. Flash a hex that carries a valid CCFG and SCFG; never a chip erase, never `-v`, never the `.bin`:

   ```bash
   /Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c cc2755p10/CC2755P10.ccxml -e -f -u empty/ticlang/empty.hex
   ```

   You should see the red LED (DIO16) toggle once a second.
5. Terminal. Restore with `cc2755p10/restore.sh <run folder>`, which prints `RESTORED: main flash and CCFG read back identical to the Minute 0 backup`.

## One-command build

```bash
portability/three-ways/tidrivers/build_cc2340r5.sh
```

It copies TI's `drivers/empty` example for the LP-EM-CC2340R5, swaps in the shared `app.syscfg` and glue file, adds the 81-line shared `app.c`, and builds: 37.1 s clean on a Mac under load 67.55. `build_cc2755p10.sh` and `build_cc35x1.sh` sit beside it. It does not flash. The MSPM0 apps inside the F3 projects build with `portability/shim/build_twins.sh m0_to_f3 <work> <results.jsonl>` (`portability/shim/README.md`).

## Driver and function map

Columns: TI Drivers (the SDK's own driver examples), MSPM0 TI Drivers code carried over through the shim, and the other layers. A dash means nothing is on record. CC2340R5 unless the cell names another board.

| Function | TI Drivers (SDK examples) | MSPM0 code carried over | Zephyr | MicroPython | Arduino |
|---|---|---|---|---|---|
| GPIO | Works: TI blink toggles the red LED on the CC2340R5 and CC2755P10 (`cc2340r5/AGENTS.md`, `cc2755p10/AGENTS.md`); on the CC1354P10 the pin behavior of `empty` and `gpiointerrupt` not observed (X12) | 10 of 10 build with the shim; the first 5 ran on the CC2340R5 with the same pin states as TI's build (X1); 4 of 4 ran on the CC1354P10 (X13) | Works: CC2340R5 blinky, 82 s from first command (`cc2340r5/AGENTS.md`); CC2755P10 blinky toggled in TI's fork | `Pin` output reads back 1 and 0; the blink was not seen | Works: stock Blink, output register bit for DIO15 clears then sets |
| UART | Works: `uart2echo` and `uart2callback` twins echo on all three boards (X11, X12) | `uart_echo` and `uart_callback` need 12 to 15 shim lines (`portability/shim/README.md`) | Works: console on the CC2340R5 | Works: REPL at 115200 | Works: `Serial` at 9600, with a six-line patch to stop dropped bytes |
| I2C | `i2ctmp` and `i2ccontroller` start; no sensor answers (X11, X12); I2C target role absent on the CC1354P10 (X14) | `i2c_target` needs 2 shim lines and a 3-line overlay | a scan finds nothing on the CC23x0 driver, a one-byte register read at 0x44 works | `readfrom_mem(0x44, 0x7E, 2)` gave `b'TI'`, 1,596 lux; the HDC2080 did not answer; `scan()` returns `[]` | `Wire` read the OPT3001, 1,594 lux; the HDC2080 did not answer |
| SPI | `spicontroller` and `spiperipheral` run; no wires (X11, X12) | 2 of 4 CC1354P10 runs matched TI's serial progress (X13) | - | `write_readinto` completed; MISO not wired, data not checked | 8 bytes completed; MISO not wired, data not checked |
| ADC | Works: `adcsinglechannel` values; the shared app reads the battery channel, about 1374 counts at 3.3 V on the CC2755P10 (`portability/three-ways/README.md`) | `adc_singlechannel` builds with 1 shim line and 1 pin flag | Works: ADC channels in the Arduino and MicroPython overlays | Works: pulled up read 65,471, pulled down 16 | Works: floating A0 read 295 to 400 |
| PWM and timers | `pwmled1` fade not observable (X11) | - (no MSPM0 TI Drivers PWM, `portability/mspm0-to-wireless.md` gap 6) | the board file's `pwm-leds` points at `pwm5` channel 0; the red LED is LGPT1 channel 2 | `duty_ns` correct; `duty_u16` wrong in 1.29.0 without a one-cast patch | `analogWrite` timer registers read back; LED not seen |
| Interrupts | Works: 28 to 29 cycles, pend to handler, on the CC2340R5 (`cc2340r5/AGENTS.md`) | `gpio_interrupt` builds with 0 shim lines and 4 pin flags | Zephyr 4.4.2 clears the edge bits when a pin is configured again | Works: `Pin.irq` 10 of 10 software-driven edges | Works: `attachInterrupt` counted software-driven edges |
| Flash and storage | storage addresses must stay below 512 kB on the CC2340R5 (0x7C000 and 0x79000) | - | - | - | - |
| Bluetooth LE | Works: `basic_ble` on all three boards; CC2755P10 echo median 29.2 ms for 20 bytes | bare metal | no Bluetooth in upstream Zephyr 4.4.2; TI's fork ran with the C library heap on (the peripheral sample leaves 1.7 kB) | - | - |
| RTOS | Works: TI's FreeRTOS, the task-switch times in the map above | bare metal | See GPIO | - | - |

Sources: matrix X1, X11, X12, X13, X14; `cc2340r5/AGENTS.md`; `cc2755p10/AGENTS.md`; `portability/shim/README.md`; `portability/arduino/zephyr/README.md` and `BOARD-cc2340r5.md`; `ports/micropython/zephyr-cc2340r5/README.md`; `site/data/ports.json`; `data/support-matrix.json`.

## How to port into this family

1. **Pick the layer.** TI Drivers is the layer the radio SDKs share, with the same names in F3 and F2 (`portability/mspm0-to-wireless.md`). MSPM0 DriverLib has no landing on any radio chip (`portability/everywhere/README.md`). Write to TI Drivers calls only, never `DL_*`, and keep pin names and app constants in one header of your own (`portability/mspm0-to-wireless.md`).
2. **Start from the target board's example of the same name.** Overwrite its `.c` files with yours; SysConfig keeps the pins and the linker file stays the target's (X11).
3. **For an MSPM0 TI Drivers app, force-include the shim.** `portability/shim/mspm0_on_f3/` is 39 lines of renames (47 with its stub header and two overlays); each `CONFIG_*_IOMUX` pin-mux name is defined as 0 on the build line (`portability/shim/README.md`).
4. **Check RAM against the stack before the code.** On the CC2340R5 `basic_ble` leaves 5.1 kB, a Zigbee router 799 B with logging on, the full Thread device fails to link by 7.5 to 23.8 kB (`cc2340r5/AGENTS.md`).
5. **Check the compiler.** The 144 F3 examples that ship a GCC makefile build with GCC 12.2; the Bluetooth and Zigbee stacks ship libraries for TI Arm Clang (and IAR for Bluetooth) only (`portability/toolchains/gcc-f3/README.md`).
6. **Protect the debug port.** On the CC2755P10 never chip-erase, and flash only a hex whose CCFG and SCFG are valid; an `ot-ti` GCC image needs `cc2755p10/thread/make_flash_hex.py` (`cc2755p10/AGENTS.md`). On the CC1354P10 the debugger connects only in the first seconds after power-on, so an image that sleeps locks you out until the cable is replugged, and the CCFG at 0x50000000 is not written (`cc1354p10/AGENTS.md`).
7. **Split a radio demo into three layers.** The protocol or format as plain C with a host test, one short vendor file of glue, and the configuration as a text diff. The BTHome beacon was 60 portable code lines against 58 lines of TI glue (`portability/driver-layering.md`).
8. **Back up and restore.** Per-board commands are in each board's `AGENTS.md` "Start here"; the CC1354P10 has no tested backup command.

## Moving inside the family

| Move | Route | Measured | Rows and files |
|---|---|---|---|
| CC2340R5 to CC2755P10 | Same F3 SDK, app source copied | 33 of 33 unchanged; all ran with no fault, 22 matched the behavior TI describes, 10 wait for hardware that is not attached | X11 |
| CC2340R5 to CC1354P10 | F3 app source in the F2 SDK project | 8 of 8 unchanged; twin and TI reference images byte-identical | X12 |
| CC1354P10 to CC2755P10, plain C | Board harness only | 6 of 6 known-answer steps, plus TI's driver for the chip's HSM | X42, `cc2755p10/RESULTS-cc2755p10.md` |
| OpenOCD flash driver | `openocd -f board/ti_lp_em_cc2340r5.cfg` | CC2340R5 and CC2755P10 only; OpenOCD 0.12 has no CC13x4 target | `docs/experiments/2026-10-18-openocd-cc23xx-driver.md`, `cc1354p10/AGENTS.md` |

## Moving to and from the other families

| To or from | Route | Measured | Rows and files |
|---|---|---|---|
| MSPM0G3507 to CC2340R5, TI Drivers | MSPM0 app source in the F3 project with the shim | 10 tried: 0 unchanged, 10 with glue; first 5 ran | X1 |
| CC2340R5 to MSPM0G3507 and G3519 | F3 app source in an MSPM0 FreeRTOS project | 10 build; on the G3519 3 show behavior, 1 toolchain, 6 no-hardware | X2, `portability/shim/README.md` |
| MSPM0G3507 to CC1354P10, TI Drivers | MSPM0 app source in the F2 project | 8 tried: 0 unchanged, 4 with glue; 4 ran | X13 |
| CC2340R5 and CC35X1 | TI Drivers twins in each other's SDK | 11 of 11 each way | X26, X27, [Wi-Fi guide](../wifi/README.md) |
| CC1354P10 to MSPM33C321A, MSPM0G3519, MSPM0G5187 | ML-KEM-768 and ML-DSA-44 plain C | 6 of 6 on the first two; 3 of 6 unchanged on the G5187's 32 kB of RAM | X39, X40, X41 |
| MSPM0G3507 Zephyr app to CC2340R5 | mcu-bench suite | 1 tried, 1 after edits (clock, RAM, cycle-counter fixes) | X5 |
| Nine TI boards, CC1407P host driver | Bare-metal build, 10.2 to 10.3 kB of flash | 9 tried, 9 with glue; CC2755P10 sent the first SPI frame equal to the PC reference; no radio fitted | X45 |
| MSPM0 radio example on a part with no radio | n/a | Does not make sense | X10 |
| One app on 11 boards | Shared `app.c` | CC2340R5 and CC2755P10 share one TI Drivers glue file (48 lines, config 14) | X53 |
| MSPM0 guide, MSPM33 guide, AM13 guide | | | [MSPM0](../mspm0/README.md), [MSPM33](../mspm33/README.md), [AM13](../am13/README.md) |

## What was measured

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| TI Drivers examples moved CC2340R5 to CC2755P10 | 33 of 33 unchanged | F3 SDK 9.21.00.36, ran 2026-10-24 | measured | `portability/matrix.csv` X11 |
| TI Drivers examples moved CC2340R5 to CC1354P10 | 8 of 8 unchanged | F2 SDK 8.33 | measured | X12 |
| MSPM0G3507 TI Drivers examples built for the CC2340R5 | 0 of 10 unchanged, 10 of 10 with the 47-line shim | TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0 | measured | X1, `portability/shim/README.md` |
| F3 examples built with GCC 12.2 | 144 of 144 (64 and 80) | Zephyr SDK GCC | measured | `portability/toolchains/gcc-f3/README.md` |
| Shared app, TI Drivers with FreeRTOS | 20.7, 19.7 and 18.4 kB of flash (linker map, FLASH region) for CC35X1, CC2755P10, CC2340R5 | 48 glue lines, 14 config lines | measured | X53 |
| CoreMark, bare metal, -O3 | 111.09, 426.31, 199.92 | CC2340R5, CC2755P10, CC1354P10 | measured | board `AGENTS.md` files |
| MicroPython 1.29.0 on the CC2340R5 | 174.8 kB flash, 29.9 kB RAM, 20 kB heap | stock settings overflow RAM by 46.8 kB | measured | `ports/micropython/zephyr-cc2340r5/README.md` |
| Arduino sketches on the CC2340R5 | 27.7 to 37.4 kB flash, 5.2 to 6.1 kB RAM | ArduinoCore-zephyr `af5b8610f9be`, Zephyr 4.4.2 | measured | `portability/arduino/zephyr/BOARD-cc2340r5.md` |
| Nordic LED and UART services on the CC2340R5 | 24.3 ms LED write, 24.2 ms UART echo, 60 of 60 lines identical | Mac with `bleak`; from a Mac, not from phone apps | measured | `ports/nordic-apps/README.md` |
| ML-KEM-768 on the CC1354P10 | 20.8, 24.0, 29.1 ms (key pair, encapsulate, decapsulate) | NIST known-answer steps pass | measured | `cc1354p10/AGENTS.md` |

## If you see this

- If the build stops at a `CONFIG_*_IOMUX` name or at `DisplayUart.h` on a carried MSPM0 TI Drivers app, force-include the shim and define each pin-mux name as 0: the radio SDKs take the mux from the board table and call the display header `DisplayUart2.h` (`portability/mspm0-to-wireless.md`).
- If `Debug is not allowed based on` appears after flashing a CC2755P10, the image left the CCFG and SCFG checksums at 0 (an `ot-ti` GCC build): recover over SACI, then flash a hex from `cc2755p10/thread/make_flash_hex.py` (`cc2755p10/AGENTS.md`).
- If a CC1354P10 load fails with `Error -242`, the board slept past the first seconds after power-on: unplug USB for 3 s, or switch the probe's hub port off for 6 s and load at once (`cc1354p10/AGENTS.md`).

The rest, with causes: [FAQ.md](FAQ.md).

## Where these facts come from

SimpleLink Low Power F3 SDK 9.21.00.36 LTS, Low Power F2 SDK 8.33.00.16, TI Arm Clang 5.1.1.LTS (4.0.4 for the Zigbee and DMM projects), SysConfig 1.26.3 and 1.28.0, UniFlash 9.6.0, CCStudio 21.0.1, Zephyr 4.4.2, MicroPython v1.29.0, ArduinoCore-zephyr `af5b8610f9be`, on an Intel Mac with macOS 15.7.x; runs dated 2026-10-05 to 2026-10-25 in the files named. Data: `portability/matrix.csv`, `data/results/cc2340r5.csv`, `data/results/cc2755p10.csv`, `data/results/cc1354p10.csv`, `site/data/ports.json`, `data/support-matrix.json`. The generated record: `portability/families/radio/EVIDENCE.md`. Checked 2026-10-25.
