# What should an AI coding assistant know before working on a TI LP-EM-CC2755P10 LaunchPad development kit?

That a chip erase locks the debug port on this chip until both configuration areas are valid again, so never erase to "reset" it, and that the board has no USB port and needs a separate XDS110 debugger. This file is the short list of what was learned, each item from runs on one board unless it says otherwise.

Terms: [docs/vocab.md](../docs/vocab.md) and the [LP-EM-CC2755P10 FAQ](FAQ.md) in the Fox Red Labs repo. In short: **CCFG** and **SCFG** are the two configuration areas at 0x4E020000 and 0x4E040000 that must both be valid before the debug port opens; **SACI** is the chip's device management interface, which programs flash without a debug connection; **DSLite** is UniFlash's command-line tool; **RCP** is a radio-only co-processor image. 1 kB = 1,024 bytes and 1 GB = 1,024 MB. Checked 2026-10-07 to 2026-10-10 on an Intel Mac, macOS 15.7.9, with SimpleLink Low Power F3 SDK 9.21.00.36 LTS, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, UniFlash 9.6.0 and CCStudio 21.0.1 (the Thread images used `ot-ti` thread-v1.4-ti-2.1 with GCC 12.2 and SysConfig 1.27.0), on one LP-EM-CC2755P10.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths below in backticks starting `cc2755p10/` are in the Fox Red Labs repo, where the evidence for each line lives; they are not in your copy, and links in this file only work inside that repo.

## The board

| Part | Fact |
|---|---|
| Chip | CC2755P10: Arm Cortex-M33 at 96 MHz with FPU, 1,024 kB flash, 162 kB RAM, a math unit (APU), a security unit (HSM), 2.4 GHz radio up to +20 dBm (TI data sheet SWRS306E, per `cc2755p10/README.md`) |
| Linker regions | 928.0 kB flash and 163.0 kB SRAM (`cc2755p10/thread/README.md`) |
| LEDs, buttons | Red LED DIO16, green LED DIO12, two buttons, 1 MB SPI flash, printed antenna with a U.FL socket (`cc2755p10/README.md`) |
| Debug probe | None on the board and no USB port. Plug it onto an LP-XDS110ET or LP-XDS110 debugger with the TGT VDD jumper on XDS. On EXT it stays dark (`cc2755p10/FAQ.md`) |
| Serial | Thread console is the backchannel UART at 921600 baud; 115200 prints garbage (`cc2755p10/thread/README.md`) |
| Zephyr | Upstream Zephyr 4.4.2 on the bench Mac has a board file for `lp_em_cc2340r5` only, none for this chip. TI's Zephyr fork v3.7.0-ti-1.00.01_ga has the board: hello_world printed its greeting and basic-blinky toggled the green LED, flashed 2026-10-17 (`data/results/cc2755p10.csv`, row `vendor-fork`) |

## Start here

Tools it was built with: SimpleLink Low Power F3 SDK 9.21.00.36 LTS (`<SDK>`), TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, UniFlash 9.6.0, CCStudio 21.0.1 (its `ccs/scripting/run.sh` runs the recovery script). Thread images used `ot-ti` thread-v1.4-ti-2.1 with GCC 12.2 and SysConfig 1.27.0.

Probe: an LP-XDS110ET or LP-XDS110 under the board, TGT VDD jumper on XDS (on EXT the board stays dark). `xdsdfu -e` lists the serial; put it in the "Enter the serial number" property of `cc2755p10/CC2755P10.ccxml`.

The safe way, in plain words. Never chip-erase this chip, and do not edit its CCFG or SCFG. The debug port opens only when both are valid, and a DSLite load erases first, which clears the SCFG. So flash only a `.hex` that carries a valid CCFG and SCFG: TI Arm Clang builds of the SDK examples do; `ot-ti` GCC builds do not (`cc2755p10/thread/make_flash_hex.py` makes one). The top 96 kB of flash (0xE8000 to 0xFFFFF, HSM firmware) cannot be read or written, and OTP at 0x4E020800 is never read or written.

Backup: no `backup.py` recipe. The recorded reads are the ones `cc2755p10/restore.sh` checks with (the length counts 32-bit words). The SCFG was never backed up; `restore.sh` writes the SDK `drivers/empty` SCFG instead. Keep the files outside the repository:

```bash
/Applications/ti/uniflash_9.6.0/dslite.sh --mode memory -c cc2755p10/CC2755P10.ccxml -r 0x0,0x3A000 -o <run folder>/backup-flash-1.bin
```

```bash
/Applications/ti/uniflash_9.6.0/dslite.sh --mode memory -c cc2755p10/CC2755P10.ccxml -r 0x4E020000,0x200 -o <run folder>/backup-ccfg-1.bin
```

Build (14 s):

```bash
cp -R <SDK>/examples/nortos/LP_EM_CC2755P10/drivers/empty .
```

```bash
make -C empty/ticlang SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR=<SDK> SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh TICLANG_ARMCOMPILER=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
```

Flash. Avoid the `.bin` (about 1.22 GB), `-v`, and any chip erase. `-e` prints progress; it is not erase.

```bash
/Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c cc2755p10/CC2755P10.ccxml -e -f -u empty/ticlang/empty.hex
```

Check: the red LED (DIO16) toggles once a second. Change the sleep time in `empty/empty.c`, build and flash again.

If the debug port is already closed (`Debug is not allowed based on`), recover over SACI, which needs no debug connection. Run `check` first (writes nothing), then `write` with TI's blink `.hex`, a CCFG and an SCFG. It refuses a CCFG or SCFG that closes debugging. Do not erase again:

```bash
/Applications/ti/ccs2101/ccs/scripting/run.sh cc2755p10/recovery/saci_program.js cc2755p10/CC2755P10.ccxml <blink.hex> <ccfg.bin> <scfg.bin> check
```

Take the two reads above before the first flash. Once the board is flashed, the shipped main flash and CCFG cannot be read back and `restore.sh` has nothing to restore from. Without those two files, the step below cannot run: say so and do not substitute another image.

The third file, `scfg-sdk-default.bin`, is the 1 kB SCFG record (0x4E040000) of the SDK `drivers/empty` hex from the build below, which carries it with its CRC32 filled. Its SHA-256 must be `e39761ce57d24e460d49fdcb15f56ca2914664e10c52c51a1afe3cfe3d1517ea`; `restore.sh` also checks the CRC32 at offset 228. No script in the repo cuts it out of the hex, so cutting it out is untested here: check the hash before using the file.

Restore: `cc2755p10/restore.sh <run folder>` needs `backup-flash-1.bin`, `backup-ccfg-1.bin` and `scfg-sdk-default.bin` in that folder. It flashes, reads back and prints `RESTORED: main flash and CCFG read back identical to the Minute 0 backup` (46 s).

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| `FRL-E099 FAILED: KeyError: 'deviceid'` from `frl identify --board cc2755p10` | Port-tool bug: the board form of the command expects a device id this chip's route does not return (2026-10-25) | Use `frl identify --probe <serial>`; it names the chip (CC2755P105E0WRHAR) | `OK: probe ... is attached to CC2755P10...` |
| The portable app on this board (TI Drivers) | ADC input that needs no wire: SysConfig `adc0.internalSignal = "Battery Channel"` (VDDS/3); reads about 1374 counts at 3.3 V. The app UART is the probe's second port at 115200 (run 2026-10-25) | Build with `portability/three-ways/tidrivers/build_cc2755p10.sh` (SysConfig 1.28.0 here) | `serial_check.py` PASS |
| `Failed: Unknown Property: FlashEraseSetting` from `dslite flash -s FlashEraseSetting=Chip Erase` on this chip | The CC27xx flash loader has no such setting (it has `RetainMainSectors`, default true, which keeps the HSM sectors); that line is the CC2340R5's (`frl` run 2026-11-02) | Leave the setting out: the default erase is what `restore.sh` uses. `frl`'s route lists `dslite_erase_settings` | DSLite prints `Success` |
| `frl` blink check fails `FRL-E040 ... stayed at one level` though the LED blinks | Reads through DSLite take about 4 s, close to three periods of the 1.35 s default pattern, so back-to-back reads sampled one phase (run 3 of 2026-11-02) | Update `tools/frl/lpf3.py`: reads now sleep an uneven 0 to 1.4 s apart, up to 16 | Three runs in a row pass |
| `Error -2001` right after a TI example such as `zigbee/rf_test` ran, with debug authentication "not required" | Unknown. A probe reset and a `uhubctl` hub cycle did not bring it back; the hub cycle most likely does not cut this board's power | Unplugging the probe's USB for ten seconds is not run; `docs/remote-board-recovery.md` shows how to read the boot status | Boot status read after each step |
| `Debug is not allowed based on` after flashing an `ot-ti` Thread image | The GCC build leaves the CCFG and SCFG checksums at 0, so the port stays closed | Recover over SACI (above), then flash a hex from `cc2755p10/thread/make_flash_hex.py` | Debug connects after reset |
| `undefined node label 'boosterpack_spi'` from `west build --shield ti_bp_cc140xp` | The board file defines `boosterpack_header` but not `boosterpack_spi` | Build with no `--shield`; pass `-DEXTRA_DTC_OVERLAY_FILE=ports/cc1407p-zephyr/boards/lp_em_cc2755p10.overlay` | Four samples build |
| A CC1407P host image prints nothing, or stops after its first line | TI's `TRX_Host_open` waits for the radio with no timeout, so a missing BP-CC1407P hangs it (`cc2755p10/host-cc1407p/README.md`, 2026-10-24) | Fit the radio (red LED jumper off, wire from header pin 23 to pin 10) or add your own timeout | `X45 tx_once returned` prints |
| TI Drivers example from the LP-EM-CC2340R5 on this board | Same F3 SDK, same app source; only SysConfig and the linker file differ (33 of 33 ran 2026-10-24) | Copy the LP-EM-CC2755P10 project and overwrite its `.c` files; the `log` example needs 3,000,000 baud, every other UART example 115200 | UART prints the example's banner |
| `sysconfig_cli.sh: not found` in `ot-ti`'s `script/build` | It checks only `~/ti/` and `/opt/ti/` and then passes an empty path | Export `TI_SYSCONFIG_ROOT` | Build starts |
| `<cerrno>` tried including `<errno.h>` but didn't find libc++'s `<errno.h>` | The SDK's Zigbee makefile lists TI's POSIX folder first | Use `-idirafter` on `source/ti/posix/ticlang` for C++ files | C++ file compiles |
| `cannot initialize return object of type 'char *' with an rvalue of type 'const char *'` | At `-Oz` TI Arm Clang turns on inline `index()` in `strings.h` | Add `-U_INLINE` for C++ files | C++ file compiles |
| `field has incomplete type 'serialRomBldrParam_t'` from `hw_fcfg.h` in C++ | The struct sits in a union in a struct, which means something else in C++ | Read the chip's MAC address and temperature from a C file; call it from C++ | C++ file compiles |
| `invalid option: --wrap=...` | TI's linker has no `--wrap` | Drop the flag, or define the `__real_` names as stubs | Link passes |
| Thread console prints garbage | It runs at 921600, not 115200 | Open the backchannel UART at 921600 | `version` prints `OPENTHREAD/1.4.0.0` |

Traps shared with other boards: TRAPS.md.

## Radios and stacks

Sizes are linker figures for each example as shipped, F3 SDK 9.21.00.36 (`data/results/cc2755p10.csv`):

| Stack | What ran | Flash | RAM left |
|---|---|--:|--:|
| Bluetooth LE `basic_ble` | Builds and runs; `data_stream` on air 0.90 s after flash, seen at -47 dBm, echo median 29.2 ms for 20 bytes | 205.3 kB | 81.0 kB |
| Zigbee router `onoff_light` | Built; join not tested | 274.2 kB | 126.5 kB |
| Zigbee end device `onoff_switch` | Built; join not tested | 205.7 kB | 132.1 kB |
| DMM `mac_sensor_ble_basic` | Links and starts; its serial menu printed. Over-the-air checks not done | 253.8 kB | 57.4 kB |
| Thread `ot-cli-ftd` | Forms its own network, `leader` 6 to 12 s after `thread start`. No second node joined | 432.2 kB | 66.5 kB |
| Thread `ot-cli-mtd` | Built | 370.1 kB | 72.5 kB |
| Matter | Not run | | |

The same DMM example is 2.1 kB short of RAM on the LP-EM-CC2340R5 (`data/results/cc2755p10.csv`).

## What ran here

| Project | Result | Source |
|---|---|---|
| SDK example sweep | 106 of 106 make-route builds; 84 images run 6 s each, 83 of 84 clean | `data/results/cc2755p10.csv` |
| CoreMark, bare metal | 426.31 at -O3 (4.44 per MHz); 211.01 at -Oz | `cc2755p10/baremetal/` |
| Interrupt latency | 21 to 35 cycles | `data/results/cc2755p10.csv` |
| AES, hardware over software | 92.8 times (driver, polling) | `data/results/cc2755p10.csv` |
| 256-point FFT | q15 0.160 ms, f32 0.224 ms | `data/results/cc2755p10.csv` |
| Room-occupancy networks | 500, 2k, 8k and 30k parameters all fit; 30k runs in 39.0 ms at -O3 | `data/results/cc2755p10.csv` |

<!-- ports:start (tools/render_ports.py) -->
## Ports that ran here

Code written for other chips and tools, brought to this board. The files to copy, the command and what you see for each are in `docs/ports.md`; the method and a prompt to port your own are there too. Works ran and did what it should; Could work ran in part.

| Port | Status | Result | Evidence |
|---|---|---|---|
| ESPHome Zigbee device on the LP-EM-CC2755P10 | Works | An ESPHome YAML with cc27xx: builds and flashes a Zigbee device that joins Home Assistant on the LP-EM-CC2755P10. | `docs/experiments/2026-10-13-esphome-cc27xx-platform.md` |
| Matter smoke and CO alarm over Thread on the LP-EM-CC2755P10 | Works | TI's Matter lighting app rebuilt as a smoke and CO alarm boots on the LP-EM-CC2755P10, exposes the smoke alarm cluster and changes state from a UART line 20 of 20 times. | `mspm0g5187/alarm/radio/matter-thread.md` |
| OpenOCD flash driver for the LP-EM-CC2340R5 and LP-EM-CC2755P10 | Works | OpenOCD erases, writes, verifies and restarts a blink on the LP-EM-CC2340R5 over the XDS110, with no CCStudio or UniFlash. | `docs/experiments/2026-10-18-openocd-cc23xx-driver.md` |
<!-- ports:end -->

## Where the evidence lives

Start at `cc2755p10/FAQ.md` for each problem with its exact error and fix, then `cc2755p10/thread/README.md`. Rows are in `data/results/cc2755p10.csv`. If this file is wrong about your board, correct the FAQ first.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-25. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- Error -2001 or -615 after TI's zigbee/rf_test example, cause unknown (`cc2755p10/FAQ.md`, `docs/remote-board-recovery.md`, 2026-10-09)
- A chip erase closes the debug port: Debug is not allowed based on, Error -2001, until main flash, CCFG and SCFG are programmed again (`cc2755p10/FAQ.md`, `docs/remote-board-recovery.md`, 2026-10-09)
- A GCC ot-ti Thread image leaves the CCFG and SCFG checksums at 0, so the port stays closed (`cc2755p10/AGENTS.md`)
- SACI blank-check commands aimed at the HSM area time out and leave boot status 0x3F (`docs/remote-board-recovery.md`, 2026-10-09)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| tools/recovery/cc2x_status.js: read the boot status (reads only), before and after each step | remote | proven | `tools/recovery/cc2x_status.js`, `docs/remote-board-recovery.md`, 2026-10-09 |
| xds110reset -a toggle -d 100 (took boot status 0x3F to 0xBA once) | remote | proven | `docs/remote-board-recovery.md`, 2026-10-09 |
| xds110reset -a toggle -d 100 (after zigbee/rf_test) | remote | failed | `docs/remote-board-recovery.md`, `cc2755p10/FAQ.md`, 2026-10-09 |
| uhubctl power cycle of the probe's hub port, 5 s (the board most likely kept power) | remote | failed | `docs/remote-board-recovery.md`, 2026-10-09 |
| Connect under reset (Error -615, one try) | remote | failed | `docs/remote-board-recovery.md`, 2026-10-09 |
| SACI programming: recovery/saci_program.js check, then write (blink hex, CCFG, SCFG); skip the chip erase on a blank chip (for the erase lockout, under a minute; HSM firmware 4.7.6 was intact) | remote | proven | `cc2755p10/recovery/saci_program.js`, `docs/remote-board-recovery.md`, 2026-10-09 |

USB hub ports recorded for this board:

- The probe's hub port does not cut power, 2026-10-09. A 5 s cycle left the chip at boot status 0x3F, so the board most likely kept power.

Restore: cc2755p10/recovery/saci_program.js; cc2755p10/restore.sh builds the image (not run since the SCFG was added). Regions: main flash, CCFG 0x4E020000 (2 kB), SCFG 0x4E040000 (1 kB); HSM firmware through UniFlash if missing (not the SDK's 3.5.0 file). (`cc2755p10/restore.sh`, `docs/remote-board-recovery.md`)

<!-- recovery:end -->
