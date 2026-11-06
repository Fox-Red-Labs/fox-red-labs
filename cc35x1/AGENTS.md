# What should an AI coding assistant know before working on a TI LP-EM-CC35X1 LaunchPad development kit?

That the chip must be activated once, by burning a key into fuses, before it accepts any program, and that the board has no debugger, so it needs a separate XDS110 plus its own USB-C cable. This file is the short list of what was learned, each item from runs on one board unless it says otherwise.

Terms: [docs/vocab.md](../docs/vocab.md) and the [LP-EM-CC35X1 FAQ](FAQ.md) in the Fox Red Labs repo. In short: **activation** is the one-time step that burns a security key into the chip's fuses; the **Toolbox** is TI's SimpleLink Wi-Fi Toolbox, the command-line tool that activates, signs and programs; **CoreMark** is a standard CPU speed test; **OTA** is an over-the-air update; **CSI** is Wi-Fi channel state information. 1 kB = 1,024 bytes; 1 MB = 1,024 kB. Checked 2026-10-08 to 2026-10-11 on an Intel Mac, macOS 15.7.9, with SimpleLink Wi-Fi SDK 10.20.00.39, Wi-Fi Toolbox 4.3.23, SysConfig 1.28.0 and TI Arm Clang 5.1.1.LTS, on one LP-EM-CC35X1 Rev A with the CC3551E chip and debugger firmware 3.0.0.22.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths below in backticks starting `cc35x1/` are in the Fox Red Labs repo, where the evidence for each line lives; they are not in your copy, and links in this file only work inside that repo.

## The board

| Part | Fact |
|---|---|
| Chip | CC3551E: Arm Cortex-M33 at 160 MHz, 1.1 MB RAM, Wi-Fi 6 on 2.4 and 5 GHz, Bluetooth LE 5.4 (TI data sheet SWRS337B, per `cc35x1/README.md`) |
| Flash | 8 MB external flash chip (IS25WJ064F, found by `detect_flash_type`) (`cc35x1/README.md`) |
| On the board | RGB LED, two buttons, temperature sensor, accelerometer, chip antenna, 40 header pins (`cc35x1/README.md`) |
| Debug probe | None on the board. Plug it onto an LP-XDS110ET or LP-XDS110 with the TGT VDD jumper on EXT, and plug a second USB-C cable into the board for power. Leave J4 and J6 on the lower two pins (`cc35x1/README.md`) |
| Serial | `/dev/cu.usbmodem<probe serial>1`; the port ending in 4 stays silent (`cc35x1/FAQ.md`) |
| Cycle counter | `DWT->CYCCNT` reads 0 in programs loaded this way; time with the FreeRTOS tick, 1 ms (`cc35x1/FAQ.md`) |
| Zephyr | Zephyr 4.4.2 has no CC35xx board (`cc35x1/RESULTS-cc35x1.md`) |

## Start here

Tools it was built with: SimpleLink Wi-Fi SDK 10.20.00.39 (`<SDK>`), SimpleLink Wi-Fi Toolbox 4.3.23 (`<Toolbox>`), SysConfig 1.28.0, TI Arm Clang 5.1.1.LTS (TI tested the SDK with 4.0.4.LTS).

Probe: an LP-XDS110ET or LP-XDS110 under the board, TGT VDD jumper on EXT, J4 and J6 on the lower two pins, and a second USB-C cable into the board for power. The probe serial is the eight characters `xdsdfu -e` prints.

Activation comes first and cannot be undone. Check it (read-only), from `<Toolbox>`; only if it says `Life cycle OPERATIONAL is not valid`, and only when the person agrees, run the `custom_activation` command in `cc35x1/README.md` step 3 (about 5 s):

```bash
./simplelink-wifi-toolbox programmer -i XDS110 -param1 <probe serial> get_fuse_data --flash_type IS25WJ064F --activation_type sdk_example_key --verbose
```

Backup: none is possible. The program sits in external QSPI flash behind the chip's security and the Toolbox has no read-back command. Save the output of the command above instead.

Build, in the example's `freertos/ticlang` folder (copy `<SDK>/examples/rtos/LP_EM_CC35X1/drivers/empty` first). Four paths, because the SDK's `imports.mak` names folders a Mac lacks; `GCC_ARMCOMPILER=` stays empty:

```bash
make SIMPLELINK_WIFI_SDK_INSTALL_DIR=<SDK> SIMPLELINK_WIFI_TOOLBOX_INSTALL_DIR=<Toolbox> SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh TICLANG_ARMCOMPILER=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS GCC_ARMCOMPILER=
```

Sign the images with the Toolbox image builder (`<build>` is the absolute path of the build folder; `cc35x1/prebuild.sh` has the same lines):

```bash
make -s -f <Toolbox>/scripts/makefile all SDK_DIR=<SDK> SYSCONFIG_ARTIFACT=<build> BUILD_DIR=<build> BUILD_ARTIFACT=<build>/empty.out TOOLBOX_DIR=<Toolbox>
```

Flash, with absolute paths inside `tool_settings.json`. The Toolbox does not reset the board afterwards; press S1 or run `xds110reset -s <probe serial>`. Do not flash a raw `.bin` from address 0.

```bash
./simplelink-wifi-toolbox programmer -i XDS110 -param1 <probe serial> programming --tool_settings <absolute path to tool_settings.json>
```

Check: the red LED toggles once a second (60 s from an edited line to a blinking LED, 20 s for a program-only change). The period is set in the example's source (README step 4). Serial is `/dev/cu.usbmodem<probe serial>1` at 115200 for TI's examples.

Restore: reprogram TI's prebuilt Network Terminal, `programmer/binaries/pre_complied_application/vendor_app_no_ota/vendor_app.CC35XXE.out` in the Toolbox folder. It is not a byte-exact image of what shipped, and the exact command was not recorded. After the Wi-Fi CSI program, erase everything before programming anything else:

```bash
./simplelink-wifi-toolbox programmer -i XDS110 -param1 <probe serial> full_flash_erase --flash_type AUTO_DETECT --activation_type sdk_example_key
```

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| `Process is stopped. Life cycle OPERATIONAL is not valid. Get fuse data failed` | The chip is not activated; the tool is fine | Ask before activating (above) | `get_fuse_data` prints the fuse table |
| `Path to programming_instructions after path correction couldn't be found` | Relative paths in `tool_settings.json` resolve under the Toolbox folder | Rebuild with an absolute build folder, or make the paths absolute | `programming` starts |
| Serial silent after programming over the Wi-Fi CSI program, even TI's own examples | The CSI program lays the flash out differently, and images programmed over it stay silent | Run `full_flash_erase` (about 16 s), then program as usual | The new image starts by itself and prints |
| Apple Home hangs on "Connecting" | The commissioning window closed; TI's Matter example keeps it open 300 s | Erase, program again, scan within 5 minutes | Device pairs |
| Matter log turns into 0x00 and 0x80 bytes once Bluetooth starts | Cause not found; pairing still worked | Read the start-up log in the first seconds | Start-up log readable |
| The CSI example never prints `DAP streaming ready` | Plugin 1.20's firmware prints `DAP inferencing ready` instead | Wait for `DAP inferencing ready`, then send `ED 08 00 9E` | A result every 2 s |
| OTA menu says `Invalid input` or `Invalid choice` | The example reads a line feed as an empty answer to the next prompt | Send only a carriage return after each answer | Menu advances |
| OTA `Show components in flash` lists slot 2 as `FAILED` | A board programmed through the Toolbox starts with its second slots in `FAILED` | Menu `3`, then the slot number (5 for the vendor image; about 20 s) | State becomes `READY` |
| `ERROR rxData_DataPacketDisptcher(): Received VLAN packet - drop it` | Seen with a router that sends VLAN-tagged packets | Ignore it | Join, ping and iperf work |
| `Timeout expired connecting to Network` with only `[WLAN EVENT] Unexpected event [0x19]` before it | The board never found the network name; on macOS 15 `ipconfig getsummary en0` prints `<redacted>` for the name (`ports/wifi-provisioning-ble/`, 2026-10-25) | Type the network name; never read it from `ipconfig` | `[WIFI] connected, addr=` |
| `[PYI-36117:ERROR] Failed to execute script 'simplelink-wifi-toolbox'` while signing, or `make: *** Terminated: 15` while linking, in a batch of builds | The disk was down to a few hundred MB; the Toolbox and linker die without saying so (2026-10-24) | Free disk (delete each example's build folder after programming), then rerun that example | Signing ends with a `.sign.bin` and programming writes the report file |

Traps shared with other boards: TRAPS.md.

## Update over Wi-Fi from the Digital Lab (route B, 2026-10-22)

The page drives TI's `ota_example` menu over the probe's serial port (115200); the board downloads a signed image from an HTTPS server on your computer, its bootloader checks and swaps it, and you keep it or go back. The module is `site/flash/cc35-ota.js`, its data `site/flash/cc35-ota.json`, its test `tools/webflash/cc35-ota.test.mjs`; the Mac run that proved it is `cc35x1/ota/` (`build.sh` builds and signs the example at a version, `bridge.py` and `drive.mjs` run the same module through a serial-to-TCP bridge).

- The board must already run the OTA example on the two-slot flash layout, a one-time Toolbox step (the `programming` command above, with `cc35x1/ota/build.sh 0.0.1.0` output). The layout comes from the image's build: `external_memory_configurator.json` says `"ota": true` (two vendor slots of 2,688 kB) for `ota_example` and `uart2echo`, and `"ota": false` (one 6,248 kB slot) in the Doom builds. Read that file in the last programmed build folder; the board cannot be read back.
- Reprogramming with the Toolbox over the same layout rewrites the primary slots only: 45 s for the 1.29 MB image, no layout change, no activation or fuse command.
- Console banner of `build.sh` images: `CC35xx OTA Example Application FRL <version>`. The version for the update comes from `action_request_extra.txt` (`primary_vendor_image_version`), not from syscfg; the server lists a file as `[NEW]` only when its header version is higher than the active one.
- Serve with TI's `tools/ota_https_server/ota_server.py --dir <folder>`; the board's own address check passed against `https://<Mac Wi-Fi address>:8443` with TI's playground certificates. Name the file `vendor_image_<version>.bin` (the server classifies by name).
- Answers end with a carriage return only. After a prompt the board keeps printing status and `Received VLAN packet - drop it` lines, so a prompt is found anywhere in the text, not at its end.
- The board echoes what you type, the Wi-Fi password included. The module mutes the echo until the next line break and masks the password in every log.
- Measured 2026-10-22 (SDK 10.20.00.39, Mac as server on the same Wi-Fi): scan 1.9 s, join 2.2 to 2.7 s, server query 0.4 to 0.6 s, download 3.6 s (20.5 s when the spare slot was left `FAILED`, which the board erases first), install under 0.1 s, restart into the new image 3.1 s, keep or go back 3.1 s. Update to 0.0.2.0 then go back to 0.0.1.0, update and keep 0.0.2.0, then 0.0.3.0 into the other slot: four rounds, all as expected. After a keep the slots swap roles; after a go back the rejected slot shows `FAILED` with its old version.
- A server that cannot be reached prints `Server query failed (HTTPS: -11, HTTP status: 0)` and returns to the download menu; a boot that finds a downloaded image offers install, cancel or continue (`candidate` in the module).
- `get_fuse_data` was not run that day. The chip is activated: it programmed with `sdk_example_key` twice that day.

## Radios and stacks

| Stack | What ran | Source |
|---|---|---|
| Wi-Fi, TI Network Terminal | Scanned 18 access points; joined a WPA2 network on 5 GHz in 5.74 s to an IPv4 address (ten runs); TCP 12.1 and UDP 16.0 Mbit/s to a Mac, ping mean 17 ms | `cc35x1/README.md` |
| Bluetooth LE | Advertised at 100 ms; the Mac saw it at -34 dBm and connected in 0.52 s | `cc35x1/README.md`, `data/results/cc35x1.csv` |
| Bluetooth provisioning | A Python script on the Mac gave the Wi-Fi login over Bluetooth; join in 7.13 s | `data/results/cc35x1.csv` |
| Bluetooth provisioning with saved network and MQTT (chained demo) | Builds at 1,337 kB; boots with no network and advertises 0.90 s after the banner; a network it cannot find times out in 16.2 s and falls back to advertising (5 of 5, 2026-10-25). The Bluetooth write, join and MQTT publish have not run | `ports/wifi-provisioning-ble/README.md` |
| MQTT through a throwaway broker | 720 of 720 requests answered over an hour, median 167 ms | `cc35x1/README.md`, `data/results/cc35x1.csv`; repeat with `cc35x1/mqtt-test/README.md` |
| OTA over HTTPS | 2026-10-09: downloaded 27.7 kB in about a second, rebooted into the new image, an unaccepted reset rolled back. 2026-10-22: four rounds of a 1.29 MB image driven by `site/flash/cc35-ota.js`, update in 12 to 28 s | `cc35x1/RESULTS-cc35x1.md`, this file |
| Matter over Wi-Fi, TI lighting example | Built on a Mac in about 11 minutes in an Ubuntu 22.04 container; commissioned into Apple Home; toggles confirmed | `cc35x1/matter/README.md` |
| Matter thermostat (this repo's data model) | Paired into Apple Home; Google Home not done | `data/results/cc35x1.csv` |
| Wi-Fi CSI presence, TI model | Builds and runs, a result every 2 s; said "presence" in 135 of 136 windows in an empty room, so it does not detect out of the box | `cc35x1/README.md` |

## What ran here

| Project | Result | Source |
|---|---|---|
| CoreMark | 718.91 per second at -O3 (4.49 per MHz); 354.11 at -Oz; cause of the high figure unexplained | `cc35x1/RESULTS-cc35x1.md` |
| Dhrystone | 1.344 DMIPS/MHz at -O3 without inlining | `cc35x1/RESULTS-cc35x1.md` |
| Room-occupancy networks | 500, 2k, 8k and 30k parameters fit: 0.602, 1.538, 6.25 and 24.4 ms at -O3 | `cc35x1/RESULTS-cc35x1.md` |
| UART loop | 4 kB correct at every speed from 115200 to 5 Mbaud; no report from 6 Mbaud up, cause not diagnosed | `data/results/cc35x1.csv` |
| SDK examples | 31 of 32 build; 29 of 31 booted | `data/results/cc35x1.csv` |

<!-- ports:start (tools/render_ports.py) -->
## Ports that ran here

Code written for other chips and tools, brought to this board. The files to copy, the command and what you see for each are in `docs/ports.md`; the method and a prompt to port your own are there too. Works ran and did what it should; Could work ran in part.

| Port | Status | Result | Evidence |
|---|---|---|---|
| DNS ad blocker (sinkhole) on the LP-EM-CC35X1 | Works | The LP-EM-CC35X1 answers DNS on UDP port 53: names on a blocklist get 0.0.0.0 and the rest is forwarded to your gateway. | `docs/experiments/2026-10-20-cc35x1-dns-sinkhole.md` |
| Doom engine (doomgeneric and rp2040-doom's design) on the LP-EM-CC35X1 and the LP-MSPM33C321A | Works | The rp2040-doom design plays all three shareware demos on the LP-EM-CC35X1 and the LP-MSPM33C321A, with frame checksums equal to a Cortex-M33 emulator. | `docs/experiments/2026-10-20-doom-smallest-ti-chip.md` |
| ESP32-BLE-Gamepad (a Bluetooth HID gamepad library) on TI, ST and C2000 boards | Works | The ESP32-BLE-Gamepad descriptor and reports match the library's output byte for byte on four boards, and 15 of 15 button changes from the LP-EM-CC2340R5 reached a Mac as gamepad reports. | `docs/experiments/2026-10-19-esp32-ble-gamepad-port-matrix.md` |
| ESPHome Wi-Fi device on the LP-EM-CC35X1 | Works | A Wi-Fi ESPHome device runs on the LP-EM-CC35X1 with ESPHome's native API: a client on the board got 20 of 20 switch commands confirmed. | `docs/experiments/2026-10-13-esphome-cc35xx-platform.md` |
| Matter thermostat over Wi-Fi on the LP-EM-CC35X1 | Works | A Matter thermostat and occupancy sensor runs on the LP-EM-CC35X1, pairs into Apple Home and takes setpoint changes from the phone. | `docs/experiments/2026-10-10-cc35x1-matter-thermostat.md` |
<!-- ports:end -->

## Where the evidence lives

Start at `cc35x1/README.md`, then `cc35x1/FAQ.md` for each problem with its exact error and fix, `cc35x1/RESULTS-cc35x1.md` for numbers and `cc35x1/matter/README.md` for Matter. Rows are in `data/results/cc35x1.csv`. If this file is wrong about your board, correct the FAQ first.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-25. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- The board looks dead at 115200 baud while the CSI image prints at 921600 (`cc35x1/FAQ.md`)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| Read the serial port at the image's rate (921600 for the CSI program) | remote | proven | `cc35x1/FAQ.md`, date not recorded |
| Press S1 or xds110reset -s <serial> after a Toolbox flash (it does not reset the board) (xds110reset is remote; S1 is hands) | remote | proven | `cc35x1/AGENTS.md`, date not recorded |
| Restore a saved signed bundle: full_flash_erase, then programming with the bundle's tool_settings.json, then xds110reset (needed after any over-the-air update, which moves the running slot) (the Sparky 0.0.33.0 image was erased and reprogrammed twice from its backup bundle: banner 0.0.33.0, Wi-Fi joined, wake turn reached the voice server) | remote | proven | `ports/xiaozhi/target-cc35x1/README.md`, 2026-11-02 |

<!-- recovery:end -->
