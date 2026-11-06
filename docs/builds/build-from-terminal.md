# Build and flash a TI example from one terminal

One make line builds TI's example and one flash line puts it on the board. There is no window to click, so you can run it from a terminal.

The page: [site/project-build-from-terminal.html](../../site/project-build-from-terminal.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## MSPM0 LaunchPad: make and OpenOCD

The simplest route: the board's own USB cable, one build line, one flash line.

Status: Works. Boards: l1306.

Folders this version uses (in a sparse clone, fetch them first, then read `mspm0l1306/AGENTS.md`):

```bash
git sparse-checkout add mspm0l1306 mspm0g3507/mcu-bench/sdk-common tools/frl
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM0L1306 LaunchPad development kit | 1 | the board |
| Micro-USB data cable | 1 | power and programming |

### Before you start

- A Mac with CCStudio 21.0.1 (1.9 GB download, 8.1 GB installed), the MSPM0 SDK 2.11.00.07 (0.4 GB, 2.2 GB installed), SysConfig 1.26.3 (0.96 GB installed) and xPack OpenOCD 0.12.0-7 (2 MB). Links are in step 1. Checked on an Intel MacBook on macOS 15.7.9 only.
- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`. It holds the two helper scripts. About 12 GB free for the tools.

### Steps

1. **Mac, terminal.** Install the tools once, all free: CCStudio 21.0.1 from [TI's CCStudio download page](https://www.ti.com/tool/download/CCSTUDIO) (1.9 GB download, 8.1 GB installed, free myTI login) (it brings TI Arm Clang 5.1.1), the MSPM0 SDK 2.11.00.07 from [TI's MSPM0 SDK download page](https://www.ti.com/tool/download/MSPM0-SDK) (0.4 GB download, 2.2 GB installed), SysConfig 1.26.3 ([TI's SysConfig page](https://www.ti.com/tool/download/SYSCONFIG), 0.96 GB installed), and xPack OpenOCD 0.12.0-7 from [its GitHub release page](https://github.com/xpack-dev-tools/openocd-xpack/releases/tag/v0.12.0-7) (2 MB; the file `xpack-openocd-0.12.0-7-darwin-x64.tar.gz` on an Intel Mac, `xpack-openocd-0.12.0-7-darwin-arm64.tar.gz` on Apple Silicon). Then clone this repository into `<repository folder>` for the helper scripts `sdkmake.sh` and `tools/ocd.sh`. SysConfig 1.26.3, the version this repo's scripts point at, came with the SimpleLink Wi-Fi SDK installer on the test Mac (it put it in `/Applications/ti/sysconfig_1.26.3`); CCStudio 21.0.1 carries its own 1.28.1, which this repo's scripts do not point at.

   You should see: `/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin/tiarmclang --version` prints 5.1.1.

   If your tools live somewhere other than `/Applications/ti`, edit the paths at the top of `mspm0g3507/mcu-bench/sdk-common/sdkmake.sh`.

2. **Bench.** Plug the board into the Mac with a micro-USB data cable. It shows up as an XDS110 probe with two serial ports; there is no driver to install. Read your probe's serial number from the port names.

   ```text
   ls /dev/cu.usbmodem*
   ```

   You should see: Two ports named `/dev/cu.usbmodem<serial>1` and `...4`. The part between `usbmodem` and the last digit is your probe's serial number.

   If nothing appears, try another cable; a charge-only cable looks the same.

3. **Terminal.** From the `mspm0l1306` folder of this repository, copy one of TI's examples out of the SDK and build it with one make line. Any example folder works the same way; the blink is the one that was timed.

   ```text
   cd <repository folder>/mspm0l1306
   cp -R /Applications/ti/mspm0_sdk_2_11_00_07/examples/nortos/LP_MSPM0L1306/driverlib/gpio_toggle_output .
   ../mspm0g3507/mcu-bench/sdk-common/sdkmake.sh gpio_toggle_output/ticlang
   ```

   You should see: A build with no errors that leaves `gpio_toggle_output/ticlang/gpio_toggle_output.out`. It took 3.3 s for the blink. 167 of the board's 170 SDK examples build this way.

   If the build cannot find SysConfig or the compiler, the paths in `sdkmake.sh` do not match your install (step 1).

4. **Terminal.** Flash it with OpenOCD. `tools/ocd.sh` picks the probe by serial number and refuses to run if it is missing; set `PROBE` to your serial first (`<probe serial>` below stands for it). To find yours, run `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e` if you have CCStudio (it prints `Serial Num:`), else `system_profiler SPUSBDataType | grep -A3 XDS110`. Run it from the same folder as the build step.

   ```text
   export PROBE=<probe serial>
   tools/ocd.sh "reset halt" "program gpio_toggle_output/ticlang/gpio_toggle_output.out verify" "reset run"
   ```

   You should see: OpenOCD ends with `** Verified OK **` in about 0.6 s, and the RGB LED cycles.

   OpenOCD prints `Error: checksum mismatch - attempting binary compare` first; that is normal, and the next line is the real result.

### Expected result

One build line and one flash line take TI's example from the SDK to a running program on the board in under 5 seconds of machine time. No window, nothing to click.

### What this shows

- TI's example builds with one make line and flashes with one OpenOCD line on the LP-MSPM0L1306: 3.3 s and 0.6 s for the blink, and the RGB LED cycled.
- 167 of 170 of the board's SDK examples build from the terminal, and all 148 that do not write the boot configuration were flashed and run by script without losing the debugger.

### Limits

- That an AI assistant can type these lines. The route was timed with a script and by hand, not with an assistant.
- That the 3 examples that do not build are the chip's fault: their causes were not all traced.
- That every example does something you can see: many need extra wiring to show they work.
- The helper scripts hard-code the Mac paths in step 1.
- `tools/ocd.sh` refuses to run until `PROBE` is set to your probe's serial number.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Blink build and flash | build 3.3 s, OpenOCD program and verify 0.6 s (UniFlash's command line takes 15.0 s for the same image) | `mspm0l1306/README.md` |
| Examples that build | 167 of 170 SDK examples build from the terminal |  |
| Tools | MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, xPack OpenOCD 0.12.0-7, Intel Mac on macOS 15.7.9 | `mspm0l1306/README.md` |

## SimpleLink LaunchPad: make and UniFlash

The LP-EM-CC2340R5 route: a clip-on probe, a connection file, one make line, one flash line.

Status: Works. Boards: cc2340r5.

Folders this version uses (in a sparse clone, fetch them first, then read `cc2340r5/AGENTS.md`):

```bash
git sparse-checkout add cc2340r5
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-EM-CC2340R5 LaunchPad development kit | 1 | the board |
| LP-XDS110ET debugger | 1 | the debug probe |

### Before you start

- A Mac with the SimpleLink Low Power F3 SDK 9.21.00.36 LTS, CCStudio 21.0.1 (TI Arm Clang 5.1.1), SysConfig 1.26.3 and UniFlash 9.6.0. Checked on an Intel MacBook on macOS 15.7.9 only.
- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`. It holds the example copy and the connection file. The probe's serial number from step 2.

### Steps

1. **Mac, terminal.** Install the tools once, all free: the SimpleLink Low Power F3 SDK 9.21.00.36 LTS from [TI's SDK page](https://www.ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK) (the macOS installer, 536 MB), CCStudio 21.0.1 from [TI's CCStudio download page](https://www.ti.com/tool/download/CCSTUDIO) (1.9 GB download, 8.1 GB installed, free myTI login) (it brings TI Arm Clang 5.1.1), SysConfig 1.26.3 and UniFlash 9.6.0 from [TI's UniFlash page](https://www.ti.com/tool/download/UNIFLASH) (0.3 GB download, 1.9 GB installed). Clone this repository into `<repository folder>` for the example copy in `cc2340r5/src`. SysConfig 1.26.3, the version this repo's scripts point at, came with the SimpleLink Wi-Fi SDK installer (10.20.00.39, which you do not need otherwise; you can get SysConfig alone from [TI's SysConfig page](https://www.ti.com/tool/download/SYSCONFIG), 0.96 GB installed) on the test Mac (it put it in `/Applications/ti/sysconfig_1.26.3`); CCStudio 21.0.1 carries its own 1.28.1, which this repo's scripts do not point at.

   You should see: `/Applications/ti/uniflash_9.6.0/dslite.sh --help` prints its options, and the folder `/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS` exists.

   If your tools live elsewhere, pass your own three paths on the make line in step 4.

2. **Bench.** Clip the LP-XDS110ET probe onto the LP-EM-CC2340R5 and plug the probe's USB-C cable into the Mac. The board has no USB port and no debugger of its own. Read the probe's serial number.

   ```text
   /Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e
   ```

   You should see: `xdsdfu` lists the probe with its serial number.

   If a flash later fails with `The target failed to see a correctly formatted SWD header` (error -615), unplug the cable, pull the probe off the board, press it back on and plug in again.

3. **Terminal.** Make the connection file with CCStudio's command line rather than by hand: a hand-written file left SWD off and gave misleading errors. Replace `<any empty folder>` with a new empty folder. It takes about four seconds. The file lands in `targetConfigs/CC2340R5.ccxml` inside the imported project; put your probe's serial number in it. The repository's `cc2340r5/CC2340R5.ccxml` is the file used here.

   ```text
   /Applications/ti/ccs2101/ccs/ccs-server.app/Contents/MacOS/ccs-server-cli.sh -noSplash -workspace <any empty folder> -application com.ti.ccs.apps.importProject -ccs.location /Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS/examples/rtos/LP_EM_CC2340R5/drivers/empty/freertos/ticlang/empty_LP_EM_CC2340R5_freertos_ticlang.projectspec
   ```

   You should see: A `targetConfigs/CC2340R5.ccxml` file in the imported project.

   Do not trust `dslite.sh -p` as a connection test: it printed the flash operations without talking to the chip while every real connection failed.

4. **Terminal.** Build TI's `drivers/empty` blink. A copy is in `cc2340r5/src`. The three paths are where TI's installers put the tools; the SDK's own build settings point at folders that do not exist.

   ```text
   cd cc2340r5/src/freertos/ticlang
   make SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR=/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS \
        SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh \
        TICLANG_ARMCOMPILER=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
   ```

   You should see: `empty.out` and `empty.hex` appear. The build took 22 s.

   If make stops on a folder that does not exist or a placeholder user name, the SDK's `imports.mak` is being read; pass all three paths on the make line as above.

5. **Terminal.** Flash it with UniFlash's command line, from the `cc2340r5` folder. `-u` runs the program afterwards and `-e` prints progress (it is not erase). Do not add `-v`: verify reports one mismatch in the boot configuration area and ends with `a data verification error occurred, file load failed`, although the program was written and ran.

   ```text
   /Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c CC2340R5.ccxml -e -f -u src/freertos/ticlang/empty.hex
   ```

   You should see: The flash took 7 s and the red LED (DIO14) is on for one second and off for one second. The host confirmed it by eye.

   If you see error -615 or `Unable to access the DAP`, check the connection file has SWD on (step 3) and the probe is seated (step 2).

### Expected result

A build of about 22 seconds and a flash of about 7 seconds put TI's blink on the board, and the red LED toggles once a second, from a terminal and with no window.

### What this shows

- TI's blink for the LP-EM-CC2340R5 builds with one make line and flashes with one UniFlash line from a Mac terminal, and the host saw the red LED blink.
- The gotchas are known and written down: probe seating, the connection file, the SDK's build paths and the verify mismatch.

### Limits

- A typical first-time duration: the 9 minutes included 8 of probe trouble, and the board was used, not sealed.
- That verify works: with `-v` UniFlash reports a mismatch in the boot configuration area on every image flashed, cause unknown.
- That an AI assistant can type these lines unaided; one in VS Code did them in a recorded run.
- The probe is a second board to buy; the LP-EM-CC2340R5 cannot be powered or flashed without one.
- UniFlash's command line needs a connection file that no TI command-line tool writes; step 3 uses CCStudio to make it.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Build | 22 s, TI Arm Clang 5.1.1.LTS, SimpleLink Low Power F3 SDK 9.21.00.36 LTS, SysConfig 1.26.3 | `cc2340r5/README.md` |
| Flash | 7 s with UniFlash 9.6.0 and the XDS110 probe; verify (-v) reports one mismatch at 0x4E020750 in the CCFG user record | `cc2340r5/README.md` |
| Whole run | about 3 min hands-on and 9 min machine time, board in hand to blinking LED, 8 of them a probe connection problem |  |

## C2000 LaunchPad: scripted build and load

The LAUNCHXL-F2800137 route: C2000Ware, a build script and CCStudio's scripting runner, no window.

Status: Works. Boards: f2800137.

Folders this version uses (in a sparse clone, fetch them first, then read `c2000-launchpad/AGENTS.md`):

```bash
git sparse-checkout add c2000-launchpad
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LAUNCHXL-F2800137 LaunchPad development kit | 1 | the board |

### Before you start

- A Mac with CCStudio 21.0.1 (C2000 compiler 25.11.1.LTS, SysConfig 1.28.1, scripting) and C2000Ware 26.02.00.00 LTS core. Checked on an Intel MacBook on macOS 15.7.9 only.
- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`. It holds the scripts in `c2000-launchpad/tools`.

### Steps

1. **Mac, terminal.** Install CCStudio 21.0.1 with every device family (about 10 GB; it brings the C2000 compiler and SysConfig). Then get only the parts of C2000Ware this chip needs from TI's public GitHub repository (no myTI login). The two commands below give the same files as a full checkout. The scripts in this repository's `c2000-launchpad/tools` expect CCStudio in `/Applications/ti/ccs2101` and C2000Ware in `/Applications/ti/c2000ware_core_26_02_00_00`.

   ```text
   git clone --filter=blob:none --depth 1 --sparse --branch REL_C2000Ware_v26.02.00.00.LTS https://github.com/TexasInstruments/c2000ware-core-sdk.git /Applications/ti/c2000ware_core_26_02_00_00
   git -C /Applications/ti/c2000ware_core_26_02_00_00 sparse-checkout set .metadata boards device_support/f280013x driverlib/.meta driverlib/f280013x libraries utilities/clb_tool/clb_syscfg utilities/cmd_tool utilities/dcsm_tool/dcsm_syscfg utilities/transfer
   ```

   You should see: `ls /Applications/ti/c2000ware_core_26_02_00_00/driverlib/f280013x/examples/led` lists the blink example. C2000Ware takes 427 MB on disk including its git folder.

   If your tools live elsewhere, edit the paths at the top of each script in `c2000-launchpad/tools`.

2. **Bench.** Plug the board into the Mac and check the onboard probe's firmware. If it is older than 3.0.0.43, update it first with the two `xdsdfu` commands in the board's FAQ. Then put your probe's serial number in place of the serial in the `.ccxml` copy named with <probe serial>; that file sets the 2-pin cJTAG mode this board needs.

   ```text
   cd /Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110
   ./xdsdfu -e
   ```

   You should see: `xdsdfu` lists the probe with its firmware version and serial number. The test probe arrived on 3.0.0.19 and TI's tools would not connect until it was on 3.0.0.43.

   If UniFlash or CCStudio prints `A firmware update is required for the XDS110 probe` and times out after 45 seconds, update the probe firmware. If the connection fails with error -2131, `Unable to access device register`, the connection file lacks the cJTAG setting (SWD Mode Settings = 4).

3. **Terminal.** From the `c2000-launchpad` folder, copy TI's blink example and build it for RAM. `c2kbuild.sh` runs SysConfig, compiles with TI's projectspec flags and links.

   ```text
   cp -R /Applications/ti/c2000ware_core_26_02_00_00/driverlib/f280013x/examples/led /tmp/led_ex1_blinky
   tools/c2kbuild.sh /tmp/led_ex1_blinky RAM
   ```

   You should see: A build that leaves `/tmp/led_ex1_blinky/build-RAM-Ooff/led_ex1_blinky.out`. It took 4.6 s.

   If the build cannot find C2000Ware or the compiler, the paths at the top of `tools/c2kbuild.sh` do not match your install.

4. **Terminal.** Load it and watch the LED pin (GPIO20, sampled 8 times 250 ms apart) with CCStudio's scripting runner. Separate UniFlash command-line calls each reconnect, so they cannot watch a running program.

   ```text
   /Applications/ti/ccs2101/ccs/scripting/run.sh tools/c2krun.js /tmp/led_ex1_blinky/build-RAM-Ooff/led_ex1_blinky.out blink
   ```

   You should see: The load took 3.9 s and the pin was seen toggling (observed 2026-10-06; the output was not saved). This is a RAM load: the program is gone at power-off.

   The first connection can print `Configuring Debugger (may take a few minutes on first launch)`; it took 1.8 s on the board tried, but give the first connection a long timeout. To keep the program, build with `FLASH` in place of `RAM` and run the same command on `build-FLASH-Ooff/led_ex1_blinky.out`; the blink example was not timed from flash.

### Expected result

A build of under 5 seconds and a RAM load of under 5 seconds, then the LED pin toggling, all from a terminal. A flash image needs the `FLASH` build and was not timed for the blink.

### What this shows

- TI's blink example for the LAUNCHXL-F2800137 builds from a Mac terminal in 4.6 s and runs from RAM in 3.9 s, with the LED pin seen toggling.
- 138 of 139 C2000Ware examples for the chip build by script with TI's own flags plus three additions the projects do not list.

### Limits

- A flash write of the blink: the timed load was to RAM. The flash route is described but the blink was not timed from flash.
- That a person watched the LED: the pin was sampled by the script.
- Anything about Apple Silicon or CCStudio's window.
- The probe may need a firmware update before TI's tools connect.
- 4-pin JTAG does not work on the board as shipped; the connection file must use 2-pin cJTAG.
- The route needs CCStudio's scripting runner because UniFlash's command-line calls each reconnect.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Blink | 4.6 s build and 3.9 s RAM load (observed 2026-10-06, not saved); the sweep recorded a 4.1 s load | `c2000-launchpad/README.md` |
| Examples that build | 138 of 139 C2000Ware examples for this chip, by script | `c2000-launchpad/README.md` |
| Tools | CCStudio 21.0.1, C2000 compiler 25.11.1.LTS, SysConfig 1.28.1, UniFlash 9.6.0, C2000Ware 26.02.00.00 LTS core | `c2000-launchpad/README.md` |
