# What should an AI coding assistant know before working on a TI LP-CC1354P10-1 LaunchPad development kit?

Short answer: the debugger on this board only connects in the first few seconds after power-on (cause unknown, suspected chip limit), so a program that sleeps locks you out until the USB cable is replugged, and that TI ships no makefile for its Zigbee and DMM examples. This file is the short list of what was learned. Each item is from runs on one board unless it says otherwise.

Terms: [docs/vocab.md](../docs/vocab.md) and the [LP-CC1354P10-1 FAQ](FAQ.md) in the Fox Red Labs repo. In short: **ZHA** is Home Assistant's built-in Zigbee integration; **DMM** is TI's scheduler that shares one radio between two stacks; a **sleepy end device** is a battery Zigbee node that wakes to poll its parent; a **border router** links a Thread network to the home network; **CCFG** is the chip's boot configuration area. 1 kB = 1,024 bytes. Checked 2026-10-06 to 2026-10-11 on an Intel Mac, macOS 15.7, with the SimpleLink Low Power F2 SDK 8.33.00.16, CCStudio 21.0.1, TI Arm Clang 4.0.4 and 5.1.1, SysConfig 1.28.1 and Zephyr 4.4.2, on one LP-CC1354P10-1 with Home Assistant ZHA.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths below in backticks starting `cc1354p10/` are in the Fox Red Labs repo, where the evidence for each line lives; they are not in your copy, and links in this file only work inside that repo.

## The board

| Part | Fact |
|---|---|
| Chip | CC1354P10: Arm Cortex-M33 (CPUID `0x410FD214`) at 48 MHz, Sub-1 GHz and 2.4 GHz radios: Bluetooth LE, Zigbee, Thread, TI 15.4 (`cc1354p10/README.md`, `cc1354p10/FAQ.md`) |
| Memory | 1 MB flash. 296 kB RAM in TI's count; the linker gives programs 256 kB of SRAM (`cc1354p10/FAQ.md`) |
| Debug probe | XDS110 on the board, USB. Probe firmware on the board used was 03.00.00.22 when current tools wanted 03.00.00.43 (`cc1354p10/README.md`) |
| Serial | Backchannel UART at `/dev/cu.usbmodem<probe serial>1` (`cc1354p10/README.md`) |
| Unprogrammed state | The board arrived with TI's Project Zero but a nearly blank CCFG (`IMAGE_VALID_CONF` = `0xFFFFFFFF`), so the boot ROM skipped the program and sat in TI's serial bootloader. The CCFG was built from TI's own `ccfg.c` with the board's `BL_CONFIG` kept, so the bootloader stays as a recovery route (`cc1354p10/README.md`) |
| Not supported | Zephyr 4.4.2 has no CC13x4 or CC26x4 support. OpenOCD 0.12 has no CC13x4 target (`cc1354p10/README.md`) |

## Start here

Tools it was built with: SimpleLink Low Power F2 SDK 8.33.00.16 (`<SDK>`, folder `simplelink_cc13xx_cc26xx_sdk_8_33_00_16`), reinstalled 2026-10-22 from TI's GitHub (tag `lpf2-8.33.00.16`, sparse checkout, 149 MB, no `examples/` folder) on a drive with room (recipe: `cc1354p10/radio-rx/README.md`, then `cc1354p10/radio-rx/build_f2_libs.py <SDK>`; add `/kernel/.meta/` to the sparse list for SysConfig; blink: `cc1354p10/blink/README.md`), CCStudio 21.0.1, TI Arm Clang 5.1.1.LTS (4.0.4 for the Zigbee and DMM projects), SysConfig 1.26.3, UniFlash 9.6.0.

Probe: XDS110 on the board, plug the USB cable in; nothing to clip on. Connection file `cc1354p10/CC1354P10.ccxml` (ICEPick router driver, 2-pin cJTAG); put your probe serial in it (`xdsdfu -e`). The debugger connects only in the first seconds after power-on (Error -242 otherwise, TRAPS.md). Hands-free: switch the probe's hub port off 6 s (3 s failed), on with `uhubctl`, on, load at once.

Backup: use frl (`docs/frl.md`): it reads the 1 MB main flash and the 2 kB CCFG at 0x50000000 twice and compares them (tested 2026-10-24 and 2026-10-25, probe LS5600RE; rows `backup_restore` in `data/results/cc1354p10.csv`). Back up both regions before any write. The board arrived with TI's Project Zero and a nearly blank CCFG (`IMAGE_VALID_CONF` = `0xFFFFFFFF`), so it sat in the serial bootloader until a CCFG was written. A read-only check that needs no backup is the CPU ID (the length counts 32-bit words):

```text
DSLite memory -c cc1354p10/CC1354P10.ccxml -r 0xE000ED00,1 -o cpuid.bin
```

Do not write the CCFG at 0x50000000, and keep the board's `BL_CONFIG` so the ROM bootloader stays as a recovery route. A flash read leaves a running program holding the only debug connection: unplug USB for 3 s before the next load.

Build: no LED blink has been built on this board. The simplest build recorded is the bare-metal benchmark, from `cc1354p10/baremetal/` (makefile copied from TI's `drivers/empty` for this board):

```bash
mkdir -p build-O3 && make -C build-O3 -f ../makefile OPT="-O3" SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR=<SDK> SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh TICLANG_ARMCOMPILER=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
```

Flash: load with CCStudio scripting, which starts the program without a power cycle. A DSLite flash does not start a new program until `xds110reset -s <probe serial>` (a Bluetooth program needs a real power cycle):

```bash
/Applications/ti/ccs2101/ccs/scripting/python/run.sh cc1354p10/tools/ccs_load_run.py build-O3/bench.out
```

Check: the board prints a report on `/dev/cu.usbmodem<probe serial>1` at 115200 ending in `BENCH DONE`. `cc1354p10/baremetal/run.sh O3` builds, loads and reads it, and prints `WRONG IMAGE` if the flags printed differ from the ones built.

Restore: tested. frl writes the backup back with CCStudio scripting `loadBinaryProgram` from 0 and reads main flash and CCFG back identical to the backup (2026-10-24 and 2026-10-25; see "When the board stops answering").

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| `IcePick_C: Error connecting to the target: (Error -261 @ 0x0) Invalid response was received from the XDS110.` | A leftover DSLite still holds the probe | Wait for it to exit, then load again | Load connects |
| `CCS headless build complete - 0 out of 1 projects have errors`, but no `.out` | SysConfig stopped and nothing compiled | Look for the `.out` after every headless build | The file exists |
| `Build-configuration ... does not exist` | DMM uses `Release`, the Zigbee coordinator `default` | Use the project's own configuration name | Build runs |
| `Compiler version 'TICLANG_3.2.2' is not currently installed - defaulting to 'TICLANG_4.0.4.LTS'` | A project asks for an older compiler | Ignore it | It builds |
| `unk_manufacturer unk_model` in Home Assistant, or every read returns `0xC1` (`SOFTWARE_FAILURE`) | The example answers every attribute read with `0xC1` | Register a read callback with `zcl_registerReadWriteCB(...)` | Manufacturer and model show |
| `HTTP Error 400: Bad Request` from `zha.issue_zigbee_cluster_command` for the DMM light | Suspected, not checked: Home Assistant keeps the old "Temp Sensor" record for this address | Remove the device in Home Assistant, commission again from the board's menu | A light entity appears |
| `R,rtos,timer_rate,0,FAIL,counter 0 systick 45015` | `DWT->CYCCNT` reads 0 after a probe pin reset | Load from the debugger, or time with SysTick | Counter counts |
| `timed out waiting for @PK from the board` after `join: making an ML-KEM-768 key pair` | `TRNG_init()` was never called, so `TRNG_open()` returns NULL | Call `TRNG_init()` before `TRNG_open()` and check the handle | Exchange completes |
| `rfPacketRx` fails to link: `drivers_cc13x4.a`, `driverlib.lib`, `nortos_cc13x4.a` missing | TI's GitHub copy of the F2 SDK carries those libraries as source only | `python3 cc1354p10/radio-rx/build_f2_libs.py <SDK>` | Link passes |
| Receiver flashed but its serial port is silent | DSLite does not start the image | `xds110reset -s <probe serial>` | `rfrx: CC1354P10 listening at 915 MHz` |

Traps shared with other boards: TRAPS.md.

## Radios and stacks

| Stack | Result | Source |
|---|---|---|
| Bluetooth LE, TI Project Zero | Advertised, LED service written and read back from a Mac; renamed without source | `cc1354p10/README.md` |
| Zigbee sleepy sensor into Home Assistant | Joined; entity appeared about 160 s later (one run) | `cc1354p10/FAQ.md`, `cc1354p10/zigbee-sleepy-sensor/` |
| Zigbee coordinator, board to board | Formed a PAN; a CC2340R5 router joined in 6.2 s | `cc1354p10/README.md` |
| DMM Zigbee light plus Bluetooth LE | 459 kB flash, 33.8 kB RAM; joined Home Assistant in about 6 s; a Mac switched the light over Bluetooth 4 of 4. Home Assistant switching it not tested | `cc1354p10/dmm-zigbee-ble/README.md` |
| Thread (`ot-ti`) | Leader here; a CC2340R5 as RCP joined as router; ping median 42 to 44 ms, 20 of 20 | `cc1354p10/README.md`, `cc1354p10/thread/README.md` |
| Thread images | `ot-cli-ftd` 427 kB flash 94 kB RAM; `ot-cli-mtd` 363 and 88; `ot-rcp` 108 and 67 | `cc1354p10/thread/README.md` |
| Matter | Not built; not on TI's supported list for this part | `cc1354p10/README.md` |
| Sub-1 GHz receiver | Counted 21,658 of 21,658 packets over about 6 hours | `cc1354p10/FAQ.md` |

## What ran here

| Project | Result | Source |
|---|---|---|
| CoreMark, bare metal | 199.92 at -O3 (4.17 per MHz); 94.70 at -Oz; IRQ latency 23 to 24 cycles | `cc1354p10/baremetal/README.md` |
| Dhrystone and FFT | 1.199 DMIPS/MHz; 256-point q15 FFT 0.340 ms | `data/results/cc1354p10.csv` |
| Control loops | PID 117.1 kHz float, 120.6 kHz Q15; FOC 46.2 kHz | `data/results/cc1354p10.csv` |
| ML-KEM-768 | 20.8 ms key pair, 24.0 ms encapsulate, 29.1 ms decapsulate | `cc1354p10/pqc-join/` |
| ML-DSA-44 | 49.5 ms key pair, 154 ms average sign | `cc1354p10/FAQ.md` |

<!-- ports:start (tools/render_ports.py) -->
## Ports that ran here

Code written for other chips and tools, brought to this board. The files to copy, the command and what you see for each are in `docs/ports.md`; the method and a prompt to port your own are there too. Works ran and did what it should; Could work ran in part.

| Port | Status | Result | Evidence |
|---|---|---|---|
| Post-quantum key exchange and signatures on the LP-CC1354P10-1 | Works | ML-KEM-768 key exchange and ML-DSA-44 signatures pass NIST's known-answer vectors on the LP-CC1354P10-1. | `cc1354p10/pqc-join/README.md` |
| Sub-1 GHz CC1407P radio on an LP-MSPM0G5187, heard by an LP-CC1354P10-1 | Works | TI's CC1407P sub-1 GHz radio driver runs unchanged on the LP-MSPM0G5187, and an LP-CC1354P10-1 receives its packets: 21,658 of 21,658. | `mspm0g5187/radio/README.md` |
| MSPM0 code on the CC2340R5 and CC1354P10 radio chips | Could work | MSPM0 driver examples build for the CC2340R5 with a 47-line shim, the first 5 ran with the same pin states as TI's own build, and 4 of 4 ran on the CC1354P10. | `portability/README.md` |
<!-- ports:end -->

## Where the evidence lives

Start at `cc1354p10/README.md`, then `cc1354p10/FAQ.md` for each problem with its exact error and fix. Rows are in `data/results/cc1354p10.csv`. If this file is wrong about your board, correct the FAQ first.

External LP-XDS110 probe, flashing (2026-10-24): CCStudio `loadProgram` and `DSLite flash` fail at the second connect (Error -242, `OnPreFileLoaded(): Connect failed`). Power-cycle the probe port during a retrying `connect()`, `halt()`, then `loadBinaryProgram(<flash bytes from 0>, 0x0)`; this loaded 12 of 12 images. Loading a full `.out` also rewrites the CCFG with the SDK default, which differed from the board's original CCFG; back up 1 MB flash plus 2 kB at 0x50000000 and restore both.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-25. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- The debugger connects only in the first seconds after power-on (cause unknown, suspected chip limit); a sleeping image misses it: Error -242, A router subpath could not be accessed (`cc1354p10/FAQ.md`, `cc1354p10/AGENTS.md`, 2026-10-07)
- A flash backup read, or a connect after a BLE demo, uses the one debug session: Error -242 on the next load (`cc1354p10/FAQ.md`, 2026-10-10)
- External LP-XDS110 probe: loadProgram and DSLite flash fail at the second connect (Error -242, OnPreFileLoaded(): Connect failed) (`cc1354p10/FAQ.md`, 2026-10-24)
- Loading a whole .out rewrites the CCFG at 0x50000000 with the SDK default, which differed from the board's original (`cc1354p10/AGENTS.md`, 2026-10-24)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| frl's ccs13 route: one CCStudio Python scripting session per step, connect retried after a 6 s power cycle of the probe port when FRL_POWER_CYCLE names it, write with loadBinaryProgram from 0 (CCFG left alone) (three full cycles and four restores on the external LP-XDS110 probe, no connect refused, so the power-cycle branch did not run) | remote | proven | `docs/frl.md`, `tools/frl/ccs13.py`, 2026-10-25 |
| xds110reset -s <serial> (does not reopen the post-power-on window and did not clear -242 after a flash read; it does start a DSLite-flashed image) | remote | failed | `cc1354p10/FAQ.md`, 2026-10-10 |
| uhubctl off 6 s on the probe's port of a USB-C adapter, then load at once (3 s off failed; never switch while a load is connected (Error -261)) | remote | proven | `cc1354p10/FAQ.md`, 2026-10-08 |
| Power-cycle the probe port during a retrying connect(), then halt() and loadBinaryProgram(<flash bytes from 0>, 0x0) (12 of 12 images on the external probe; leaves the CCFG alone) | remote | proven | `cc1354p10/FAQ.md`, 2026-10-24 |
| Start a loop of loads, then unplug and replug USB (in on the 21st try; a replug with no loop missed the window twice) | hands | proven | `cc1354p10/FAQ.md`, 2026-10-07 |
| Unplug USB for 3 s, flash again (for -242 after a flash read) | hands | proven | `cc1354p10/FAQ.md`, 2026-10-10 |

USB hub ports recorded for this board:

- The probe's hub port power cut not checked, 2026-10-25. The external LP-XDS110 probe leaves USB within 1 s of `uhubctl -a off` and is back about 1 s after `on`, and a CCStudio connect works at once after each cycle; whether the chip itself lost power was not measured, because every connect resets this chip and clears what would show it (SRAM, reset source).
- The probe's hub port power cut not checked, date not recorded. The hands-free load route in the blink README; no separate before-and-after check.
- The probe's hub port cuts power, 2026-10-08. 6 s off, then load at once, reopened the debugger window every time; 3 s did not. Never switch it while a load is connected: the probe then answers Error -261 until the next off and on.

Restore: CCStudio scripting loadBinaryProgram. Regions: 1 MB main flash and 2 kB CCFG at 0x50000000: back up both, restore both. (`cc1354p10/AGENTS.md`)

<!-- recovery:end -->
