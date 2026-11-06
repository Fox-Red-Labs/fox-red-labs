# Moving a TI example from one board to another

To port a TI example, carry the example's SysConfig file with it, let the target's SysConfig reject what it cannot take, add a small header for the names the target does not generate, build with the target's own project, and run it with the board's backup and restore. Each step below is a tool in this folder. On 2026-10-10 and 11 these tools built ports between real TI boards: MSPM0, MSPM33C321A, AM13E230, AM263Px, the IWRL6432 radar, CC23xx and CC35X1. The ports to the MSPM33C321A, AM13E230, AM263P, SK-TDA4VM and IWRL6432 also ran on the chip. Results are matrix ids X26 to X36 in `../matrix.csv`.

Contents: 1 find out whether the move can work, 2 carry the SysConfig file, 3 generate the alias header, 4 build in the target's project, 5 run it with a way back, 6 differences that compile, 7 worked routes, then board and SDK notes.

Terms, beyond [vocab](../../docs/vocab.md):

- DriverLib is TI's register-level driver library for MSPM0 and MSPM33.
- A twin is the same example, by name, in two SDKs. Baseline is the target's own twin built untouched; swapped is the same project with the source's app files dropped in.
- SysConfig is TI's configuration tool: a `.syscfg` file in, generated C (`ti_msp_dl_config.h` and friends) out.
- MCU+ is the SDK base TI uses for the AM26x processors and the low-power radar chips; EDMA is its DMA engine. An `.appimage` is the radar's flashable boot image, and arprog is the radar SDK's serial flasher.
- R5F and A72 are the Arm Cortex-R5F core of the AM263P and the Cortex-A72 cores of the TDA4VM. CSI is Wi-Fi channel state information, which the CC35X1's presence example reads.
- CFSR and HFSR are the Arm fault status registers; zero means no fault. NONMAIN and BCR are the MSPM0 flash areas that hold boot and security settings; writing them wrong can lock a chip.

Cause tags: **Our mistake** (something done wrong in this repo), **Setup or handling** (the bench or the Mac), **Tool or docs** (a tool or document behaves wrong, including two SDKs that disagree on a name or a behavior), **Chip limit** (the target silicon does not have it), **Unknown** (not found; "suspected" names the lead).

SDKs measured: MSPM0 SDK 2.11.00.07, MSPM33 SDK 1.04.00.00, AM13E2x SDK 26.01.00.03, AM263Px MCU+ SDK 26.01.00.08, MMWAVE-L-SDK 05.05.04.02 (IWRL6432), Low Power F3 SDK 9.21.00.36, Wi-Fi SDK 10.20.00.39; TI Arm Clang 5.1.1, SysConfig 1.26.3 and 1.28.0.

## 1. Find out whether the move can work at all

Run from `portability/everywhere/`. The SDK paths are the `SDKS` table at the top of `inventory.py`.

```bash
python3 inventory.py      # what every example needs (data/inventory.jsonl)
python3 feasibility.py    # a verdict per example and board (data/map.jsonl, SUMMARY.md)
```

Look up your example in `data/map.jsonl`, for example `grep '"gpio_toggle_output"' data/map.jsonl | grep am13e230`. The verdict is one of four: Should work, Could work (a block is missing and the port may drop it), Does not exist (no driver layer reaches the target), Does not make sense (a radio, radar or USB example on a board without one). Treat Should work as an upper bound: G3519 to G5187 predicted 189 and 141 built. In the G3519 to G5187 run, 41 of the 117 failures were names (section 2); 11 had no makefile, Among the rest: 10 need a block the G5187 lacks (MCAN 6, DAC12 4), 15 stop on a SysConfig `Resource conflict` for a pin in the package SysConfig chose, The others were not sorted; the per-example reasons are in the `port_mspm0.py` results. Names are fixable with the alias header; for a `Resource conflict`, pick another free pin for that signal in the `.syscfg` by hand; a missing block means that part of the example cannot come along; an example with no makefile (`make: *** No targets specified and no makefile found.`) builds just from CCStudio's project file, so import it there or skip it. If the verdict is Does not exist, only the plain C part of the example can move (last row below).

What moved, as measured:

| From | To | Tool | Build rate | On silicon |
|---|---|---|---|---|
| MSPM0 board | another MSPM0 board | `port_mspm0.py` (SysConfig device switch) | 141 of 258 (G3519 to G5187) | built only, not run |
| MSPM0 | MSPM33C321A | `crossport_mspm0_mspm33.py` | 43 of 83 | 40 ran, 39 clean |
| MSPM0 | AM13E230 | `crossport_mspm0_am13.py` | 20 of 43 single-block examples | 14 ran, 14 clean |
| F3 SDK (CC23xx, CC27xx) | Wi-Fi SDK (CC35X1) | `crossbuild_f3_wifi.sh` | 11 of 11 | programmed, console not seen |
| Wi-Fi SDK | F3 SDK | `crossbuild_f3_wifi.sh` | 11 of 11 | built only, not run |
| AM263Px MCU+ SDK | IWRL6432 radar SDK | `crossbuild_am263p_mmwave.sh` with the `mcuplus-compat/` header | 19 of 23 (11 without the header) | 2 ran, both built and run before the header existed |
| IWRL6432 radar SDK | AM263Px MCU+ SDK | same, other header | 15 of 19 (11 without) | built only, not run |
| MSPM0 CMSIS-DSP FFT example | AM263P R5F, SK-TDA4VM A72, the Mac | `am263p-fft/`, `host-fft/` | 1 of 1 each | same peak bin and value on all three |
| The radar frame parser (plain C) | SK-TDA4VM A72, the Mac | `host-radar/` | 1 of 1 each | 231 of 231 frames identical |

Only plain C reaches the SK-TDA4VM from an MCU SDK, and no MSPM0, MSPM33, AM13 or wireless SDK code reaches the AM263Px MCU+ driver layer; the radar SDK does, because it is the same MCU+ base.

How to call each tool:

```bash
python3 crossport_mspm0_mspm33.py <work dir> <results.jsonl> [FROM board] [category ...]
python3 crossport_mspm0_am13.py   <work dir> <results.jsonl> [--run] [--only <regex>] [FROM board]
./crossbuild_f3_wifi.sh           <work dir> <results.jsonl> f3_to_wifi | wifi_to_f3
COMPAT=$PWD/mcuplus-compat/am263p_on_mmwave.h MODES=swapped ONLY='uart|gpio' \
  ./crossbuild_am263p_mmwave.sh   <work dir> <results.jsonl> am263p_to_mmwave
```

`MODES` defaults to `baseline swapped` (the target's own example untouched, then the source's app dropped in); `ONLY` limits the run to examples matching a regex; for the reverse direction use `mmwave_on_am263p.h` and `mmwave_to_am263p`.

## 2. Carry the SysConfig file with the code

Inside one driver layer the API moves; the names do not. An MSPM0 app calls `GPIO_LEDS_USER_LED_1_PIN` because its SysConfig file named the LED that. The MSPM33 twin of the same example names it `PASS_LED`. Dropping the app into the target's project fails on names, with errors like `error: use of undeclared identifier 'MCAN0_INST'` (54 of 62 MSPM0-to-MSPM33 failures were names). Two TI SDKs name the same instance differently. Taking the app's own `.syscfg` along makes the target generate the names the app expects: 20 builds became 43.

What the porters do to the `.syscfg`, in order:

1. Swap the device and package lines to the target's.
2. Unlock every pin and peripheral assignment, so the target's SysConfig picks its own.
3. Map module ids when the target SDK spells them differently (MSPM0 `/ti/driverlib/GPIO` is `/drivers/gpio` on the AM13E230). The table is in the porter.
4. Run the target's SysConfig. Each line it rejects is commented out with `// PORT-DROPPED:` and listed in the result with SysConfig's own message.

If you see a `PORT-DROPPED` line, read its message: a dropped clock line usually does no harm; a dropped peripheral line means that block is not on the target, and the matching app code will not build.

If the build still says `use of undeclared identifier 'UART0'` (or `SPI1`, `TIMA0`, `DMA`), the app names the silicon instead of the alias SysConfig made (`UART_0_INST`). Rename it in the alias header (next section) or in the app; 41 of the 117 G3519-to-G5187 failures were this, counted by the first undeclared identifier (UART 16, SPI 11, I2C 5, TRNG 3, timers 6). TI's example uses the silicon name, not SysConfig's alias.

## 3. Generate the alias header

What the target's generator does not emit, a header does. It is force-included (`-include <header>` on the compile line) so no app line changes. It holds renames and small inline helpers only, no logic.

- MSPM0 on AM13: `board_pins.h`, written per example by `crossport_mspm0_am13.py`: the group `_PORT` macro, `_INT_IRQN` and `_IRQHandler` for every GPIO group, UART and timer, and `#define RX DL_UART_INTERRUPT_RX`, because the AM13 generator writes the bare word `RX` into C (`error: use of undeclared identifier 'RX'`). The AM13 SysConfig generator writes it.
- AM263Px and radar: `mcuplus-compat/am263p_on_mmwave.h` and `mmwave_on_am263p.h`, generated from both SDKs' `edma.h` by `mcuplus-compat/gen_compat.py`: 47 EDMA renames (`EDMACCPaRAMEntry_init` and `EDMA_ccPaRAMEntry_init`), the cache include, LIN register names, the watchdog start. Its README lists what it maps and what it cannot.

## 4. Build in the target's own project

Copy the target SDK's twin example (or its `empty` project), drop in the source app's `.c` and `.h` files, and build with the target's makefile. Before a build counts:

- **Remove TI's pre-generated config files** (`ti_msp_dl_config.c`, `.h`) that sit beside the source app. The makefile's `-I..` finds them before SysConfig's fresh output, and every port then fails with `use of undeclared identifier 'TIMG0_INT_IRQn'` for names the target never generated. It happened once; the porters skip them now.
- **Delete every shipped output** (`.out`, `.appimage`, `.map`) in the target project. The AM263Px and radar SDKs ship them beside each makefile; a failed build then leaves the old `.out` in place and a script reads it as a success. It happened once.
- **Check the disk.** Stop under 2 GB free. On 2026-10-11 two batches stopped with `OSError: [Errno 28] No space left on device`. If you see it, free space (the CCStudio debug-server cache under `~/.ti` is the usual culprit) and rerun; `crossport_mspm0_mspm33.py` and `crossport_mspm0_am13.py` skip what their results file already holds.

## 5. Run it with a way back

- **Back up first, restore after.** `../../tools/board_sweep/run_all.py <BOARD> <builds.jsonl> <out.jsonl>` backs up, runs each image, reads CFSR, HFSR and the GPIO output registers, and restores. `port_runlist.py` turns a porter's results into its input.
- **Skip what can lock a board.** `run_all.py` refuses key, flash-protection, NONMAIN and BCR examples unless asked by name.
- **One debugger connection where the target needs it.** On the AM263P, a RAM read in a fresh DSLite (TI's command-line loader) connection returns all zeros even right after a load. Not isolated. `dss_run_read.py` loads, runs, halts and reads in one CCStudio scripting connection.
- **Read the console at the program's own baud.** The CC35X1's CSI program talks at 921600; read at 115200 it returns nothing or a run of `wwww`. It happened once.

## 6. Look for the differences that compile

These print nothing, so a build or a clean fault register does not catch them:

- **An enable that moved between calls.** The AM263Px starts its watchdog in `Watchdog_open`; the radar SDK only in `Watchdog_enable`. Ported as is, the watchdog would never start (read from both drivers' source; not run on the board). The `mcuplus-compat` header wraps `Watchdog_open`. The two SDKs disagree.
- **Same names, different values.** The I2C status codes in those two SDKs: `I2C_STS_ERR_BUS_BUSY` is -4 on the AM263Px and -3 on the radar. Compare against names, never numbers. The two SDKs disagree.
- **A generator that accepts a property it does not support** and writes junk into C (the AM13 `RX` above). The generator accepts it.
- **A low-power sequence written for one chip's registers.** Both hard faults on other silicon were MSPM0 standby code on the MSPM33C321A: `sysctl_lfxt_standby` and `timx_timer_mode_periodic_standby`, each a bus fault, CFSR 0x200, HFSR 0x40. Not traced. Run low-power examples last and expect to port the sequence by hand.

## 7. Worked routes, run on both boards

Each route below ran on silicon on 2026-10-24 (rows in `data/results/mspm33c321a.csv` and `mspm0g3519.csv`, test `port_example_behavior`; write-up in `docs/experiments/2026-10-24-mspm33-mspm0-driverlib-both-ways.md`). "As is" means the app `.c` files were copied byte for byte. Tools: `crossport_pair.py` builds (either direction), `bench_pair_run.py` flashes and reads the result over the probe.

### 7.1 MSPM0G3519 DriverLib example to the LP-MSPM33C321A

| Part | What it took | Run it came from |
|---|---|---|
| App code (`DL_GPIO_*`, `DL_SYSTICK_*`, `DL_Timer*`, `DL_SPI_*`, `DL_UART_*`, `DL_CRCP_*`) | As is. The function names are shared. | gpio_toggle_output, systick_periodic_timer, timx_timer_mode_periodic_sleep, timx_timer_mode_pwm_edge_sleep, spi_controller_internal_loopback_poll, crcp_calculate_checksum: pins, timer registers and RAM variables read as the app defines them |
| Configuration | Carry the example's `.syscfg`. Swap the first three comment lines for the MSPM33 device (`MSPM33C321A`, package `LQFP-100(PFA)`, board `/ti/boards/LP_MSPM33C321A`), comment out every `.$assign` pin line so SysConfig picks pins, comment out the `SYSCTL` clock-tree lines, and add the `ProjectConfig` block (`genLinker`, `genStartup`, `genOpts`, `genLibs` true) if the file has none. SysConfig then names each remaining rejected line; comment it out and rerun. | the same six, 1 to 3 dropped lines each |
| Header | None. A one-line `ti/driverlib/m0p/dl_core.h` that includes the `m33` one is needed only if the app includes the MSPM0 core header by name. | |
| Edits | None to the app. A UART example that expects a TX to RX wire runs without one after `UART1.enableInternalLoopback = true;` in the `.syscfg`. | uart_rw_multibyte_fifo_poll: rxPacket reads MSP! |
| Build | The target SDK's makefile with `MSPM33_SDK_INSTALL_DIR`, `SYSCONFIG_TOOL`, `TICLANG_ARMCOMPILER` set and `GCC_ARMCOMPILER=` empty. Start from the MSPM33 example of the same name, or from its `empty` example. | |
| Does not move | I2C (the UNICOMM block: `DL_I2CC_` and `DL_I2CT_` names, `DL_I2C_ClockConfig` is unknown), ADC12 (the MSPM33 has the HSADC block), a UART example that uses `DL_INTERRUPT_GROUP_1`, any DMA example (SysConfig wants an MSPM33 DMA instance added). | i2c_controller_rw_multibyte_fifo_poll, adc12_single_conversion, uart_internal_loopback_standby_restore, uart_tx_console_multibyte_repeated_fifo_dma: build stops |

Pins land on the pins SysConfig picks (the app's LED is not the board's RGB LED unless you assign it). Read the generated `ti_msp_dl_config.h` before you flash.

### 7.2 MSPM33 DriverLib example to the LP-MSPM0G3519

| Part | What it took | Run it came from |
|---|---|---|
| App code | As is for GPIO, SysTick, timers, SPI, UART and CRC. | systick_periodic_timer, timx_timer_mode_periodic_sleep, timx_timer_mode_pwm_edge_sleep, spi_controller_internal_loopback_poll, uart_internal_loopback_spgss0_uc1, uart_tx_rw_multibyte_fifo_poll, crcp_calculate_checksum |
| Configuration | The same moves the other way: device `MSPM0G3519` (`--device "MSPM0G351X"`, package `LQFP-100(PZ)`, board `/ti/boards/LP_MSPM0G3519`), pin lines commented out, `launchPadShortcut` lines dropped (the MSPM0 GPIO module has no such property), MSPM33 `SYSCTL` and `ProjectConfig` lines dropped when rejected. Keep the pins your wiring uses out of SysConfig's reach: add a GPIO instance that holds them as inputs (`--reserve PA10,PA11,...` in `crossport_pair.py`). | the same seven |
| Header | None needed for these examples. | |
| Edits | `gpio_toggle_output` names one port per LED (`GPIO_LEDS_USER_LED_2_PORT`). The G3519 LEDs share port B, so SysConfig writes `GPIO_LEDS_PORT` only: 5 lines changed in the app, and the pins assigned to PB22, PB26 and PB27 in the `.syscfg`. A UART example that expects a wire runs without one after `UART1.enableInternalLoopback = true;`. | gpio_toggle_output (3 LED pins changed, LED 1 and 3 together, LED 2 opposite) |
| Does not move | I2C (12 `DL_I2CC_` names are `DL_I2C_CONTROLLER_` and `DL_I2C_` names on the MSPM0; not a prefix swap), HSADC (the G3519 has ADC12), a UART RX example whose pin has a pull-up SysConfig rejects on an open-drain pin. | i2c_controller_rw_byte_poll, hsadc_single_conversion, uart_rx_rw_multibyte_fifo_poll: build stops |

On the LP-MSPM0G3519 the console pins PA10 and PA11 and the pins to PA13 can carry other wiring. Reserve them as above and use the other UART or an internal loopback.

### 7.3 Plain C: TI's CC1407P host driver to either board

| Part | What it took | Run it came from |
|---|---|---|
| TI's driver (7 files) | As is, in either SDK's `empty` project. | `portability/cc1407p-host/build_board.sh g3519\|m33 <work dir>` |
| Board layer | `hal_bare.c` and seven `hb_*` hooks. The fake-radio hooks (`c28x/hooks_fake.c`) need no pins. A real board writes each hook as a few DriverLib calls. | first SPI message `00 04 11 00 02 32 00 00 E9 71` and CRC-16 0x29B1 on both boards, as on a PC |
| Watch for | Two API structs are smaller on TI Arm Clang (1-byte enums). They are not sent on the wire, but code that copies them to a PC build must not assume the PC sizes. | sz[9] and sz[10] read 3 and 12 here, 6 and 42 on the PC |

No radio was attached, so no packet was sent from these two boards. The one board that has sent packets is the LP-MSPM0G5187.

## Board and SDK notes for porters

- **Radar SDK on a Mac.** The MMWAVE-L-SDK 05.05.04.02 installer is a 32-bit Linux or Windows program (`MMWAVE_L_SDK_05_05_04_02-Linux-x86-Install.bin`, myTI login and export approval). It runs in an x86_64 Ubuntu 22.04 VM (lima, instance `sdk22`, with `libc6:i386`, `libstdc++6:i386`, `zlib1g:i386`); on Ubuntu 26.04 it stops with `Unable to initialize installer. Is /tmp writable ?`. The boot-image packer is Linux or Windows only: in the VM, `make -o syscfg -o generated -o <name>.release.out <name>.release.appimage`. On the Mac its makefiles stop with `make: obj/release/: No such file or directory` until you add `"MKDIR=mkdir -p" "RMDIR=rm -rf" "RM=rm -f" COPY=cp CAT=cat TOUCH=touch` to the make line. Flash with `../../iwrl6432boost/tools/flash_arprog.py` from a Python that has pyserial: S1 position 1 off, S2, flash, S1 position 1 on, S2. The SDK ships no macOS installer or image packer.
- **Wi-Fi SDK images** go on through the SimpleLink Wi-Fi Toolbox (`run_cc35x1.sh`). After a failed debug connect the programmer stops with `return error code -615` until both the probe cable and the board's J12 cable are cycled. On 2026-10-11 the 11 F3-app images programmed but the console stayed silent, even for TI's own untouched build, so that batch stays at programmed; the cause was not isolated.
- **Radar board LED.** If `gpio_led_blink` prints `GPIO LED Blink Test Passed!!` on the IWRL6432BOOST but no LED blinks, the radar SDK's project drives ball J10, which is the LED pin on TI's xWRL6432 EVM; change the pin in the example's `.syscfg` to the one the BOOST's user LED (D7) uses. The BOOST schematic was not checked.
- **CC35X1 console silent after programming.** Read at the program's own baud first (921600 for the CSI image, 115200 for the SDK driver examples), then cycle both the probe cable and J12. If no bytes arrive even then, suspect the board rather than your image: TI's own untouched example was silent the same way on 2026-10-11.
- **AM263Px and radar examples the header cannot fix.** `gpio_input_interrupt` (both ways) stops on `GPIO_GET_BANK_INDEX` or `GPIO_clearInterrupt`: the two chips have different GPIO interrupt models, so write the interrupt part against the target's own example. The two SDKs' GPIO and memory APIs differ. The radar's `edma_*_transfer` examples stop on `CSL_APP_RAM_U_BASE`: they put buffers in the radar's memory; point them at a buffer in the AM263P's RAM.
- **Recording a run:** each tool writes one JSON line per example. Add one row per batch to `../matrix.csv`, then run `python3 ../render.py`.

Next: find your source and target in the table in section 1, run that tool on one example with `--only` or `ONLY=`, and read its result line before running the whole set.
