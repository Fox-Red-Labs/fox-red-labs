# Switch a board's LED from Home Assistant over Zigbee

Flash TI's on/off light example on an LP-EM-CC2340R5 and switch its LED from Home Assistant. It worked on 2026-10-05; a repeat on 2026-10-16 did not join.

The page: [site/project-zigbee-light.html](../../site/project-zigbee-light.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Zigbee on/off light, switched from Home Assistant

The demonstrated build: one LaunchPad, its debug probe, a ZBT-2 radio, Home Assistant with ZHA.

Status: Could work. Boards: cc2340r5.

Folders this version uses (in a sparse clone, fetch them first, then read `cc2340r5/AGENTS.md`):

```bash
git sparse-checkout add cc2340r5 mac-setup
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-EM-CC2340R5 LaunchPad development kit | 1 | the board, with a CC2340R52 chip and 36 kB of RAM. Its red LED is the light |
| LP-XDS110ET debugger | 1 | the debug probe: powers, flashes and carries the serial port |
| Home Assistant Connect ZBT-2 | 1 | the Zigbee coordinator Home Assistant talks through; the only one tested |
| USB-C data cable | 1 | from the probe to your Mac |
| A computer or small server that runs Home Assistant with ZHA | 1 | the controller; free software |
| Zigbee smart plug (bought on Amazon; model not recorded) | 1 | optional, not part of the recorded run: a real lamp to switch from the same Home Assistant. |

### Before you start

- Nothing is wired: the probe clips onto the board's debug header, so no jumper wires are needed.
- A Mac with the SimpleLink Low Power F3 SDK 9.21.00.36 LTS, TI Arm Clang 5.1.1, which comes inside CCStudio 21.0.1, SysConfig 1.26.3 and UniFlash 9.6.0. Built and flashed on an Intel MacBook on macOS 15.7.9; nothing else was tried.
- Home Assistant with ZHA already running on a ZBT-2. The Zigbee network in this run was on channel 20.
- Your debug probe's serial number. Step 2 shows where to read it.
- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`. The build folder `cc2340r5/zigbee/onoff_light` is TI's example with the two-line change already made.

### Steps

1. **Mac, terminal.** Install the tools once, all free. Get the macOS installer for the SimpleLink Low Power F3 SDK 9.21.00.36 LTS from [TI's SDK page](https://www.ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK) (0.55 GB download, 2.0 GB installed). Get CCStudio 21.0.1 from [TI's CCStudio download page](https://www.ti.com/tool/download/CCSTUDIO/21.0.1) for TI Arm Clang 5.1.1. Get SysConfig 1.26.3 from [TI's SysConfig page](https://www.ti.com/tool/download/SYSCONFIG) (0.96 GB installed). Get UniFlash 9.6.0 from [TI's UniFlash page](https://www.ti.com/tool/download/UNIFLASH) (0.3 GB download, 1.9 GB installed). Install steps, times and disk space are in the repository's `mac-setup/README.md`. This repo's scripts point at SysConfig 1.26.3. It came with the SimpleLink Wi-Fi SDK installer on the test Mac, which put it in `/Applications/ti/sysconfig_1.26.3`. CCStudio 21.0.1 carries its own 1.28.1. This repo's scripts do not point at it.

   You should see: The SDK folder `simplelink_lowpower_f3_sdk_9_21_00_36_LTS` and the folders `sysconfig_1.26.3` and `uniflash_9.6.0` are under `/Applications/ti`.

   If your tools live somewhere else, the three paths on the `make` line in step 4 and the `dslite.sh` path in step 6 are the only places to change.

2. **Bench, then terminal.** Clip the probe onto the board, plug the probe's USB-C cable into the Mac, and read the probe's serial number. Put that serial number into `cc2340r5/CC2340R5.ccxml`, the connection file the flash command uses; it names the probe and the chip with SWD switched on. Do not write that file by hand. If you need a new one, let CCStudio write it; the import command is in `cc2340r5/README.md`, gotcha 2.

   ```text
   /Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e
   ```

   You should see: The probe's serial number is printed.

   If a later flash says `The target failed to see a correctly formatted SWD header` (error -615), pull the probe off the board and press it back on, then plug the cable in again. That cleared it on this bench.

3. **Mac, text editor.** Open `cc2340r5/zigbee/onoff_light/on_off_light.syscfg` and check that these two lines are set. They are the whole change to TI's example. The first makes the board a router that joins an existing network. TI ships a coordinator that forms its own network. The second makes it search all 16 channels. TI ships channel 11 only, so a network on any other channel is never found. You can list just your own network's channel instead; Home Assistant shows it in the Zigbee integration's network settings.

   ```text
   zigbee.deviceType = ["zr"];
   zigbee.primaryChannels = [11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26];
   ```

   You should see: The file contains `zigbee.deviceType = ["zr"];` and `zigbee.primaryChannels = [11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26];` at lines 109 and 110 in this repository's copy.

   If you start from TI's own copy instead of this repository's copy, set both lines or the board never appears in Home Assistant and nothing says why.

4. **Mac, terminal.** Build the example. Run the first two commands from `cc2340r5/`. The three paths are where TI's installers put the tools on this Mac; change them if yours differ.

   ```text
   cd zigbee/onoff_light/freertos/ticlang
   make -j4 SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR=/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS \
        SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh \
        TICLANG_ARMCOMPILER=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
   cd ../../../..
   ```

   You should see: About 30 s of building, then `onoff_light.hex` in that folder.

   If the make step cannot find a tool, check the three paths. The SDK's own `imports.mak` points at folders that do not exist, which is why the paths are on the command line.

5. **Home Assistant web page.** Open joining before you flash. In Home Assistant go to Settings, Devices, Add device (ZHA). Or call the `zha.permit` service with a duration of 180.

   You should see: Home Assistant is searching for Zigbee devices. In the tries the board joined within 37 to 58 s of booting, so flash straight after this.

   If joining closes before the board arrives, open it again.

6. **Mac, terminal.** Flash it, from `cc2340r5/`. The `-e` in this command prints progress; it does not erase.

   ```text
   /Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c CC2340R5.ccxml -e -f -u zigbee/onoff_light/freertos/ticlang/onoff_light.hex
   ```

   You should see: UniFlash prints `Loading Program: zigbee/onoff_light/freertos/ticlang/onoff_light.hex`, then `Setting PC to entry point.` and `Success`, about 14 s.

   If the connection fails, see the SWD line in step 2. If you are starting over on a board that joined before, remove the device in Home Assistant first. Wait until it is gone from the device list. Re-pairing early left a stale record and the error `Failed to send request: device did not respond`.

7. **Home Assistant web page.** Watch the Zigbee device list. Prove the join from here rather than from the board's log.

   You should see: A new device named `unk_manufacturer unk_model` appears 37 s after the flash finished, shown as a switch rather than a light. Its address starts `00:12:4b`, TI's prefix.

   The example leaves the manufacturer and model names empty, so Home Assistant fills in those words. It is harmless. If nothing appears in about a minute, the channel is the usual cause: recheck step 3.

8. **Home Assistant web page, then bench.** Turn the new switch on and off from its device page.

   You should see: The board's LED follows the switch. Over 20 switch commands from a script on the Mac, all 20 landed. Home Assistant showed the new state after a median of 95 ms, with a minimum of 73 and a maximum of 292.

   Latency is one run on one desk. About two hours later, after a re-pairing, the same image answered in 3.5 to 7.0 s, median about 5 s, while the coordinator reported a weaker link. The cause was not found. Measure more than once before you quote a number.

### Expected result

On 2026-10-05 the board appeared in Home Assistant about 40 s after the flash as a switch, and the LED followed it. A repeat on 2026-10-16 did not join, so this result is not reproducible today.

### What this shows

- TI's Zigbee on/off light example joins a Home Assistant (ZHA) network through a ZBT-2 coordinator on a 36 kB-RAM LaunchPad, with two lines changed in a text file.
- Commands from Home Assistant reach the board and change the LED: 20 of 20 in one run, median 95 ms until Home Assistant showed the new state.

### Limits

- That a lamp or any other real light was switched. The only light in any run is the board's own LED. A Zigbee plug with a lamp, switched from the same Home Assistant, is not part of the recorded run.
- Range, current draw, packet loss over hours, or a sleeping battery device. The board is a router here and stays awake.
- That other coordinators or Zigbee2MQTT work; only a ZBT-2 with ZHA was tried.
- That the 95 ms is typical. It is one run of 20; the same board answered in about 5 s after a re-pairing two hours later, cause unknown.
- Not reproducible today: the 2026-10-16 repeat did not join Home Assistant (cause open). Open joining in Home Assistant first and retry if the board does not join.
- RAM is nearly full: 799 bytes free with logging on. Turn `loggingEnabled` off in SysConfig before adding application code.
- The serial log is TI's binary format. The decoder `tilogger` fails on a Mac as shipped (`unpack requires a buffer of 8 bytes`); the one-line fix is in the board FAQ.
- Checked on an Intel Mac only.
- The over-the-air update from Home Assistant stalled at about 1% in four tries; cause unknown.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Image size | 275.7 kB of flash; 35.2 kB of RAM used, 799 bytes free (logging on), from the linker map of the same source rebuilt later that evening | `cc2340r5/zigbee/README.md` |
| Toolchain | SimpleLink Low Power F3 SDK 9.21.00.36 LTS, TI Arm Clang 5.1.1, SysConfig 1.26.3, UniFlash 9.6.0 (DSLite 21.0.0.4106) |  |
| Build and flash time | 30 s build (31 s after the channel fix), 14 to 15 s flash, 37 s from the flash finishing to the device listed in ZHA | `cc2340r5/zigbee/README.md` |
| Switch latency | 20 of 20 landed; Home Assistant service call median 85 ms; state shown after a median of 95 ms, min 73, max 292 |  |
| Link at first pairing | LQI 60, RSSI -85 dBm as the coordinator reported it (weak) | `cc2340r5/zigbee/README.md` |
| Same board after a re-pairing, two hours later | 30 of 30 landed, median about 5 s (3.5 to 7.0 s); cause unknown, suspected radio environment |  |
