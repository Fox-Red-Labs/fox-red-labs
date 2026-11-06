# Control a CC35X1 Matter light from Apple Home

Build the lighting example, flash the CC35X1, add it to Apple Home. Phone tile and board buttons switch the red LED.

The page: [site/project-matter-device.html](../../site/project-matter-device.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## TI's Matter light in Apple Home

TI's lighting example, unmodified. The red LED becomes a light tile in Home.

Status: Works. Boards: cc35x1.

Folders this version uses (in a sparse clone, fetch them first, then read `cc35x1/AGENTS.md`):

```bash
git sparse-checkout add cc35x1
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-EM-CC35X1 LaunchPad development kit | 1 | the board |
| LP-XDS110ET debugger | 1 | the debugger that programs the board |
| iPhone with the Home app | 1 | the Matter controller you pair with |
| Apple TV (the Home hub) | 1 | the hub the phone controls the accessory through |
| A Mac to program from | 1 | runs the Toolbox and programs the board |
| Linux or Docker host | 1 | builds the image |

### Before you start

- A Mac with SimpleLink Wi-Fi Toolbox 4.3.23 (free from TI), tested on an Intel Mac, macOS 15.7.9. The Matter build needs Ubuntu 22.04, so add a Linux or Docker host (a Mac with OrbStack and 6 GB free worked). Clone with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch this version's folders. Below, `<repository folder>` is that clone.
- Your probe serial number: the eight characters `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e` prints (needs CCStudio 21.0.1).
- One step cannot be undone. Activation burns TI's example key into the chip's fuses, and every image is signed with it from then on. Step 2 checks first.
- A Home hub and an iPhone with the Home app. Bluetooth on, phone on the hub's Wi-Fi.

### Steps

1. **Bench.** Set the debugger's TGT VDD jumper to EXT and push the board's 20-pin edge into the debugger. Plug both USB-C cables into the Mac. Leave J4 and J6 on the lower two pins.

   You should see: The Mac lists the probe and `xdsdfu -e` prints its serial number.

   Use both cables. TI says the debugger can brown out during Matter transmissions otherwise.

2. **Terminal (Mac).** In `/Applications/ti/simplelink_wifi_toolbox_macos_4_3_23`, run the read-only activation check. If it prints the fuse table, go on. Only if it stops with `Process is stopped. Life cycle OPERATIONAL is not valid.`, activate by hand as under “If something goes wrong.”

   ```text
   ./simplelink-wifi-toolbox programmer -i XDS110 -param1 <probe serial> get_fuse_data --flash_type IS25WJ064F --activation_type sdk_example_key --verbose
   ```

   You should see: The check prints the fuse table instead of the error. Activation took about 5 seconds.

   Activation cannot be undone. If the fuse table prints, the chip is already activated: do nothing. Only for the exact lifecycle error, type this by hand, never paste it: `./simplelink-wifi-toolbox programmer -i XDS110 -param1 <probe serial> custom_activation --flash_type IS25WJ064F --activation_type sdk_example_key --authentication_enabled yes`. The Copy button copies only the check, on purpose.

3. **Terminal, Linux or Docker host.** Build from the repository's `cc35x1/matter` folder. The script downloads four tools, checks each against a published SHA-256, fetches TI's repository at the pinned commit and leaves images in `/path/to/work/out/debug`. Copy `toolbox` to the Mac.

   ```text
   ./build.sh /path/to/work
   ```

   You should see: A signed 1,758.3 kB image (`toolbox/primary_vendor_image.sign.bin`, 1.7 MB). The clean build took 673 seconds (11 minutes 13 seconds) under emulation on an Apple silicon Mac.

   Needs 6 GB free. Afterward run `docker volume rm frl-matter-build`. If the SysConfig 1.28.0 installer is gone, the script uses 1.28.1 build 4785, which built this image.

4. **Terminal (Mac).** From the repository root, set TB to the copied toolbox folder and run `cc35x1/matter/mac_paths.sh` to swap Linux paths for local ones. Then change to the Toolbox folder and program the image.

   ```text
   TB="/absolute/path/to/copied/toolbox"
   cc35x1/matter/mac_paths.sh "$TB"
   cd /Applications/ti/simplelink_wifi_toolbox_macos_4_3_23
   ./simplelink-wifi-toolbox programmer -i XDS110 -param1 <probe serial> programming --tool_settings "$TB/tool_settings.mac.json"
   ```

   You should see: The Toolbox reports success after about a minute.

   If another TI image was on the board (above all Wi-Fi CSI presence) and the serial port stays silent, save what you need, erase, and program again: `./simplelink-wifi-toolbox programmer -i XDS110 -param1 <probe serial> full_flash_erase --flash_type AUTO_DETECT --activation_type sdk_example_key`. This wipes the old image and settings. Skip it on a clean board. `mac_paths.sh` has run only on saved thermostat build folders and synthetic paths, not a fresh light build. If it reports a missing file, recopy `toolbox` and rerun.

5. **Bench.** Press S1 (reset) to open a pairing window. The Toolbox does not reset the board. If it was paired before, hold SW1 for 1 second first to wipe the pairing.

   You should see: The board advertises over Bluetooth. At 115200 baud the log shows the start. One Wi-Fi disconnect line per boot is TI's known issue MATTER-456.

   Scan within 5 minutes. The thermostat image, same TI code, closed at 300 seconds. Matter's default is 15 minutes, untimed on the light. The blinking green LED only means Identify.

6. **Phone (Home app).** In the Home app tap +, then Add Accessory, and scan `cc35x1/matter/setup-qr.png`. If the scan fails, tap More options, My Accessory Isn't Shown Here, Enter Code, and type the code `cc35x1/matter/payload.py` prints.

   You should see: The phone finds the accessory and warns Uncertified Accessory.

   Stay within a few meters, Bluetooth on. Not sure it is advertising? Press S1 again.

7. **Phone (Home app).** Tap Add Anyway, wait about a minute, pick a room and name, tap Done.

   You should see: A progress screen, then room and name. In the board's log from the Bluetooth connect: secure channel at 3 s, Wi-Fi at 16 s, commissioning done at 63 s.

   If pairing stalls, press S1 and restart from step 6.

8. **Phone and bench.** Tap the light tile. Then press SW1 (on) and SW2 (off) on the board.

   You should see: The tile shows On with a yellow bulb and the red LED follows. Buttons change the tile within about a second.

   It is called "Matter Accessory" until you rename it.

### Expected result

After about a minute Apple Home adds a light. The tile toggles the red LED and the buttons change the tile. Commissioning took 63 seconds from the Bluetooth connect, per the device log.

### What this shows

- TI's lighting example builds from source in a Linux container and runs on the CC3551E, 1,758.3 kB signed.
- An iPhone commissions it over Bluetooth and Wi-Fi. The tile toggles the LED and the buttons update the tile. One run, 2026-10-09.

### Limits

- Google Home, Alexa and Home Assistant have no recorded run.
- 63 seconds is one run. Toggle latency is untimed.
- Setup after a reflash is unmeasured. A new image loses the pairing.
- The Matter test vendor ID `0xFFF1` makes Apple Home show Uncertified Accessory.
- TI supports Ubuntu 22.04 only. This build ran in a linux/amd64 container under emulation, hence 11 minutes.
- Commissioning from a Mac with the matter.js shell stopped at the Bluetooth link: macOS refused a UUID. Phones are fine.
- Matter over Thread is not for this chip: the CC3551E has no 802.15.4 radio.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Software | TI Matter release `matter-v1.5-ti-1.2-ea.2`, commit `33a45a1ba707418ee9f764723a5b98f2718e8abd`, example `examples/lighting-app/ti/cc35xx` | `cc35x1/matter/README.md` |
| Toolchain | SimpleLink Wi-Fi SDK 10.20.00.39, Toolbox 4.3.23, SysConfig 1.28.1, Arm GNU 12.3.rel1, CMake 3.21.3 | `cc35x1/matter/README.md` |
| Flash and RAM | 1,752.1 kB of code and constants plus initialized data; 578.9 kB of initialized plus zero-initialized data (heap and stacks not counted) | `cc35x1/matter/README.md` |
| Build record | 673 s clean: packages 74, tools 20, source 9, submodules 118, bootstrap 175, Wi-Fi SDK libraries 69, lighting app 208 | `cc35x1/matter/README.md` |
| Commissioning | 63 s from the Bluetooth connect to commissioning completed (secure channel 3 s, Wi-Fi joined 16 s) |  |
| Core and clock | Arm Cortex-M33 at 160 MHz on the CC3551E | `cc35x1/README.md` |

## A Matter thermostat in Apple Home

A thermostat built from TI's lighting example on the CC35X1, paired into Apple Home.

Status: Could work. Boards: cc35x1.

Folders this version uses (in a sparse clone, fetch them first, then read `cc35x1/AGENTS.md`):

```bash
git sparse-checkout add cc35x1
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-EM-CC35X1 LaunchPad development kit | 1 | the board |
| LP-XDS110ET debugger | 1 | the debugger that programs the board |
| iPhone with the Home app | 1 | the Matter controller you pair with |
| Apple TV (the Home hub) | 1 | the hub the phone controls the accessory through |
| A Mac to program from | 1 | runs the Toolbox and programs the board |
| Linux or Docker host | 1 | builds the image |

### Before you start

- A Mac with SimpleLink Wi-Fi Toolbox 4.3.23 (free from TI), tested on an Intel Mac, macOS 15.7.9. The Matter build needs Ubuntu 22.04, so add a Linux or Docker host (a Mac with OrbStack and 6 GB free worked). Clone with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch this version's folders. Below, `<repository folder>` is that clone.
- Your probe serial number: the eight characters `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e` prints (needs CCStudio 21.0.1).
- One step cannot be undone. Activation burns TI's example key into the chip's fuses, and every image is signed with it from then on. Step 2 checks first.
- A Home hub and an iPhone with the Home app. Bluetooth on, phone on the hub's Wi-Fi.

### Steps

1. **Bench.** Set the debugger's TGT VDD jumper to EXT and push the board's 20-pin edge into the debugger. Plug both USB-C cables into the Mac. Leave J4 and J6 on the lower two pins.

   You should see: The Mac lists the probe and `xdsdfu -e` prints its serial number.

   Use both cables. TI says the debugger can brown out during Matter transmissions otherwise.

2. **Terminal (Mac).** In `/Applications/ti/simplelink_wifi_toolbox_macos_4_3_23`, run the read-only check. Only if it stops with `Process is stopped. Life cycle OPERATIONAL is not valid.`, run the second command once. One at a time.

   ```text
   ./simplelink-wifi-toolbox programmer -i XDS110 -param1 <probe serial> get_fuse_data --flash_type IS25WJ064F --activation_type sdk_example_key --verbose
   ```

   You should see: The check prints the fuse table instead of the error. Activation took about 5 seconds.

   Activation cannot be undone. If the fuse table prints, stop. Only for the exact lifecycle error, type this, do not paste: `./simplelink-wifi-toolbox programmer -i XDS110 -param1 <probe serial> custom_activation --flash_type IS25WJ064F --activation_type sdk_example_key --authentication_enabled yes`. Copy copies the check only, on purpose.

3. **Terminal, Linux or Docker host.** Build from the repository's `cc35x1/matter` folder. It is TI's `matter-v1.5-ti-1.2-ea.2` tag, commit `33a45a1ba707418ee9f764723a5b98f2718e8abd`, with a thermostat data model and application swapped into the lighting example. Copy the `toolbox` folder from `/path/to/work/out/debug` to the Mac.

   ```text
   ./build-thermostat.sh /path/to/work
   ```

   You should see: A signed 1,747.9 kB image (`toolbox/primary_vendor_image.sign.bin`). A warm container took 5 minutes 1 second. Cold was never timed in one piece; about 12 minutes is an estimate.

   Needs 6 GB free in a Docker volume (`frl-matter-build`). Afterward run `docker volume rm frl-matter-build`. TI builds Matter on Ubuntu 22.04 only.

4. **Terminal (Mac).** From the Toolbox folder, erase the flash, then program the image. First run `cc35x1/matter/mac_paths.sh` from the repository: the build leaves Linux paths in `tool_settings.json` and the Toolbox needs absolute Mac ones. One command at a time.

   ```text
   TB=<absolute path of the copied toolbox folder>
   cc35x1/matter/mac_paths.sh "$TB"
   cd /Applications/ti/simplelink_wifi_toolbox_macos_4_3_23
   ./simplelink-wifi-toolbox programmer -i XDS110 -param1 <probe serial> full_flash_erase --flash_type AUTO_DETECT --activation_type sdk_example_key
   ./simplelink-wifi-toolbox programmer -i XDS110 -param1 <probe serial> programming --tool_settings "$TB/tool_settings.mac.json"
   ```

   You should see: Erase takes about 16 seconds, programming about 60. On an erased flash the image starts by itself.

   Always erase first if another image was on the board, above all Wi-Fi CSI presence. Without it, every later image stayed silent, TI's light included. `mac_paths.sh` has run only on a sample file. If the Toolbox reports a path error, make every path in `tool_settings.json` absolute by hand.

5. **Terminal (Mac).** Optional. Read the board's serial port, `/dev/cu.usbmodem<probe serial>1`, at 115200 baud (8N1). The command below, from the repository, sets the baud and prints the log. Ctrl+C stops it.

   ```text
   python3 cc35x1/matter/logread.py /dev/cu.usbmodem<probe serial>1
   ```

   You should see: Within 2 seconds: a boot line, Wi-Fi start, the Matter server and `THERMO ready: UART2_1 115200, endpoint 1 thermostat, endpoint 2 occupancy`. Bluetooth advertising starts about 5 seconds after the boot line.

   After Bluetooth starts the log fills with 0x00 and 0x80 bytes. Cause unknown. Pairing and control still work. A silent port usually means wrong baud. `logread.py` has run only against a virtual port; any serial program set to 115200 8N1 works.

6. **Phone (Home app).** The window is 300 seconds. In the Home app tap +, then Add Accessory, and scan `cc35x1/matter/setup-qr.png`. If you cannot scan, choose More options and type the code `cc35x1/matter/payload.py` prints. At Uncertified Accessory tap Add Anyway, then pick a room and name.

   You should see: The accessory appears as a thermostat named "Matter Accessory".

   If the scan finds it and hangs on "Connecting", the 300-second window has closed. Erase and program again (step 4), then scan within 5 minutes.

7. **Phone (Home app).** Open the thermostat tile, set Heat and move the heating setpoint to 67 F.

   You should see: Heat at 67 F. Current temperature reads 69 F: the board's fixed 20.8 C fallback, since no sensor is wired.

   With no sensor line, SW1 and SW2 step the board's setpoint by 0.5 C.

### Expected result

Within the window Apple Home adds a thermostat and the heating setpoint changes from the phone. The idle board reports about 150 kB of heap free. Pairing time is unmeasured here; the light took 63 s.

### What this shows

- TI ships no thermostat for the CC3551E. This one runs, Apple Home adds it and it takes a heating setpoint change. One iPhone, 2026-10-11.
- Swapping the data model and one application file in the lighting example was enough. It paired on the first scan.
- The image is 10.4 kB smaller than TI's light: 1,747.9 kB against 1,758.3 kB signed.

### Limits

- Google Home, Alexa and Home Assistant have no recorded run. Their result rows are waiting.
- The occupancy endpoint is not driven: no presence line is wired.
- Temperature is a fixed 20.8 C fallback, not a sensor. No driver path to the chip's own temperature turned up.
- Setpoint latency, local control offline, four controllers at once and setup after a reflash are unmeasured.
- The board's log of the write was not recorded. The evidence is the phone screen.
- Programming over the Wi-Fi CSI presence image left the board silent until a full erase. The Toolbox guide does not mention it.
- The Matter test vendor ID `0xFFF1` makes Apple Home show Uncertified Accessory. The device is not certified.
- The 300-second window means erase, program and pair in one sitting.
- Unknown. After Bluetooth starts the log UART turns to 0x00 and 0x80 bytes at every rate tried, 1,200 to 115,200 baud.
- Matter over Thread is not for this chip: the CC3551E has no 802.15.4 radio.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Software | TI Matter release `matter-v1.5-ti-1.2-ea.2`, commit `33a45a1ba707418ee9f764723a5b98f2718e8abd`, plus the thermostat example from this repository (Thermostat on endpoint 1, Occupancy Sensing on endpoint 2) | `cc35x1/matter/README.md` |
| Toolchain | SimpleLink Wi-Fi SDK 10.20.00.39, Toolbox 4.3.23, SysConfig 1.28.1 (TI pins 1.28.0; the Linux installer for it was no longer served), Arm GNU 12.3.rel1, CMake 3.21.3, in a linux/amd64 container | `cc35x1/matter/README.md` |
| Flash | 1,741.6 kB of code and constants plus initialized data; signed image 1,747.9 kB in the 8 MB flash chip (the light on the same tag: 1,752.1 kB and 1,758.3 kB) | `cc35x1/matter/README.md` |
| RAM | 579.3 kB of initialized plus zero-initialized data from the ELF, heap and stacks not counted (the light: 578.9 kB). Idle heap before pairing: 149.7 kB free, lowest ever 146.8 kB | `docs/experiments/2026-10-10-cc35x1-matter-thermostat.md` |
| Core and clock | Arm Cortex-M33 at 160 MHz on the CC3551E; Wi-Fi 6 and Bluetooth LE 5.4 | `cc35x1/README.md` |
| Commissioning window | 300 s from start-up (TI's code unchanged) | `docs/experiments/2026-10-10-cc35x1-matter-thermostat.md` |
| Build time | App step 165 s on a warm volume (last try); whole script 301 s warm; cold container to the app step 500 s | `cc35x1/matter/README.md` |
