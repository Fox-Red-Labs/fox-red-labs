# What is on a TI LP-CC1354P10-1 LaunchPad development kit, and how do you get it running from a Mac terminal?

> Disclosure: Fox Red Labs is an independent lab run by a TI employee on personal time. Contributors' projects are their own. Not affiliated with, sponsored by, or endorsed by Texas Instruments or any other manufacturer.

This kit arrived with TI's Project Zero Bluetooth LE demo in flash that had never run, because its config area (CCFG) was almost blank. Building a correct CCFG from TI's own `ccfg.c` and flashing it with the original image made it advertise Project Zero on 2026-10-06; it was then renamed to "Fox Red 1354" without the source. Measured here since: CoreMark 199.92 at -O3 (4.17 per MHz), Thread and Zigbee links to a CC2340R5, and a Zigbee plus Bluetooth LE example that stays joined to Home Assistant.

Terms: [docs/vocab.md](../docs/vocab.md) and [FAQ.md](FAQ.md). In short: **CCFG** is the chip's boot configuration area; **cJTAG** is the 2-wire debug link; **DSLite** is UniFlash's command-line flasher; **DMM** is TI's scheduler that shares one radio between two stacks. 1 kB = 1,024 bytes.

## What is in this folder, and what is not

<!-- folder-contents -->
**In this folder:** this README; `AGENTS.md` (notes for an AI assistant, every line from a run); `FAQ.md` (every error hit, with its exact text and the fix). Folders: `baremetal/` bare-metal builds, `dmm-zigbee-ble/` Zigbee plus Bluetooth at once, `pqc-join/` post-quantum join demo, `radio-rx/` radio receive demo, `thread/` Thread demos, `tools/` helper scripts for this board, `zigbee-coordinator/` a Zigbee coordinator, `zigbee-sleepy-sensor/` a sleepy Zigbee sensor. Results are rows in `data/results/cc1354p10.csv` at the top of the repository, not in this folder.

**Not in this folder:** TI's SDK, CCStudio, SysConfig and the compilers (you install them; the versions are under "Where these facts come from"); built images (`.out`, `.hex`, `.bin`, `.elf`), which are not cleared for redistribution, so you build them with the command below; raw run logs and benchmark capture folders, which are lab records and not something to run or flash.

**Taking this folder alone:** the commands in this README run from the top of a clone and call `tools/` and `data/`. A sparse clone fetches only those, about 10 to 60 MB, instead of the whole repository (about 500 MB):

```bash
git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git
cd fox-red-labs
git sparse-checkout add cc1354p10 tools data cc2340r5 board-to-board
```

`docs/` is left out on purpose (236 MB, mostly pictures). Read a page a link points to there with `git show HEAD:docs/<file>`, for example `git show HEAD:docs/vocab.md`.


## The board

| Part | Fact |
|---|---|
| Kit | LP-CC1354P10-1, first plugged in 2026-10-06 |
| Chip | CC1354P10: Cortex-M33 (CPUID `0x410FD214`), Sub-1 GHz and 2.4 GHz radios (Bluetooth LE, Zigbee, Thread, TI 15.4) |
| Memory | 1 MB flash |
| Debugger | Onboard XDS110 probe, serial LS45013A, firmware 03.00.00.22 (current tools want 03.00.00.43) |
| Serial | Backchannel UART at `/dev/cu.usbmodemLS45013A1` |
| Not supported | Zephyr 4.4.2 has no CC13x4 or CC26x4 support; OpenOCD 0.12 has no CC13x4 target |

Sources: a read-only look at the board on 2026-10-06; [FAQ.md](FAQ.md); [AGENTS.md](AGENTS.md).

### What is on it

Read without changing anything (2026-10-06):

- CPUID `0x410FD214`: Arm Cortex-M33.
- FCFG1 `ICEPICK_DEVICE_ID` `0x1BB7802F`, `USER_ID` `0x1287F000`.
- Flash: a program in the first 212 kB (0x0 to 0x34FFF), rest blank. Strings in it: `project_zero.c`, LED service messages, BLE5-Stack paths from TI's build server. It is TI's **Project Zero** Bluetooth LE demo, built by TI.
- The backchannel UART (`/dev/cu.usbmodemLS45013A1`) was silent for 4 s after plug-in.
- **The program had never been running.** The config area (CCFG at 0x50000000) was almost blank: `IMAGE_VALID_CONF` = `0xFFFFFFFF`, so the boot ROM skipped the program and sat in TI's serial bootloader (`BL_CONFIG` `0xC5FFFFFF`). Silent UART, all-zero RAM and no Bluetooth all came from that.

## Ten-minute path (not timed)

The steps ran on 2026-10-06; no time was recorded for them. Step 1 is plugging in, the rest is in a terminal.

1. Plug the board's USB cable into the Mac. The probe is on the board; nothing clips on.
2. In the terminal: check the connection with the connection file below (explained under "Connecting from a terminal").

```bash
DSLite memory -c cc1354p10/CC1354P10.ccxml -r 0xE000ED00,1 -o cpuid.bin
```

3. Back up all 1 MB of flash and the CCFG (outside the repo).
4. Build a correct CCFG by compiling TI's own `source/ti/devices/cc13x4_cc26x4/startup_files/ccfg.c` (F2 SDK 8.33) with its defaults, keeping the board's `BL_CONFIG` so the ROM bootloader stays as a recovery route. Flashed it with the original image: after a power cycle the Mac saw **Project Zero** advertising TI's LED service.
5. Rename it without the source (`tools/rename_ble.py`, `tools/ti_cinit.py`). Both names sit in the linker's compressed startup data (copy-table record 0, 1.3 kB packed, 4.1 kB unpacked into RAM at 0x20000650). The tool unpacks it with a line-for-line port of TI's `copy_decompress_lzss.c`, edits the names, stores it as literal-only LZSS in blank flash at 0x35000, and repoints the copy table. TI's own decompressor reads it unchanged.
6. Result: advertises as **Fox Red 1354** (the advertising array holds 12 characters of name beside the BLE flags); GATT device name **Fox Red Lab CC1354** (21-byte slot). From the Mac: found at -39 dBm, connected, both LED characteristics written and read back 1, 0, 1 correctly.

Gotchas: a program flashed through DSLite does not start advertising until a real power cycle (a probe reset with `xds110reset` is not enough). While the demo runs, the debugger often fails with "A router subpath could not be accessed" (error -242) until the board is power-cycled. macOS hides the GAP service, so the GATT device name can only be read from a phone.

Services seen: Device Information (0x180A), LED (F0001110: LED0 1111, LED1 1112), Button (F0001120: 1121, 1122 notify), Data (F0001130).

### Connecting from a terminal

`cc1354p10/CC1354P10.ccxml` works with UniFlash's DSLite (command in step 2).

It is built from TI's template `ccs_base/arm/cc13xx_cc26xx_2pin_cJTAG_XDS110.ccxml`. Two things a hand-written file gets wrong: it needs the ICEPick router driver (`tixds510icepick_c.xml`), or DSLite says "An invalid processor ID has been found" without touching the chip; and it needs 2-pin cJTAG. OpenOCD 0.12 has no CC13x4 target, and plain 4-wire JTAG reads all zeroes. DSLite's memory length is in 32-bit words.

## One-command build

No single command builds the board's examples: TI ships no makefile for any of the 42 Zigbee or 11 DMM examples, only a CCStudio project and an IAR project. Bluetooth, TI 15.4, Wi-SUN, proprietary RF and driver examples do have makefiles. These steps build only; they do not flash. A clean rebuild of the DMM example took 30.5 s with `-j4`.

1. **CCStudio's headless command line**, every time:

       ccs-server-cli.sh -noSplash -workspace <dir> -application projectImport -ccs.location <example>.projectspec
       ccs-server-cli.sh -noSplash -workspace <dir> -application projectBuild -ccs.projects <name> -ccs.configuration <config>

2. **Import once, then plain make.** The first headless build writes a full makefile into the workspace's configuration folder. After that, `gmake` rebuilds without CCStudio: a clean rebuild of the DMM example took 30.5 s with `-j4` and produced the identical image. Use CCStudio's own `/Applications/ti/ccs2101/ccs/utils/bin/gmake` (GNU Make 4.4.1); macOS's built-in make 3.81 stops at the link step on `-Onone`.

Traps found on the way:

- The headless build summary says "0 out of 1 projects have errors" even when SysConfig failed and nothing was built. Check that the `.out` file exists.
- Configuration names differ between examples: the DMM project uses `Release`, the Zigbee coordinator `default`. A wrong name fails with "Build-configuration ... does not exist".
- A second import of the same project is refused, even with `-ccs.overwrite full`. Use a fresh workspace or copy changed files in.
- App sources are linked into the SDK, not copied. To change one, copy it into the repo and point the `.projectspec` at the copy, or the edit lands in TI's SDK.
- TI's Zigbee coordinator project for this board names its device CC2674R10 (a sibling chip); the board define `EM_CC1354P10_1_LP` is still correct.
- The DMM project asks for TI Arm Clang 3.2.2; CCStudio substituted 4.0.4.LTS without asking.
- Renaming `ble.deviceName` in the DMM example also needs TI's explicit `scanRes1.completeLocalName` line removed, or SysConfig stops.

### Software

- SDK: SimpleLink Low Power F2 SDK (was "CC13xx CC26xx SDK"), not installed. Public on GitHub: `TexasInstruments/simplelink-lowpower-f2-sdk`, newest tag seen `lpf2-8.33.00.16`, about 1.7 GB.
- Zephyr 4.4.2: no CC13x4 or CC26x4 support (`soc/ti/simplelink` has cc13x2_cc26x2, cc13x2x7_cc26x2x7, cc23x0).
- Portability: third TI SDK with TI Drivers (after MSPM0 and F3). Next test in `portability/`.

## What was measured

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| CoreMark | 199.92 per second (4.17 per MHz) at -O3; 94.70 at -Oz (1.97 per MHz) | 48 MHz, TI Arm Clang, bare metal, median of 3 | measured | `coremark`, `coremark_per_mhz` in `data/results/cc1354p10.csv` |
| Interrupt latency | 23 to 24 cycles | 48 MHz, bare metal | measured | `irq_latency` (bare) |
| GPIO toggle by register write | 1.51 cycles at -O3; 8.01 at -Oz | 48 MHz, bare metal | measured | `gpio_toggle` (register-bare-o3, register-bare-oz) |
| Thread ping to the CC2340R5 | Median 42 to 44 ms each way, 20 of 20, RSSI -30 | This board as leader, the CC2340R5 as TI's RCP joined as router | measured | `board-to-board/thread-two-radios-2026-10-06/` |
| Zigbee join, CC2340R5 router to this coordinator | 6.2 s | PAN 0xe99d | measured | `board-to-board/zigbee-cc1354-cc2340/` |
| BLE round trip from the CC2340R5 central | 195 ms average, 20 of 20 LED writes acked | Central found "Fox Red 1354" | measured | `board-to-board/ble-cc2340r5-central/` |
| DMM example: Zigbee plus BLE | Joined Home Assistant's Zigbee network in about 6 s; a Mac switched the light over BLE 4 of 4 while it stayed joined | `dmm_zr_light_remote_display_app` | measured | `dmm-zigbee-ble/README.md` |
| DMM example size | 459 kB flash, 33.8 kB RAM | Linker figures | computed | `dmm-zigbee-ble/README.md` |
| Sizes | Project Zero 212 kB flash; Zigbee coordinator 225 kB flash, 59 kB RAM; Zigbee+BLE DMM 470 kB flash, 44.6 kB RAM | Build outputs | computed | `data/results/cc1354p10.csv` |
| Headless rebuild of the DMM example | 30.5 s, identical image | `gmake -j4` with CCStudio's own gmake | measured | `cc1354p10/README.md` |

Sleep current is not measured.

## If you see this

- If you see `IcePick_C: Error connecting to the target: (Error -242) A router subpath could not be accessed`, start a loop of loads, then unplug and replug the USB cable: one load gets in during the few seconds the chip stays awake at boot. On 2026-10-07 the load went in on the 21st try, about 10 s after the replug.
- If you see `Compiler version 'TICLANG_3.2.2' is not currently installed - defaulting to 'TICLANG_4.0.4.LTS'` in a headless CCStudio build, it still builds, with the compiler you have.
- If you see `CCS headless build complete - 0 out of 1 projects have errors` and there is no `.out` file, nothing was built: SysConfig stopped. Check that the `.out` exists after every headless build.

The rest of the errors, with causes: [FAQ.md](FAQ.md).

## What ran on this board

Status uses Works and Could work. The last column names what did not run or what blocks it.

| Test | Status | Detail |
|---|---|---|
| BLE: Project Zero from a phone (TI SimpleLink Connect app): find it, toggle LEDs, read the button | Works | LEDs over BLE from a Mac; the phone app and the button did not run |
| Rename the BLE device without the example source | Works | "Fox Red 1354" advertised, "Fox Red Lab CC1354" as device name |
| BLE board to board with the CC2340R5 | Works | CC2340R5 central found "Fox Red 1354", 20 of 20 LED writes acked, 195 ms average round trip (`board-to-board/ble-cc2340r5-central/`) |
| Thread (OpenThread, `ot-ti`) | **Works, two radios** | Leader on this board; the CC2340R5 (as TI's RCP with a Mac host) joined over the air and became a router. Ping median 42 to 44 ms each way, 20 of 20, RSSI -30 (`board-to-board/thread-two-radios-2026-10-06/`) |
| Matter over Thread (`TexasInstruments/matter`) | Could work | Its family (cc13x4_26x4) is in TI's Matter v1.5 lighting example, but the part is not on TI's supported list; Linux build, about 25 GB; needs a Thread border router. |
| Zigbee: board to board, and into Home Assistant | Works | **Board to board**: CC1354P10 coordinator formed PAN 0xe99d, CC2340R5 router joined in 6.2 s (`board-to-board/zigbee-cc1354-cc2340/`). The Home Assistant join is in the next row |
| Coexistence: BLE plus Zigbee or 15.4 at once (DMM) | Works | **Runs, both sides.** TI's `dmm_zr_light_remote_display_app` (459 kB flash, 33.8 kB RAM by the linker) joined Home Assistant's Zigbee network in about 6 s and a Mac switched the light over BLE 4 of 4 while it stayed joined; a toggle from Home Assistant did not run (`dmm-zigbee-ble/README.md`) |
| Limits and speed: CoreMark, flash and RAM per stack, sleep current | Works | **CoreMark 199.92 at -O3 (4.17 per MHz), 94.70 at -Oz**, median of 3; IRQ latency 23 to 24 cycles (`baremetal/README.md`). Sizes: Project Zero 212 kB flash; Zigbee coordinator 225 kB flash, 59 kB RAM; Zigbee+BLE DMM 470 kB flash, 44.6 kB RAM. Sleep current is not measured |

## Where these facts come from

Checked 2026-10-06 and 2026-10-07. Tools: SimpleLink Low Power F2 SDK 8.33.00.16, CCStudio 21.0.1, TI Arm Clang 5.1.1.LTS (4.0.4.LTS for the Zigbee and DMM projects), SysConfig 1.26.3, UniFlash 9.6.0 with DSLite, CCStudio's gmake (GNU Make 4.4.1), Zephyr 4.4.2 (no support), OpenOCD 0.12 (no target). The Mac model and macOS version were not recorded. Files: `data/results/cc1354p10.csv`, `cc1354p10/AGENTS.md`, `cc1354p10/FAQ.md`.
