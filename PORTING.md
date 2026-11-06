# Moving code to a TI LaunchPad kit from another TI chip or another vendor's board

Carry the configuration with the code, keep the application above a thin board layer, and reuse the layers already in this repo before writing new ones. Plain C moves almost anywhere; driver calls move inside one TI driver layer; anything vendor-specific is rebuilt from what the demo does.

Read the board's `AGENTS.md` and `FAQ.md` before porting to it. Measured with MSPM0 SDK 2.11.00.07, SimpleLink Low Power F3 SDK 9.21.00.36, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, Zephyr 4.4.2. Counts below are builds that passed, not "works".

## Pick the route

| Starting from | Fastest route to a TI board | What carries over | What you rewrite | Example in this repo |
|---|---|---|---|---|
| Another MSPM0 chip | Keep the example's `.syscfg`, switch the device in SysConfig, unlock pins, build with the target SDK makefile. Unchanged build rate: L-series to G3507 101 of 170, G3507 to L1306 22 of 270 (96 after SysConfig drops) | App C, most settings | Pins, clock-tree lines, peripherals the smaller part lacks | `portability/three-ways/` |
| MSPM0 to a TI wireless chip (CC23xx, CC27xx) | Write to TI Drivers calls only, then force-include `portability/shim/mspm0_on_f3/fr_mspm0_on_f3.h`. 5 of 10 driver examples build with it, and the 5 that built ran on a CC2340R5 | App logic, GPIO/SPI/I2C/UART/ADC calls | Board config names, direct `DL_*` calls, anything radio | `portability/crossbuild/m0_to_f3.sh` |
| C2000 | Plain C only. Copy TI's C2000Ware example for the part, bring the maths in | Algorithms, fixed-point code | 8-bit types, byte-wrap assumptions, `.ccxml` connection file, interrupt values | `ports/bitnetmcu/c2000/`, `c2000-launchpad/AGENTS.md` |
| STM32 HAL or LL | The STM32 migration skill that ships inside CCStudio 21.0.1 (`ti-ccstudio-migrate-to-ti`, marked for evaluation use) to MSPM0: 5 of 5 built, 3 ran as written. Or TI's STM32G4 HAL layer on the LP-AM13E230 (GPIO and UART ran; no timer, ADC or clock tree) | State machines, messages, timing intent | Clock, pin choice, DMA, button polarity, timers | `docs/experiments/2026-10-09-am13e230-stm32g4-hal-layer.md` |
| ESP-IDF or ESPHome | ESPHome: add the `cc27xx` or `cc23xx` platform with `external_components`, then `esphome compile` and `esphome upload`.  | The YAML (switch, sensor) | Pins as `DIO` numbers; sensors ESPHome limits to its own platforms | `ports/esphome-ti/` |
| Nordic nRF Connect / Zephyr | Zephyr app: add an out-of-tree TI board with `-DBOARD_ROOT`. Nordic phone-app services: rebuilt from public UUIDs | App C, GATT UUIDs and properties | Devicetree overlay, Kconfig, drivers Zephyr lacks (no MSPM0 ADC driver, so DriverLib) | `mspm0l1306/zephyr/`, `portability/three-ways/` |
| Arduino sketch | Build with the Arduino layer: unmodified ArduinoCore-API plus this repo's core, libraries untouched. LP-MSPM0G3507 only | Sketch, Arduino libraries | Anything using `analogRead`, `analogWrite`, `tone`, `attachInterrupt`, `Serial1` (not implemented) | `portability/arduino/` |
| Vendor demo with a device-locked license | Rebuild from behavior and public specs; never copy the locked files. Describe it as a port of their public example | UUIDs, byte formats, observable behavior | All the code | `ports/ble-hid-remote/` |

## Rules for any port

1. Build the target's own twin example untouched and prove blink first; a pair only counts if the target's own example builds.
2. Keep the application in one vendor-free file and the board in a thin glue file (`board_init`, `adc_read`, `uart_write`, `led_toggle`, `sleep_ms`); the app includes no device header.
3. A shim holds renames and empty stubs only; if it needs logic, that is an edit, and you count the lines.
4. Carry the `.syscfg`, let the target's SysConfig reject lines one at a time, and read every dropped setting; delete pre-generated `ti_msp_dl_config.*` and shipped `.out` files in the target project.
5. Recheck every pin after changing the device in SysConfig: it keeps the old board's pins and says nothing.
6. Map interrupts by number against the target's header and implemented lines, and compare status codes by name, never by value.
7. Check RAM and flash in the linker map before flashing; small parts run out first (MSPM0L1306 4 kB RAM, MSPM0C1104 1 kB).
8. Keep upstream code unmodified in `third-party/<project>/` with a `SOURCE.md`; changes are a patch beside it or a copy under `ports/`. Check each file's license before copying.

## If you see this

| Symptom | Cause | Fix |
|---|---|---|
| `error: use of undeclared identifier 'MCAN0_INST'` | The app uses a generated name the target SDK spells differently | Take the app's `.syscfg` along so the target generates the names the app expects |
| `use of undeclared identifier 'TIMG0_INT_IRQn'` | Old pre-generated `ti_msp_dl_config.*` beside the app shadows the fresh output | Delete them before the build |
| `error: use of undeclared identifier 'UART0'` (also `SPI1`, `TIMA0`, `DMA`) | The app names the silicon instance, not SysConfig's alias (`UART_0_INST`) | Rename in an alias header force-included with `-include`, or in the app |
| `error: use of undeclared identifier 'RX'` | The AM13 SysConfig generator writes a bare list word into C | `#define RX DL_UART_INTERRUPT_RX` in the alias header |
| The ported blink builds and flashes, the LED stays dark | Device switch kept the old board's pins; a C1104 blink toggles PA22, the L1306's light-sensor amplifier | Unlock pin assignments or set the LED pin by hand (PA0 on the L1306) |
| `fatal error: 'main.h' file not found` | An imported STM32CubeIDE project leaves `Inc/` off the include path | Add `-I"${PROJECT_ROOT}/Inc"` |
| `invalid option -- O` | Apple's `make` is too old for the generated makefile | Use CCStudio's `gmake` (`ccs/utils/bin/gmake -B all`) |
| `error: 'IOMUX_PINCM38' undeclared here (not in a function)` | Zephyr's MSPM0 GPIO driver has the 80-pin L222x pin table | Use the corrected driver copy in the board folder |
| `***** HARD FAULT *****` at boot in `sys_heap_init` | Zephyr's libc heap defaults to all RAM left, 8 to 16 bytes here | `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=0` |
| `region`RAM' overflowed` | Zephyr sample stacks and buffers are sized for bigger parts | Cut stacks and buffers; most samples need 6 to 17 kB |
| Interrupt handler never runs, no compiler error | The target implements fewer lines (L1306: 13 of 32), or macro names kept but values changed (C28x PIE) | Pick a line the chip implements; read the value from the target's header |
| `"i8.c", line 2: error: identifier "int8_t" is undefined` | The C28x has no 8-bit type | `typedef int16_t int8_t;` and check nothing relies on 8-bit wrap |
| `cannot open source file "inc/stw_dataTypes.h"` | A ported F280015x example needs a block the F2800137 lacks (MCAN, LIN, some PWM and GPIO) | Port the part that fits; drop the example if the block is the point |
| 16-bit service UUID arrives byte-swapped | SysConfig writes it high byte first; Bluetooth wants low first | Type the bytes swapped (0xFCD2 as `0xD2FC`); check `ti_ble_config.c` |
| `OSError: [Errno 28] No space left on device` mid-batch | Cross-build batches fill the disk | Free space (CCStudio's cache under `~/.ti`) and rerun; porters skip finished work |

## Reuse these layers

- `portability/arduino/` (`core/`, `boards/<board>/`): `BOARD=<id> ./build.sh`. Add a board by adding a folder.
- `portability/shim/`: `mspm0_on_f3`, `f3_on_mspm0`, `mspm0_on_am13e230`.
- `portability/three-ways/`: one app, three glue sets (DriverLib, TI Drivers, Zephyr).
- `portability/everywhere/`: porters that carry `.syscfg`, generate the alias header and list dropped settings (MSPM0 to MSPM33, MSPM0 to AM13, F3 to Wi-Fi); `mcuplus-compat/` for AM263Px and radar SDKs.
- `portability/boards/`: `gen_board_h.py` writes clock and pin constants for shared tests.
- Split a radio demo into three files: a portable format header, a vendor glue file and a configuration diff.
- `portability/matrix.csv`: check whether your move already has a row before you try it.

Next: find your row, run the nearest example on the target untouched, then port one thing at a time.
