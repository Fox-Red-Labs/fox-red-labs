# A Bluetooth sensor your phone can find

Build and flash TI's data_stream example on the LP-EM-CC2340R5, see it advertise from a Mac, then connect and time a 20-byte echo. No sensor value is sent.

The page: [site/project-bluetooth-sensor.html](../../site/project-bluetooth-sensor.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## TI's data_stream example seen and echoed by a Mac, on the LP-EM-CC2340R5

The board advertises as Data Stream. A Python script on the Mac connects to it and times an echo. It carries no sensor reading.

Status: Works. Boards: cc2340r5.

Folders this version uses (in a sparse clone, fetch them first, then read `cc2340r5/AGENTS.md`):

```bash
git sparse-checkout add cc2340r5 cc2755p10/ble tools/ai_runs
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-EM-CC2340R5 LaunchPad development kit | 1 | the board |
| LP-XDS110ET debugger | 1 | debug probe |
| A Mac with Bluetooth | 1 | the phone stand-in that scans and connects |

### Before you start

- TI's SimpleLink Low Power F3 SDK 9.21.00.36, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3 and UniFlash 9.6.0 (DSLite), all free from TI with a myTI login. Checked on an Intel MacBook, macOS 15.7.9.
- Python 3 with the `bleak` Bluetooth library: `python3 -m pip install bleak`.
- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git`, then `git sparse-checkout add cc2340r5 cc2755p10 tools`. Below, `<repository folder>` is that clone.

### Steps

1. **Bench.** Clip the LP-EM-CC2340R5 onto the LP-XDS110ET probe and plug the probe into the Mac with a USB data cable.

   You should see: The Mac lists the probe, and two serial ports appear.

   If nothing appears, try another cable; a charge-only cable looks the same.

2. **Terminal (Mac).** Copy TI's data_stream example out of the SDK into a working folder and build it with TI Arm Clang. Run it in the example's `freertos/ticlang` folder.

   ```text
   cp -R /Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS/examples/rtos/LP_EM_CC2340R5/ble/data_stream ~/data_stream
   cd ~/data_stream/freertos/ticlang
   make -j4 SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR=/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS \
        SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh \
        TICLANG_ARMCOMPILER=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
   ```

   You should see: The build ends without an error and leaves `data_stream.hex` in that folder.

   If make cannot find the SDK, SysConfig or the compiler, change the three paths to where yours are installed.

3. **Terminal (Mac).** Flash the image with DSLite, using the board's CC2340R5.ccxml from the `cc2340r5` folder of the repository. Replace `<home>` with your home folder. The write erases the whole chip first.

   ```text
   cd <repository folder>/cc2340r5
   /Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c CC2340R5.ccxml -e -f -u <home>/data_stream/freertos/ticlang/data_stream.hex
   ```

   You should see: DSLite prints that the flash finished. In the recorded run it connected and wrote on the first try.

   If it cannot connect, check that the probe is seated on the board and unplug and replug it. Do not use a mass erase to fix a connection error.

4. **Terminal (Mac).** Scan for ten seconds from the Mac.

   ```text
   python3 tools/ai_runs/blescan.py 10
   ```

   You should see: A line `Data Stream` with a signal strength in dBm. At 1 to 2 m the recorded runs read about -42 dBm.

   If the name is missing, press the board's reset button, run the scan again and keep the Mac close. The Mac shows only some of the advertisements, so a short scan can miss it.

5. **Terminal (Mac).** Connect to it and time the echo. The script writes 20, 64 and 128 bytes, 50 times each, and waits for the board to send the text back with its letter case flipped.

   ```text
   python3 cc2755p10/ble/ds_echo.py
   ```

   You should see: `connected + services` in about 0.55 s, then a line per size. The recorded run on this board: 45 ms median for 20 bytes, 105 ms for 64, 135 ms for 128.

   If it prints `found None`, the scan did not see the board: repeat step 4 first. If bytes come back wrong, the script says so on the size's line.

### Expected result

The Mac sees a device called Data Stream, connects to it in about half a second, and gets 20 bytes echoed back in about 45 ms. The board sends back what the Mac wrote; it does not read a sensor.

### What this shows

- TI's own Bluetooth example builds from a terminal, flashes, and is found and connected to by a Mac with no pairing.
- A round trip over Bluetooth takes 45 ms for 20 bytes on this board (one run of 50).

### Limits

- A sensor reading in the advertisement or the connection. No sensor was read over Bluetooth on any board in this repository for this project.
- A phone app: only a Mac script was timed.
- That the advertisement reaches a phone. No phone run is recorded for this example.
- One board, one Mac and one run each, so the timings are single figures.
- The 45 ms echo is probably paced by the connection interval macOS picks, not by the radio. The interval was not read.
- The 1.40 s from flash to first sight was measured with TI's basic_ble example, not data_stream.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Software | SimpleLink Low Power F3 SDK 9.21.00.36, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, DSLite 9.6.0.5764 |  |
| Connect | 0.55 s to connect and list services, ATT MTU 65 |  |
| Echo | 45.0 / 105.0 / 135.1 ms median round trip at 20 / 64 / 128 bytes, goodput 3.5 / 4.8 / 7.4 kbit/s |  |
| First seen after flash | 1.40 s at 100 ms interval, 0.56 s at 20 ms (basic_ble) |  |
