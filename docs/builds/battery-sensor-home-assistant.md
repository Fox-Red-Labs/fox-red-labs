# Show a low-power temperature sensor on your Home Assistant dashboard

A Zigbee temperature sensor that sleeps between polls shows up on your Home Assistant dashboard. Built on the LP-CC1354P10-1, on USB power, not a battery.

The page: [site/project-battery-sensor-home-assistant.html](../../site/project-battery-sensor-home-assistant.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Sleepy Zigbee temperature sensor in ZHA

The demonstrated build: one LaunchPad on USB power, a ZBT-2 radio, Home Assistant with ZHA.

Status: Works. Boards: cc1354p10.

Folders this version uses (in a sparse clone, fetch them first, then read `cc1354p10/AGENTS.md`):

```bash
git sparse-checkout add cc1354p10 mac-setup
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-CC1354P10-1 LaunchPad development kit | 1 | the board, with a CC1354P10 chip and 256 kB of RAM |
| Home Assistant Connect ZBT-2 | 1 | the Zigbee coordinator Home Assistant talks through; the only one tested |
| Zigbee smart plug (bought on Amazon; model not recorded) | 1 | a mains-powered Zigbee relay near the board. In the test every join went through one, because the ZBT-2 was out of the board's range |
| USB data cable (micro-USB to your Mac) | 1 | powers and programs the board; no battery in the recorded run |
| CR2032 coin cell and holder (optional) | optional | battery power: the board runs from 3.3 V, so a coin cell wired to 3V3 and GND with the USB power jumper removed is the usual option; battery life was not measured |

### Before you start

- A Mac with CCStudio 21.0.1, used headless. It brings SysConfig 1.28.1 and TI Arm Clang 4.0.4.LTS, which it substitutes for the 3.2.2 the project asks for. Also the SimpleLink CC13xx CC26xx SDK 8.33.00.16, the F2 SDK, with Z-Stack and TI-RTOS 7. Checked on an Intel MacBook with a Core i7-8750H on macOS 15.7.9 only. Install steps and disk space: the repository's `mac-setup/README.md`.
- A Zigbee network in Home Assistant, with the board in range of the coordinator or of a Zigbee router on that network. In the test the coordinator was out of range and every join went through a smart plug acting as a router.
- Your debug probe's serial number in `cc1354p10/CC1354P10.ccxml`, the connection file the load script uses (its `SEPK.POD_SERIAL` line).
- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`.

### Steps

1. **Mac, terminal.** Read this before you load anything. On this board the debugger connects only in the first seconds after a power-on boot, and once the sleepy image is loaded it cannot get back in. The way back that worked is the loop below (the same one as in step 5's fix line): start it first, then unplug and replug the USB cable. It needs the image from step 3, so build first. Replace `<repository folder>` with your clone. Keep that line, or a non-sleeping image and its backup, before you leave this image on a board you share.

   ```text
   cd <repository folder>
   for i in $(seq 1 60); do /Applications/ti/ccs2101/ccs/scripting/python/run.sh cc1354p10/tools/ccs_load_run.py cc1354p10/zigbee-sleepy-sensor/out/frl_sleepy_sensor.out && break; done
   ```

   You should see: You have the recovery line to hand before the board is running the sleepy image.

   A replug with no loop already running missed the window twice in the test. Nothing from software got the debugger back: not the serial menu's reset, not hundreds of connects, not holding the left button through a reset.

2. **Mac, terminal.** Install the tools once, all free: CCStudio 21.0.1 from [TI's CCStudio download page](https://www.ti.com/tool/download/CCSTUDIO/21.0.1) and the SimpleLink CC13xx CC26xx SDK 8.33.00.16 from [TI's SDK page](https://www.ti.com/tool/download/SIMPLELINK-CC13XX-CC26XX-SDK) (2.6 GB installed; the download size was not recorded). The paths in the scripts are where TI's installers put them: `/Applications/ti/ccs2101` and `/Applications/ti/simplelink_cc13xx_cc26xx_sdk_8_33_00_16`.

   You should see: Both folders are under `/Applications/ti`.

   If they live elsewhere, `build.sh` reads `SDK` and `CCS_CLI` from the environment, and `/Applications/ti/ccs2101/ccs/scripting/python/run.sh` in step 5 needs the matching path.

3. **Mac, terminal.** Build the sensor from the repository root. The script copies TI's `zed_temperaturesensor` example and the three app files it links. It applies `sleepy_sensor.patch`, which gives a real temperature reading, names Home Assistant can show, working attribute reads, joining without a button, and all 16 channels searched. It imports the project with CCStudio's headless command line and builds it.

   ```text
   cc1354p10/zigbee-sleepy-sensor/build.sh
   ```

   You should see: About 21 s later the script prints the FLASH, SRAM and CCFG lines of the linker map and `built:` followed by the path of `cc1354p10/zigbee-sleepy-sensor/out/frl_sleepy_sensor.out`. The image is 179.4 kB of flash and 45.5 kB of RAM.

   The build summary says "0 out of 1 projects have errors" even when SysConfig failed. So the script checks that the image exists and stops with "no image built" if not. One warning about `NVOCMP_NVPAGES` and a note that the headless import swapped the compiler to TI Arm Clang 4.0.4.LTS are normal. To list only your own network's channel instead of all 16, edit `zstack.rf.primaryChannels` in the patch before building; that avoids the board trying other networks first.

4. **Home Assistant web page.** Open joining first. In Home Assistant go to Settings, Devices & services, Zigbee Home Automation, Add device. Joining stays open for about 4 minutes. If your board is out of the coordinator's range, call the `zha.permit` service from Developer tools, Actions on the router it can reach: `duration: 254` and that router's `ieee` address.

   You should see: Home Assistant is searching for Zigbee devices.

   If the board is out of the coordinator's range and joining is open on the coordinator alone, the board finds no network in 3 minutes. Open joining on the router and keep the board near it for the first join.

5. **Mac, terminal.** Load the image and start it. This board needs a CCStudio scripting run to start a freshly loaded program; a plain UniFlash flash does not start it until you unplug and replug the USB cable. `run.sh` is CCStudio's own scripting Python.

   ```text
   /Applications/ti/ccs2101/ccs/scripting/python/run.sh cc1354p10/tools/ccs_load_run.py cc1354p10/zigbee-sleepy-sensor/out/frl_sleepy_sensor.out
   ```

   You should see: After about 15 s the script prints `loaded and running` and the path of the image.

   If the debugger will not connect, this is the recovery loop; it tries up to 60 loads and stops at the first one that gets in. Start it, then unplug and replug the USB cable. It got in on the 21st attempt, about 10 s after the replug, in the seconds the chip stays awake at boot. Put the image you want back on the board in place of the sleepy sensor's.

6. **Mac, terminal.** Watch the board's own screen at 115200 baud. `ls /dev/cu.usbmodem*` lists two ports for the probe; the board's serial screen is the one ending in 1. Replace `<probe serial>` with yours, brackets included. To find yours, run `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e` if you have CCStudio (it prints `Serial Num:`), else `system_profiler SPUSBDataType | grep -A3 XDS110`.

   ```text
   screen /dev/cu.usbmodem<probe serial>1 115200
   ```

   You should see: `[State] In the network` on the "ZDO Info" line and `[PAN ID] 0x....` with your network's channel on the "NWK Info" line. On the clean run that took 6.0 s from boot.

   To log it with timestamps and decode it afterwards, use `cui_log.py` (`capture`, then `decode`) in the same folder. If the board joined another open network first, list only your channel as in step 3. The board tries the first network it finds, and each failed try costs 3 s. Four of six association tries on this bench lost the network key. It arrived after the 3 s that TI's stack waits for it, and why it arrived late was not found.

7. **Home Assistant web page.** Prove the join from Home Assistant's device list rather than the board's screen. Give it a few minutes. Home Assistant asks the sleeping board a set of questions, and about half of its first requests get no answer until the board's next poll.

   You should see: About 162 s after boot a device named "Fox Red Labs Temp Sensor" appears. It is an end device with power source "Battery or Unknown", one temperature entity and an identify button. The reading was 22 C on the serial line and Home Assistant showed 71.6 F because Home Assistant is set to Fahrenheit.

   While Home Assistant is still setting it up, it may list the device as `unk_manufacturer unk_model` with no value. That is the setup still running, and on TI's unpatched example it never finishes: every attribute read fails with status 0xC1. To start over, remove the device in Home Assistant and wait until it is gone. Use the serial menu's RESET TO FN, which resets to factory new, and load the image again. The board stays silent after that reset until it is loaded again.

8. **Home Assistant web page.** Let Home Assistant ask for the value. The reporting interval is set by Home Assistant and not by the board. It set a minimum interval of 30 s, a maximum of 900 s and a reporting change of 0.50 C.

   You should see: At a steady temperature the first report arrived 899.9 s after Home Assistant set up reporting. Reads you ask for from Home Assistant came back in 1.6 s (median), because the board polls its parent every 3 s.

   A reading that looks stale for up to 15 minutes at a steady temperature is the reporting setting. It is not a fault. If you re-pair after removing the device, Home Assistant keeps the old entity id `sensor.unk_manufacturer_unk_model` while the device is named "Fox Red Labs Temp Sensor". Rename the entity if it matters.

### Expected result

A device named "Fox Red Labs Temp Sensor" shows up in Home Assistant a few minutes after the board boots, with a temperature that reads about 22 C in the test room. It keeps reporting while it sleeps between polls.

### What this shows

- TI's temperature-sensor example, patched, joins a Home Assistant (ZHA) network through a ZBT-2 as a sleepy end device: its node descriptor says the receiver is off between polls.
- Home Assistant can read its temperature on demand: 20 of 20 reads returned 22.00 C, median 1.6 s, as a 3 s poll predicts.

### Limits

- Battery life or sleep current. No battery was used and there was no meter on the bench. The serial menu may also keep the chip out of its deepest sleep; a build without it was not made.
- That the temperature tracks the room. Only 22 C was seen; the chip was not warmed or cooled and no thermometer sat beside it.
- That the join is reliable. The clean 6.0 s join is one run; 4 of 6 earlier association tries lost the network key.
- Anything after the first hour: packet loss, range, and the board's signal strength to its parent were not measured.
- One clean join; three runs are needed before quoting a median.
- After the debugger lock-out in step 1 the board needs the recovery loop to be reprogrammed.
- On the final build, Home Assistant's whole Zigbee integration went down before the next report was due. Its radio disappeared from the hub's USB, for a reason outside the run. A report on the final build was not seen. The board stayed in the network.
- Checked on an Intel Mac only.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Image size | 179.4 kB of flash and 45.5 kB of RAM of 256 kB of SRAM; TI's example as shipped is 183,230 and 46,449 (the patch adds 508 and 152). A further 10 kB of flash are reserved for the stack's non-volatile storage | `data/results/cc1354p10.csv` |
| Toolchain | SimpleLink CC13xx CC26xx SDK 8.33.00.16 (Z-Stack, TI-RTOS 7), CCStudio 21.0.1 headless with SysConfig 1.28.1 and TI Arm Clang 4.0.4.LTS, -Oz -g | `cc1354p10/zigbee-sleepy-sensor/README.md` |
| Build and load time | 21 s build, 15 s load, 6.0 s to join, 162 s until Home Assistant showed the entity | `cc1354p10/zigbee-sleepy-sensor/README.md` |
| Read latency from Home Assistant | 20 of 20 reads; median 1,574 ms, spread 171 to 2,799 ms; the board polls every 3,000 ms |  |
| Reporting set by Home Assistant | minimum 30 s, maximum 900 s, change 0.50 C; first report 899.9 s after setup at a steady 22 C |  |
