# What goes wrong with the TI LP-EM-CC35X1 LaunchPad development kit on a Mac, and how do you fix it?

Problems with the LP-EM-CC35X1 (CC3551E chip: Arm Cortex-M33, 160 MHz, 1.1 MB RAM, external flash, Wi-Fi 6 and Bluetooth LE) from a Mac terminal. Checked on a real board 2026-10-08. 1 kB = 1,024 bytes. Terms: [docs/vocab.md](../docs/vocab.md).

**What caused it**, one tag per answer:

| Tag | Meaning |
|---|---|
| Our mistake | Something was done wrong in the run. Listed so you do not repeat it |
| Setup or handling | How the hardware was plugged in or set up |
| Tool or docs | A TI tool bug, a missing tool, or a missing or wrong instruction |
| Chip limit | The silicon cannot do it |
| Unknown | Not found |

## Building

### `make` on a CC35X1 example stops at once, or SysConfig cannot find the Toolbox. Which paths does it need?

Pass four paths on the `make` line: the SDK, the Toolbox, SysConfig 1.28 and the compiler. The SDK's `imports.mak` (Wi-Fi SDK 10.20.00.39, read 2026-10-07) names `/Applications/ti/simplelink_wifi_toolbox_4_3_18`, `/Applications/ti/ccs2100/ccs/utils/sysconfig_1.28.0/sysconfig_cli.sh` and `/Applications/ti/ccs2100/ccs/tools/compiler/ti-cgt-armllvm_4.0.4.LTS`; a Mac with CCStudio 21.0.1 and Toolbox 4.3.23 has none of those folders. Every example makefile loads the Toolbox as a second SysConfig product, so the Toolbox path is needed even for a plain driver example. The line that works here:

```bash
make SIMPLELINK_WIFI_SDK_INSTALL_DIR=/Applications/ti/simplelink_wifi_sdk_10_20_00_39 SIMPLELINK_WIFI_TOOLBOX_INSTALL_DIR=/Applications/ti/simplelink_wifi_toolbox_macos_4_3_23 SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh TICLANG_ARMCOMPILER=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS GCC_ARMCOMPILER=
```

TI tested the SDK with TI Arm Clang 4.0.4 (release notes); 5.1.1 is what CCStudio 21.0.1 installs. If a build fails only on a compiler message, that difference is the first suspect. Source: `cc35x1/prebuild.sh`.

## Activation and programming

### `get_fuse_data` prints `Process is stopped. Life cycle OPERATIONAL is not valid. Get fuse data failed`. Is the tool broken?

No. It means the chip is not activated. The command needs an activated chip (life cycle DEPLOYED) to print its table, and the error is the only way it shows OPERATIONAL. The board used here arrived this way. The command wrote nothing. To activate, see the ten-minute path in the [README](README.md). Afterwards the same command prints the whole fuse table.

### `programming` stops with `Path to programming_instructions after path correction couldn't be found`

Rebuild the images with an absolute build folder, or replace the relative paths in `tool_settings.json` with absolute ones. The Toolbox looks for relative paths under its own install folder. The image builder writes whatever build folder you give it.

## Serial output

### The board's serial port is silent, or prints one or two odd characters

Check which program is on the board first: TI's SDK examples print at 115200, and the Wi-Fi CSI presence program prints at **921600** (`BAUD` in `csi/csi_run.py`). Read at the wrong rate and the port looks silent or prints junk, so a board running the CSI image looks dead at 115200. Then open the port and set that baud in the same program that reads it, and read `/dev/cu.usbmodem<probe serial>1`. On macOS the baud rate is set by the program that opens the port, so `stty -f` in one command and `cat` in another reads at the wrong rate. The other port (`...4`) stays silent. After a reset the board prints its banner again; `xds110reset -s <probe serial>` resets it.

### A new image programmed after the Wi-Fi CSI presence program prints nothing, even TI's own examples

Erase the whole flash first, then program. The CSI program lays the flash out differently, and every image programmed over it stayed silent, TI's Matter light included. From the Toolbox folder:

```bash
./simplelink-wifi-toolbox programmer -i XDS110 -param1 <probe serial> full_flash_erase --flash_type AUTO_DETECT --activation_type sdk_example_key
```

It takes about 16 seconds. Then program as usual: after an erase the new image starts by itself when programming ends, with no replug.

### The Matter log prints fine at start-up, then turns into a stream of 0x00 and 0x80 bytes

Seen on the Matter thermostat image once Bluetooth starts; the same bytes come back at every host rate, so it is not the rate. Pairing and control from the phone still work. Read the start-up log in the first few seconds after programming, and judge the device by its Bluetooth advertising and the phone app after that. Cause not found.

### Apple Home finds the Matter device but hangs on "Connecting"

The commissioning window has closed: TI's Matter example keeps it open for 300 seconds after start-up. Erase and program again, then scan within 5 minutes. Pressing S1 or holding SW1 to reopen the window was not run on the thermostat image.

## Matter

### Does activating the chip lock the board to TI's example key?

Treat it as locked. The activation burns a key into the chip's fuses once and cannot be undone; this repo uses TI's example key (`sdk_example_key`), and every later erase and programming command names it, so the images programmed are signed with it. Whether the chip can switch to another key afterwards was not found out. Check the fuse state first (`get_fuse_data`, read-only) and activate only if it reports `Process is stopped. Life cycle OPERATIONAL is not valid.` Do not activate for connection failures or other errors. Source: `cc35x1/README.md`, steps 2 and 3.

### Which Apple hub does the Matter thermostat need, an Apple TV or a HomePod?

Apple Home does not control a Matter device without a hub. It was tested with an Apple TV. A HomePod is Apple's other hub; none was tried. Apple TV and iPhone models and software versions were not recorded.

### Is there a prebuilt Matter image, so you can skip the Docker build?

None is published. `cc35x1/matter/fetch_image.sh` only copies a finished build from this repo's build host to the Mac and rewrites its paths, so it is no download for you. Build with `build-thermostat.sh` (or `build.sh` for TI's light) on a Linux or Docker host. The warm build took 5 minutes 1 second; a cold one is an estimate of about 12 minutes. Source: [Matter README](matter/README.md).

### Apple Home says "Uncertified Accessory". Why, and is Add Anyway safe?

The warning is expected: the image uses Matter's test vendor ID `0xFFF1` and test credentials, so it is not a certified product. Add Anyway was tapped for boards built here, on a private network; what the warning guards against was not tested, so add only a board you built yourself. Source: `docs/experiments/2026-10-10-cc35x1-matter-thermostat.md`.

### How do you pair again after programming a new image?

The pairing is lost when the image is replaced, so add the board again. On TI's light, holding SW1 for 1 second wipes the board's old pairing and S1 restarts it; neither was tried on the thermostat image, where the board was erased and programmed again, then scanned within 5 minutes.

### Does the Matter thermostat work with Google Home or Alexa?

Unknown: only Apple Home was tried. Google's own page says a device with a test vendor ID needs a developer project (read 2026-10-10); that was not run. Source: `docs/experiments/2026-10-10-cc35x1-matter-thermostat.md`.

### Which Mac did the Matter steps run on? Apple Silicon or Windows?

An Intel Mac, macOS 15.7.9, with Toolbox 4.3.23. Apple Silicon and Windows were not run. The image builds on a separate Linux or Docker host, which was an Apple silicon Mac running OrbStack. Source: `cc35x1/README.md`.

### Why does `mac_paths.sh` say “signed component file not found”?

 The copied Matter `toolbox` folder is missing a file named in `tool_settings.json`. Copy the complete folder from the build’s `out/debug`, then rerun `cc35x1/matter/mac_paths.sh` from the repository root with that copied folder as its argument. The helper writes absolute paths only after every referenced file exists; it leaves any previous output unchanged on a validation error. It passed synthetic path tests and checks against two saved thermostat build folders. Those checks do not validate a fresh light build or program a board. See the [Matter light recipe](../docs/builds/matter-device.md).

## Wi-Fi

### Does the Network Terminal keep your Wi-Fi password on the board?

No. `wlan_connect -s "<name>" -t WPA2 -p "<password>"` joined the network without storing a profile: `wlan_get_profile -i 0` to `-i 7` answered `invalid index`. If you add profiles, `wlan_del_profile -i 255` deleted all of them (`wlan_delete_profile success`). Run it before leaving the board. The password still shows in your terminal log if the terminal echoes your command, so scrub captured logs before sharing them.

### `ERROR rxData_DataPacketDisptcher(): Received VLAN packet - drop it` keeps printing after the join

Ignore it. It appeared with a router that sends VLAN-tagged packets; the join, ping and iperf all worked in spite of it.

### The CSI presence example never prints `DAP streaming ready`, so a script waits forever

Wait for `DAP inferencing ready` instead. Plugin 1.20's firmware prints `[CSI] DAP inferencing ready -- console input disabled, connect Edge AI Studio now`; the README quotes different words. Then send the start-streaming frame (`ED 08 00 9E`); the board answers with the same command byte and sends one result every 2 s.

### A build that takes the network name from the Mac never joins: `Timeout expired connecting to Network`

Type the network name yourself. On macOS 15, `ipconfig getsummary en0` prints the placeholder `<redacted>` as the network name for a program without Location access, so the board looks for a network called `<redacted>`. The console then shows only `[WLAN EVENT] Unexpected event [0x19]` (event 25, `WLAN_EVENT_CONNECT_PERIODIC_SCAN_COMPLETE`: a scan found no match) and gives up 16.2 s after the station starts. The chained demo then falls back to Bluetooth provisioning and advertises 0.2 s later (5 of 5 runs, 2026-10-25, `ports/wifi-provisioning-ble/README.md`).

### `bleak` never finds the board, and never times out, on a Mac

Check the Mac first: `system_profiler SPBluetoothDataType | grep -E "State|Address"`. `State: Off` with `Address: NULL` means the Mac's Bluetooth controller is down; turn Bluetooth off and on in Control Center. Over SSH, macOS refuses Bluetooth with `Bluetooth is not authorized` until the app is allowed in Privacy & Security; run the client from a Terminal window instead (2026-10-25).

## Over-the-air update

### The OTA example's menu says `Invalid input` or `Invalid choice` right after a number

Send only a carriage return after each answer, not a carriage return and a line feed. The example reads the line feed as an empty answer to the next prompt.

### In `Show components in flash`, slot 2 shows `FAILED`. Can you still update?

Yes, after cleaning the slot: menu `3`, then the slot number (5 for the vendor image). It erases the slot, which takes about 20 seconds, and the state becomes `READY`. A board programmed through the Toolbox starts with its second slots in `FAILED`.

### After the update reboots, nothing prints on the serial port

The new image is running and has no serial output (TI's blink does not). Use an update image that prints, such as `uart2echo`. If you reset the board without accepting the trial, the bootloader goes back to the OTA app and prints `STAGED updates found after unexpected reboot`.

## Timing

### `DWT->CYCCNT` reads 0, so CoreMark prints `ERROR! Must execute for at least 10 secs`

Time the run with the FreeRTOS tick instead. With `DEMCR.TRCENA` set, the Cortex-M33 cycle counter still read 0 in a program loaded by the Toolbox. FreeRTOS's tick is 1 ms here, so an 11 second run is good to about 0.01 percent. This repo's port, `cc35x1/coremark/core_portme.c`, falls back to the tick when the counter reads 0. CoreMark's four checksums still match.

### The link stops with `uxTaskGetStackHighWaterMark` under `undefined ... first referenced` and `error: unresolved symbols remain`

Measure the stack by painting it instead. The FreeRTOS kernel the Wi-Fi SDK 10.20.00.39 links is prebuilt without `INCLUDE_uxTaskGetStackHighWaterMark`, so a thread cannot ask for its own high-water mark. Fill the stack below the thread's first frame with a known word before the work, then find the deepest word that changed; `cc35x1/bench/voicefe/run.c` does it in eight lines (2026-10-26).

### Signing fails with `Failed to execute script 'simplelink-wifi-toolbox'`, or the link step prints `Terminated: 15`

Free disk space and rerun that example. With about 500 MB free the Toolbox's signing step and the linker stopped with those messages and nothing about disk. Two of 11 builds in a batch hit it on 2026-10-24 and passed once space was back. Delete each example's build folder after programming it.

## Mistakes to avoid

The two entries tagged Our mistake above (relative paths, and the serial baud) cost about 20 minutes together in the run.

### How do I print my own message from this board with frl, and put the old program back?

Run `FRL_RESTORE_IMAGE=<toolbox/tool_settings.json of the image you want back> frl run --board cc35x1 --recipe print-message --pattern "Hello from my LaunchPad" --probe <serial>`, then `frl restore <the backup folder it prints>`. frl edits TI's uart2echo, builds and signs it, programs it with the Wi-Fi Toolbox and reads the message on the probe's serial port. This chip cannot be read back, so the backup keeps the fuse report and the running program's first line, and the restore programs the image you named and checks that line. If you see `FRL-E053`, build or pick that image first. The chip must be activated already: frl never activates it. 3 of 3 runs on 2026-10-25, probe <probe serial>; `docs/frl.md`.

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
