# One app, three software layers, on the MSPM0G3507 and MSPM0L1306

Bringing the app to a new board: [PORTING.md](PORTING.md).

The same 81-line app file builds unchanged three ways, and bare DriverLib needs the least around it: 39 lines of glue, 35 lines of SysConfig, 2.3 kB of flash and 528 bytes of RAM on the LP-MSPM0G3507 LaunchPad development kit. TI Drivers needs 49 to 59 lines of glue and 136 lines of hand-written C configuration, for 13.5 to 20.5 kB of flash. Zephyr 4.4.2 needs 68 lines of glue (30 of them DriverLib, because Zephyr has no MSPM0 ADC driver) and 6 configuration lines, for 12.4 kB of flash and a 65-second build. Seven images built, and one more (TI Drivers on FreeRTOS for the L1306) did not link. On the L1306, DriverLib and Zephyr ran and printed averages 6 counts apart (2,684 and 2,678), and TI Drivers ran but its serial output is unreadable. The four G3507 images are built, not run.

Time to repeat: about 3 minutes to rebuild all images with no board; flash and record takes 12 s of serial capture per image plus the flash step (flash time not recorded; run on the L1306 only), once the tools in [mac-setup/README.md](../../mac-setup/README.md) are installed.

Terms: [docs/vocab.md](../../docs/vocab.md), the [LP-MSPM0G3507 FAQ](../../mspm0g3507/FAQ.md) and the [LP-MSPM0L1306 FAQ](../../mspm0l1306/FAQ.md). **Glue** is the code that connects the app to one software layer. **Configuration** is what says which pins and settings the board uses (SysConfig text, devicetree, Kconfig, or C tables). **DPL** is TI's driver porting layer, the small set of calls (clock, interrupt, semaphore) TI Drivers run on, with one version for FreeRTOS and one for no RTOS. **TI Drivers** is TI's higher-level driver library (`UART_open`, `ADC_open`, `GPIO_setConfig`) that sits on top of DriverLib and can run with FreeRTOS or with no RTOS. **FreeRTOS** is a small open-source real-time operating system. **NONMAIN** is the flash area that holds the chip's boot settings; a bad write there can lock the chip. 1 kB = 1,024 bytes.

Checked 2026-10-07 on an Intel Mac, macOS 15.7.9: MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS (from CCStudio 21.0.1), SysConfig 1.26.3, Zephyr 4.4.2 with GCC 12.2.0 from Zephyr SDK 0.17.4 (the Zephyr SDK is a separate download: <https://github.com/zephyrproject-rtos/sdk-ng/releases>), xPack OpenOCD 0.12.0. Line counts are code lines: no blank lines and no comment-only lines.

Contents: the app, the numbers, what each way's glue needed, moving to the L1306, problems, how to run it, what could not be confirmed, files.

## The app

Once a second it reads one ADC channel against the chip's internal 2.5 V reference, keeps a 4-sample moving average, prints one line on the LaunchPad kit's backchannel UART at 115200, and toggles the user LED:

```text
t=0 adc=2650 avg=2650
```

| Board | ADC input | UART | LED |
|---|---|---|---|
| LP-MSPM0G3507 | ADC0 channel 2 on PA25, header pin J1_2 (nothing connected on the board) | UART0, PA10 and PA11 | PB22, blue |
| LP-MSPM0L1306 | ADC0 channel 9 on PA15, the onboard thermistor | UART0, PA8 and PA9 | PA0, LED1 red (jumpers J2 on and J10 on 2:3) |

The ADC runs from the 32 MHz internal oscillator divided by 8, with a 125 µs sample window, in all three ways, so the readings should match. `common/app.c` holds the filter, the line formatting (its own number printer, so no `printf`) and the once-a-second loop. It includes nothing but `app.h` and `stdint.h`. `common/test_app.c` checks it on the Mac with fake glue: `cc -std=c99 -I. test_app.c app.c` passes 3 of 3 checks. Each way implements five calls: `board_init`, `adc_read`, `uart_write`, `led_toggle`, `sleep_ms`.

## The numbers

The G3507 has 128 kB of flash and 32 kB of RAM; the L1306 has 64 kB of flash and 4 kB of RAM.

| Board | Way | Shared lines | Glue lines | Config lines | Flash used, bytes | RAM used, bytes | Clean build, s |
|---|---|--:|--:|--:|--:|--:|--:|
| LP-MSPM0G3507 | DriverLib, no RTOS | 81 | 39 | 35 | 2,352 | 528 | 3.4 |
| LP-MSPM0G3507 | TI Drivers on FreeRTOS | 81 | 59 | 136 | 21,040 | 7,059 | 2.7 + 10.6 |
| LP-MSPM0G3507 | TI Drivers, no RTOS | 81 | 49 | 136 | 13,848 | 4,207 | 2.5 |
| LP-MSPM0G3507 | Zephyr 4.4.2 | 81 | 68 | 6 | 12,704 | 3,992 | 65.4 |
| LP-MSPM0L1306 | DriverLib, no RTOS | 81 | 39 | 34 | 2,320 | 272 | 3.2 |
| LP-MSPM0L1306 | TI Drivers, no RTOS | 81 | 49 | 130 | 10,744 | 3,413 | 2.9 |
| LP-MSPM0L1306 | TI Drivers on FreeRTOS | 81 | 59 | 130 | does not link | does not link | kernel 12.3, link fails |
| LP-MSPM0L1306 | Zephyr 4.4.2, this repo's board | 81 | 68 | 7 | 11,888 | 3,888 | 69.7 |

How each column was measured:

- **Lines:** `count.py`. Shared is `common/app.c` plus `common/app.h`: 81 code lines. Config is `driverlib/<board>.syscfg`; the four files in `tidrivers/<board>/`; `zephyr/prj.conf` and the board's overlay (plus one Kconfig line on the L1306). Build files are in no column: Zephyr's `CMakeLists.txt` is 8 code lines, and the TI ways use this folder's `build.sh` scripts in place of TI's example makefiles.
- **Flash and RAM:** the "used" column of the linker map's memory table for the TI builds, Zephyr's own memory report for Zephyr, one basis for every row: bytes used, not bytes left. The TI Drivers rows are the rebuild with 32-byte UART ring buffers (`maps/`). RAM includes stacks and heaps: 512 bytes of stack in the G3507 TI builds, 256 on the L1306; the 3,072-byte FreeRTOS heap; a 2,048-byte C library heap in both no-RTOS TI Drivers builds.
- **Build:** wall clock of a clean build from the build script, after `rm -rf` of the output. DriverLib's time includes SysConfig. TI Drivers on FreeRTOS shows the app, then the FreeRTOS kernel library, which the SDK cannot build from a terminal as shipped and `../crossbuild/build_mspm0_freertos.sh` builds once (10.6 s for the G3507 and 12.3 s for the L1306). The script reports `compiled 41 files, 2 failed`. The two are `kernel/freertos/Source/portable/Common/mpu_wrappers.c` and `mpu_wrappers_v2.c`: TI's kernel project file (`kernel/freertos/builds/<board>/release/ticlang/*.projectspec`) lists them but marks them `excludeFromBuild="true"`, and SDK 2.11.00.07 does not ship them (the script compiles every listed file, so it reports `no such file or directory` for both). This app does not need them: they are FreeRTOS's memory-protection wrappers, and TI's `FreeRTOSConfig.h` for both boards sets `configENABLE_MPU 0`. The G3507 FreeRTOS image linked without them, and the L1306 link stops on RAM (problem 7), not on a missing function. Zephyr's build runs 1,105 steps on 12 cores and compiles the kernel and C library from source; the TI ways link TI's prebuilt libraries on one core. The table shows the last build. Earlier builds, not saved: DriverLib 3.1 to 4.3 s on the G3507 and 2.7 to 4.3 s on the L1306, Zephyr 64.6 s on the G3507 and 79.3 s on the L1306. Other builds were running on the same Mac at the same time.
- **Clock:** the TI builds run the CPU at 32 MHz. Zephyr's upstream G3507 board runs it at 80 MHz; the L1306 board at 32 MHz. The ADC clock is the same everywhere.
- **NONMAIN:** no image writes it. The TI maps show 0 bytes in `BCR_CONFIG` and `BSL_CONFIG` (0x41C00000), and no record in either Zephyr HEX file is at or above 0x41C00000. `count.py` checks both every run.

Where TI Drivers' flash goes on the G3507 without an RTOS (from `maps/tidrivers-g3507-nortos.map`): the drivers 8.1 kB (UART 2,336, GPIO 1,410, DMA 1,304 of code (1,360 with constants), ADC 1,162, ring buffer 918, UART front end 914, other driver code 290; the six named parts sum to 8,044), the no-RTOS DPL 1,670, the C library's `malloc` 690. FreeRTOS adds 7 kB of flash and 2.8 kB of RAM on top. The GPIO driver's three per-pin tables cost 720 bytes of RAM on the G3507's 60 pins and 336 on the L1306's 28.

## What each way's glue needed that the others did not

**DriverLib** (`driverlib/glue_driverlib.c`, 39 lines):

- Names SysConfig makes up from the `.syscfg` file: `ADC12_0_INST`, `UART_0_INST`, `GPIO_LEDS_USER_LED_PIN`, `CPUCLK_FREQ`.
- A wait for the reference to settle, as in TI's `adc12_single_conversion_vref_internal` example.
- A busy-wait delay (`delay_cycles`) for `sleep_ms`. There is no tick to sleep on.

**TI Drivers** (`tidrivers/glue_tidrivers.c` 41 lines, plus `main_freertos.c` 18 or `main_nortos.c` 8):

- Open calls with parameter structs and handles (`ADC_open`, `UART_open` with the baud rate).
- The chip's pin-mux index inside the portable GPIO call: `GPIO_setConfig(CONFIG_GPIO_LED_0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW | CONFIG_GPIO_LED_0_IOMUX)`. The same gap is in the portability results (`../README.md`).
- A `main` that starts the kernel: one FreeRTOS task, or nothing extra with no RTOS. The same glue file builds under both.
- DriverLib calls in the configuration to power the ADC, UART and GPIO blocks and to turn on the reference, because the TI Drivers ADC driver does neither (`ti_msp_dl_config.c`, 30 lines).
- The board configuration itself, as C tables (`ti_drivers_config.c`, 86 lines). MSPM0 TI Drivers have no SysConfig module (SDK 2.11.00.07, checked by listing the examples: none of the 40 TI Drivers example folders, `examples/rtos/<board>/drivers/`, 10 examples on each of four LaunchPad kits, has a `.syscfg` file), so TI's examples ship these tables as hand-written C, and these were cut down from TI's `adc_singlechannel` and `uart_echo` examples.

**Zephyr** (`zephyr/src/glue_zephyr.c`, 68 lines):

- Zephyr's GPIO and UART calls on devicetree names (`led0`, the console UART), so the LED and UART pins need no lines of this repo's on either board.
- The ADC and the reference in DriverLib from Zephyr's TI module: 30 of the 68 lines. Zephyr 4.4.2 has no MSPM0 ADC driver (`drivers/adc` has no MSPM0 file); Zephyr main (4.5.0-dev, 2026-10-20) has `adc_mspm0_adc12.c` and the `ti,mspm0-adc12` binding, which this build did not use.
- Two DriverLib source files added in `CMakeLists.txt` (`dl_adc12.c`, `dl_vref.c`), because Zephyr's MSPM0 Kconfig has switches only for the GPIO, UART, timer and CAN parts of DriverLib.
- The ADC channel from a devicetree property (`zephyr,user` node, `adc-channel`), so the glue file is the same on both boards.

## Moving from the G3507 to the L1306

In all three ways the app and the glue file built for the L1306 unchanged. Only configuration changed:

| Way | What changed for the L1306 |
|---|---|
| DriverLib | 11 SysConfig lines changed or added (device and package, pins, ADC channel, one reference line copied from TI's L1306 example) and 2 dropped (the G3507's reference output pin and a clock line) |
| TI Drivers | 14 lines of the C configuration changed and 6 dropped (port B, and four clock calls SysConfig does not generate for this chip); startup and linker files from the SDK's device folder, because no TI Drivers example exists for this board |
| Zephyr | 1 devicetree line (the channel) and 1 Kconfig line (no C library heap), on this repo's out-of-tree board (`../../mspm0l1306/zephyr`) |

The same three moves are rows of this repo's portability matrix, [`../matrix.csv`](../matrix.csv), which the site's portability page is built from.

## Problems found

1. **Zephyr 4.4.2 has no ADC driver for MSPM0.** Fact: Zephyr 4.4.2's `drivers/adc` folder has no MSPM0 file. In Zephyr main (4.5.0-dev, 2026-10-20) the driver exists (`ti,mspm0-adc12`) and was not run here. What follows for 4.4.2: the glue calls DriverLib for the ADC, so this part of the Zephyr glue would have to be rewritten for another vendor's chip.
2. **Zephyr's TI module cannot switch on the DriverLib ADC or reference files.** Fact: `zephyr/modules/Kconfig.mspm0` defines only `USE_MSPM0_DL_GPIO`, `_UART`, `_TIMER` and `_MCAN`, while `modules/hal/ti/mspm0/CMakeLists.txt` asks for `CONFIG_USE_MSPM0_DL_ADC` and `CONFIG_USE_MSPM0_DL_VREF`. The fix: the app adds the two files itself in `CMakeLists.txt`. Zephyr's DriverLib copy is not the SDK 2.11 one: the two headers used differ from the SDK copies in 143 lines (93 in `dl_adc12.h`, 50 in `dl_vref.h`, counted as the lines `diff` marks on either side; the two `.c` files are identical).  Inference, not checked: the differences are version drift, and none of them changes this app.
3. **MSPM0 TI Drivers configuration is hand-written C.** No MSPM0 TI Drivers example has a `.syscfg` file, so the configuration is 136 lines of C against 35 lines of SysConfig for DriverLib.
4. **The TI Drivers ADC driver does not turn on the internal reference.** Read from the source: `ADCMSPM0.c` sets the reference choice in the ADC but never touches the VREF block. The configuration turns it on with DriverLib. Not checked on a board: no TI Drivers image has printed a reading (problem 9).
5. **A GPIO table entry without a pin-mux index sets up PA0's pin.** Read from the source, not run: `GPIO_init` takes the pin-mux index from each entry's low byte (`pincm = pinConfig & 0xFF`), and 0 is PA0's pin control register. TI's G3507 `adc_singlechannel` table has four entries without one (two ADC pins and the two UART pins). Here every entry is `GPIO_DO_NOT_CONFIG` and the glue and `ADC_open` set the pins.
6. **No TI Drivers example or board configuration for the L1306.** The SDK ships the driver library (`drivers_mspm0l11xx_l13xx.a`), the no-RTOS DPL and a FreeRTOS settings file for this board, but nothing that uses them.
7. **TI Drivers on FreeRTOS does not fit the L1306 with TI's own settings file.** The link stops with `run placement with alignment fails for section ".bss" size 0x1458` against 0x1000 of RAM: TI's `FreeRTOSConfig.h` for this board asks for a 3,072-byte heap. FreeRTOS itself fits with static allocation (`../../mspm0l1306/freertos/README.md`); that was not run with TI Drivers here.
8. **The G3507 TI Drivers images carry the DMA driver, which this app never uses.** 1.3 kB of flash and 77 of RAM. The G3507 driver library is built with UART DMA support; the L13xx one is built with `-DUART_NO_DMA` (its `.mak` file) and carries no DMA driver. Rebuilding the G3507 library without DMA was not run.
9. **TI Drivers on the L1306 sends binary, not text, on the serial port.** The image runs and keeps sending, but nothing readable arrives at 115200 or at six other rates. The suspicion is the hand-ported configuration, because TI ships no TI Drivers example for this chip (problem 6); no cause has been found. Details under [Runs on the board](#runs-on-the-board-2026-10-07).

### Mistakes to avoid

- On the L1306, check that the map names `startup_mspm0l130x_ticlang.c`; a build script that picks the G3507 startup file still links.
- The 2,048-byte heap in the no-RTOS TI Drivers builds is used: `GPIO_init` takes one semaphore from it (`SemaphoreP_createBinary` calls `malloc` in the no-RTOS DPL).
- This SDK's UART driver has no `UART_init()`; its buffers are built inside `UART_open`.
- `serial_check.py` saves what it records as text, so binary bytes are replaced by the replacement character. Save raw bytes when the output is not text.
- Give every TI Drivers GPIO table entry a pin-mux index (problem 5) before you flash.

## How to run it

The L1306 commands below (DriverLib, TI Drivers with no RTOS, and Zephyr) were run on 2026-10-07; the G3507 commands were not. Flashing overwrites the board's main flash, so back up whatever is on it first. No image touches NONMAIN. Run from the repo root. The checker records the serial port and checks the format, that `t` counts up by one, that `avg` is the 4-sample average of the `adc` values, and that lines come about once a second. It prints `PASS` or what failed.

`ocd.sh` is a short wrapper in this repo around xPack OpenOCD 0.12.0 ([OpenOCD](../../docs/vocab.md) is open-source flashing and debug software), installed as in [mac-setup/README.md](../../mac-setup/README.md); it picks the probe by serial number and passes each argument to OpenOCD as a command. `mspm0_board_reset` is a command in OpenOCD's own MSPM0 script (`target/ti_mspm0.cfg`) that resets the whole board through the probe's reset line. The probe serials below (<probe serial>, <probe serial>) are from this bench: replace them, in the commands and inside each `ocd.sh`, with yours (`ls /dev/cu.usbmodem*` shows them; the board's UART is the port ending in 1).

Rebuild everything first if the build folders are gone (about 3 minutes, no board needed):

```bash
portability/three-ways/build_all.sh
```

**LP-MSPM0G3507** (probe <probe serial>, serial port `/dev/cu.usbmodemMG3500011`). DriverLib image; for TI Drivers use `tidrivers/build-g3507-freertos/three_ways.out` or `tidrivers/build-g3507-nortos/three_ways.out`:

```bash
mspm0g3507/mcu-bench/sdk-common/ocd.sh "reset halt" "program portability/three-ways/driverlib/build-g3507/three_ways.out verify" "mspm0_board_reset"
```

```bash
python3 portability/three-ways/serial_check.py /dev/cu.usbmodemMG3500011 12 /tmp/run-driverlib-g3507.txt
```

Zephyr, then the board-level reset (after a debugger soft reset the G3507 has sat in Zephyr's NMI handler before, `../../mspm0g3507/zephyr/README.md`):

```bash
cd ~/zephyrproject/zephyr && source ../.venv/bin/activate && west flash -d <repo>/portability/three-ways/zephyr/build-g3507 --openocd ~/zephyr-toolchain/xpack-openocd-0.12.0-7/bin/openocd --openocd-search ~/zephyr-toolchain/xpack-openocd-0.12.0-7/openocd/scripts --cmd-pre-init "adapter serial MG350001"
```

```bash
mspm0g3507/mcu-bench/sdk-common/ocd.sh "mspm0_board_reset"
```

**LP-MSPM0L1306** (probe <probe serial>, serial port `/dev/cu.usbmodemML1300011`). Images: `driverlib/build-l1306/three_ways.out`, `tidrivers/build-l1306-nortos/three_ways.out`, `zephyr/build-l1306/zephyr/zephyr.hex`:

```bash
mspm0l1306/tools/ocd.sh "reset halt" "program portability/three-ways/driverlib/build-l1306/three_ways.out verify" "reset run"
```

```bash
python3 portability/three-ways/serial_check.py /dev/cu.usbmodemML1300011 12 /tmp/run-driverlib-l1306.txt
```

TI Drivers with no RTOS, then the same checker with `/tmp/run-tidrivers-l1306.txt` as the last argument:

```bash
mspm0l1306/tools/ocd.sh "reset halt" "program portability/three-ways/tidrivers/build-l1306-nortos/three_ways.out verify" "reset run"
```

Zephyr, then the board-level reset, as in [`mspm0l1306/zephyr/README.md`](../../mspm0l1306/zephyr/README.md), then the checker with `/tmp/run-zephyr-l1306.txt`:

```bash
cd ~/zephyrproject/zephyr && source ../.venv/bin/activate && west flash -d <repo>/portability/three-ways/zephyr/build-l1306 --openocd ~/zephyr-toolchain/xpack-openocd-0.12.0-7/bin/openocd --openocd-search ~/zephyr-toolchain/xpack-openocd-0.12.0-7/openocd/scripts --cmd-pre-init "adapter serial ML130001"
```

```bash
mspm0l1306/tools/ocd.sh "mspm0_board_reset"
```

The Zephyr command runs from `~/zephyrproject/zephyr`; change back to the repo root before the reset and the checker.

What to expect:

- One line a second, `t=<n> adc=<raw> avg=<avg>`, and the LED changing state with each line: blue on the G3507, LED1 red on the L1306. Zephyr prints `*** Booting Zephyr OS build v4.4.2 ***` first. OpenOCD may print `Error: checksum mismatch - attempting binary compare` before `** Verified OK **`; on the L1306 that is normal ([FAQ](../../mspm0l1306/FAQ.md)).
- **G3507:** PA25 is a header pin with nothing on it, so the reading drifts. Touch J1_2 to GND for about 0, or to 3V3 for 4095 (the input is above the 2.5 V reference). The three ways should agree.
- **L1306:** expected about 2,600 to 2,700 counts at room temperature, from an earlier reading of the same thermistor: 2,013 counts against the 3.3 V supply, scaled to the 2.5 V reference. That assumes a 3.3 V supply and a similar room; it is an expectation, not a measurement.

## Runs on the board (2026-10-07)

| Board | Way | Result |
|---|---|---|
| L1306 | DriverLib | **Ran: 12 lines in 12 s, gaps 0.997 s, `t` counting up, thermistor 2,683 to 2,685 counts, average 2,684**, at room temperature |
| L1306 | TI Drivers, no RTOS | **Ran; serial output unreadable.** The output is runs of zero bytes and a repeating 32-bit word 0x0000136f at 115200, no text. A second run gave 46 bytes in 2.5 s at 115200, 8% of them printable, the same repeating words; nothing is readable at 57600, 230400, 28800, 460800, 9600 or 38400 (these two runs and the baud sweep were observed, not saved). So the port is at 115200 and sending binary. 32-byte UART ring buffers in place of the hand-ported 1-byte ones changed nothing (L1306 now 10.5 kB of flash, 3.3 kB of RAM). The clock is not the cause: the working DriverLib build confirms a 32 MHz bus clock, and the driver's divide-by-4 arithmetic is right. No cause found. |
| L1306 | Zephyr | **Ran: 12 lines in 12 s, gaps 0.997 to 0.998 s, thermistor 2,677 to 2,680 counts, average 2,678**, 6 counts below the DriverLib build's average of 2,684 |
| G3507 | all four | **Built, not run.** |

## What could not be confirmed

- **Whether the G3507 images run.** None has been flashed.
- **Why TI Drivers sends binary on the L1306** (problem 9).
- **Whether the three ways read the same value** for the same input. On the L1306, DriverLib and Zephyr averaged 2,684 and 2,678 counts from the thermistor, a few minutes apart, so the room may have changed; TI Drivers gave no reading. The G3507 test with J1_2 tied to GND and then 3V3 is not done.
- **The internal reference on the L1306 with TI Drivers and Zephyr.** SysConfig adds one extra reference line for the L1306 (`adcMem0vrefDependencySelect`); the other two ways set the same register value by hand. Not checked on the chip.
- **Zephyr's flash split** between kernel, C library and drivers. Not broken down here.
- **Build times on a quiet machine.** The load average was 5 to 73 during these builds.

## Files

| Path | What it is |
|---|---|
| `common/app.c`, `app.h`, `test_app.c` | The shared app and its host test |
| `driverlib/` | `glue_driverlib.c`, `g3507.syscfg`, `l1306.syscfg`, `build.sh <board>` |
| `tidrivers/` | `glue_tidrivers.c`, `main_freertos.c`, `main_nortos.c`, the four configuration files per board, `build.sh <board> [freertos or nortos]` |
| `zephyr/` | `src/glue_zephyr.c`, `prj.conf`, `boards/` (overlays and the L1306 Kconfig line), `CMakeLists.txt`, `build.sh <board>` |
| `build_all.sh` | Every build in turn, the host test, then `count.py` |
| `count.py`, `results.json` | The line counts, sizes, build times and NONMAIN check behind the table |
| `serial_check.py` | Records and checks a run |
| `maps/` | Linker maps of the builds in the table |

Build output (`build-*` folders, SysConfig's generated files) is git-ignored and rebuilt by the scripts.

## Four more boards, same app.c (2026-10-25)

`common/app.c` and `app.h` (81 code lines) built unchanged for four more boards with DriverLib, bare metal. Each board added a glue file and one SysConfig file. All four ran on silicon and passed `serial_check.py` (12 lines in 12 s, `t` counting up, average checked).

| Board | Layer | Glue lines | Config lines | Flash, B | RAM, B | Build, s | What the ADC read |
|---|---|--:|--:|--:|--:|--:|---|
| LP-MSPM0L1306 (rerun) | DriverLib | 39 | 34 | 2,320 | 272 | 18.4 (machine load 109) | thermistor, avg 2,644 |
| LP-MSPM0G5187 | DriverLib, `g5187.syscfg` | 39 | 33 | 2,456 | 272 | 4.5 | PA25 floating, about 1,450 |
| LP-MSPM33C321A | DriverLib, `glue_driverlib_hsadc.c`, `m33c321a.syscfg` | 37 | 41 | 2,816 | 272 | 9.9 | PA27 floating, 2,886 to 3,341 |
| LP-AM13E230 | AM13 DriverLib, `glue_am13.c`, `am13e230.syscfg` | 38 | 35 | 9,376 | 9,205 | 14.3 | PA17 floating, 157 to 209 |

- The G5187 reuses `glue_driverlib.c` byte for byte; only the SysConfig file is new (ADC property `adcMem0vrefp`, UART instance `UC0`, LED PA17).
- The MSPM33 has no ADC12, only the HSADC (1.4 V internal reference), so `adc_read` is rewritten: 37 glue lines, of which the other four calls are the same code. UART, GPIO and delay calls are the MSPM0 ones. The SDK's startup file is the one SysConfig lists as referenced.
- The AM13 SDK has a sequencer/SOC ADC, `Device_Init()` and its own generated header. After `DL_ADC_forceSequencer` the code waits for the pending flag (`DL_ADC_getSOCStatus`) and then for `DL_ADC_isBusy`. The RAM figure is mostly the SDK linker file's stack and heap (4 kB of heap unused).
- On the AM13 the ADC on PA16 (ADCIN15, the pin TI's ADC example uses) read 0 on every sample with nothing connected; PA17 on the same image reads 157 to 209. Cause not confirmed (floating or pulled low).
- Build with `driverlib/build.sh g5187`, `driverlib/build_m33.sh`, `driverlib/build_am13.sh`; `build_all.sh` runs them.
- Flash with DSLite and `-r 1 -u -t 60` on the MSPM33 and AM13 (it ends with `Failed: Target is not halted` or a timeout after the image is written and running); OpenOCD on the L1306. Each board was backed up first and restored and read back at the end.
- `tools/preflight_image.py` reads each chip's memory map from `data/recovery.json`; the G5187 (RAM at 0x20200000, not 0x20000000) and the MSPM33 are listed there.

## LP-MSPM0G3519, same app.c (2026-10-25)

`common/app.c` and `app.h` (81 code lines) built unchanged with DriverLib for the LP-MSPM0G3519, and `glue_driverlib.c` is the G5187's file byte for byte (39 lines). Only `g3519.syscfg` is new (34 lines: device MSPM0G351X, LQFP-100, UART0 on PA10/PA11, LED PB27, ADC0 channel 11). 2.3 kB of flash, 272 B of RAM, clean build 3.9 s (`driverlib/build.sh g3519`). It ran on silicon and `serial_check.py` passed (13 lines in 14 s, steady 1,085 to 1,086 counts from the internal temperature sensor, which needs no wire). The capture opened after the nRST pulse, so t=0 is cut.

- PA25 left floating, as on the G5187, read a swinging 900 to 2,600 here: something on the header drives it. Channel 11 is the wire-free input.
- DSLite's start-after-flash leaves the SRAM-error NMI on this board; an nRST pulse after the flash starts the image (`../arduino/zephyr/FAQ.md`).
- `data/recovery.json` carries the G3519 memory map (FLASH 0x0, 512 kB; SRAM 0x20200000, 128 kB), so `tools/preflight_image.py` accepts G3519 images.
- Backed up first and restored; main flash verified by DSLite, data flash and NONMAIN read back equal to the arrival backup.

## What is open

1. Find why TI Drivers sends binary on the L1306, then rerun it. One check to try: the same TI Drivers build on the G3507, where TI's own UART examples are known to work, to separate this port from the driver.
2. Run the four G3507 images with `serial_check.py`, and check that the three ways read the same value from the same input (J1_2 tied to GND, then to 3V3).
3. TI Drivers on the L1306 under FreeRTOS with static allocation, the way `../../mspm0l1306/freertos/` fits FreeRTOS in 4 kB.
4. The same app on a non-TI board under Zephyr, where the DriverLib ADC lines are the part that has to change.
