# Blink an LED, then change the pattern in plain English

Plug in a LaunchPad, build TI's blink example, flash it and watch the LED. Then change the pattern: ask for it in plain English, or edit one number in a terminal.

The page: [site/project-blink.html](../../site/project-blink.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## From a terminal, no AI step, on the LP-MSPM0L1306

One command backs the board up, copies TI's example, builds it, flashes it and checks the LED.

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

- A Mac with the free tools from step 1: CCStudio 21.0.1 (1.9 GB download, 8.1 GB installed, free myTI login; it brings TI Arm Clang 5.1.1), the MSPM0 SDK 2.11.00.07 (0.4 GB, 2.2 GB installed), SysConfig 1.26.3 (0.96 GB installed) and xPack OpenOCD 0.12.0-7 (2 MB). Checked on an Intel MacBook on macOS 15.7.9 only.
- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`. It holds the helper scripts `sdkmake.sh` and `tools/ocd.sh`.

### Steps

1. **Mac, terminal.** Install the tools once, all free: CCStudio 21.0.1 from [TI's CCStudio download page](https://www.ti.com/tool/download/CCSTUDIO) (1.9 GB download, 8.1 GB installed, free myTI login) (it brings TI Arm Clang 5.1.1), the MSPM0 SDK 2.11.00.07 from [TI's MSPM0 SDK download page](https://www.ti.com/tool/download/MSPM0-SDK) (0.4 GB download, 2.2 GB installed), and xPack OpenOCD 0.12.0-7 from [its GitHub release page](https://github.com/xpack-dev-tools/openocd-xpack/releases/tag/v0.12.0-7) (2 MB; the file `xpack-openocd-0.12.0-7-darwin-x64.tar.gz` on an Intel Mac, `xpack-openocd-0.12.0-7-darwin-arm64.tar.gz` on Apple Silicon). The helper script `sdkmake.sh` in this repository expects the SDK at `/Applications/ti/mspm0_sdk_2_11_00_07`, SysConfig 1.26.3 at `/Applications/ti/sysconfig_1.26.3` and the compiler under `/Applications/ti/ccs2101`. SysConfig 1.26.3, the version this repo's scripts point at, came with the SimpleLink Wi-Fi SDK installer (10.20.00.39, which you do not need otherwise; you can get SysConfig alone from [TI's SysConfig page](https://www.ti.com/tool/download/SYSCONFIG), 0.96 GB installed) on the test Mac (it put it in `/Applications/ti/sysconfig_1.26.3`); CCStudio 21.0.1 carries its own 1.28.1, which this repo's scripts do not point at.

   You should see: `/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin/tiarmclang --version` prints 5.1.1, and `openocd --version` prints 0.12.0.

   If your tools live elsewhere, set `FRL_MSPM0_SDK`, `FRL_SYSCONFIG` and `FRL_TICLANG` to where they are; `sdkmake.sh` reads them through `tools/frl/toolenv.py`.

2. **Bench.** Plug the board into the Mac with a micro-USB data cable. The board shows up as an XDS110 probe with two serial ports; there is no driver to install. Read your probe's serial number with `ls /dev/cu.usbmodem*` (the text after `usbmodem`, minus the last digit), or with `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e` from CCStudio.

   You should see: The Mac lists the probe and two serial ports. A new board shows TI's out-of-box demo.

   If nothing appears, try another cable; a charge-only cable looks the same.

3. **Terminal.** From the root of this repository, run the whole route with your probe's serial number. It backs the board up, copies TI's blink example, builds it, flashes it and times the LED pin through the debugger. It took 13 to 14 s on this bench with the tools installed. Leave off `--probe` only if one probe is plugged in; with several, it stops with FRL-E002.

   ```text
   ./start.sh mspm0l1306 blink --probe <serial>
   ```

   You should see: The run ends with a line that starts `OK:` and the RGB LED on the board cycles.

   If it stops with FRL-E002, several probes are plugged in: name yours with `--probe`. `python3 tools/frl/frl.py detect` lists them.

4. **Terminal.** Or do it by hand, from the `mspm0l1306` folder of this repository. Copy TI's example out of the SDK, build it (3.3 s), then flash it with OpenOCD (0.6 s). The repository's `tools/ocd.sh` picks the probe by serial number and refuses to run if it is missing.

   ```text
   cd <repository folder>/mspm0l1306
   cp -R /Applications/ti/mspm0_sdk_2_11_00_07/examples/nortos/LP_MSPM0L1306/driverlib/gpio_toggle_output .
   ../mspm0g3507/mcu-bench/sdk-common/sdkmake.sh gpio_toggle_output/ticlang
   PROBE=<serial> tools/ocd.sh "reset halt" "program gpio_toggle_output/ticlang/gpio_toggle_output.out verify" "reset run"
   ```

   You should see: OpenOCD ends with `** Verified OK **`, and the RGB LED cycles.

   OpenOCD prints `Error: checksum mismatch - attempting binary compare` first; that is normal, and the next line is the real result.

5. **Terminal.** Ask for a different pattern: run the same command with `--pattern` and four times in milliseconds (on, off, on, pause).

   ```text
   ./start.sh mspm0l1306 blink --probe <serial> --pattern 150,150,150,900
   ```

   You should see: The run ends with `OK:` and says LED2 red (PA26) repeats the pattern you asked for.

   If the check fails, read the line it prints; the check decides the result, not the LED you see.

### Expected result

The board's RGB LED blinks after a flash that takes about a second, and the one-command run ends with an OK line. Whole run 13 to 14 s with the tools installed.

### What this shows

- TI's gpio_toggle_output example builds and flashes on the LP-MSPM0L1306 from one command, with no CCStudio window and no AI step.
- The check reads the LED pin through the debugger and passes the pattern you asked for.

### Limits

- That an AI assistant can drive this route: no AI step was timed on this board for this route.
- A person's first-hour time: the 13 to 14 s is a scripted run with the tools already installed.
- The helper scripts hard-code the Mac paths in step 1.
- Checked on an Intel Mac on macOS 15.7 only.
- Nothing here ran through CCStudio on this board.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Whole route | 13 to 14 s from the command to a checked pattern, tools installed | `docs/frl.md` |
| Build and flash by hand | build 3.3 s, OpenOCD flash and verify 0.6 s; UniFlash's command line also works in 15.0 s | `mspm0l1306/README.md` |
| Tools | MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, xPack OpenOCD 0.12.0-7, Intel Mac on macOS 15.7 | `mspm0l1306/README.md` |

## From a terminal, no AI step, on the LP-MSPM0C1104

Copy TI's example, build it with make, flash it with OpenOCD. No window to click.

Status: Works. Boards: c1104.

Folders this version uses (in a sparse clone, fetch them first, then read `mspm0c1104/AGENTS.md`):

```bash
git sparse-checkout add mspm0c1104 mspm0g3507/mcu-bench/sdk-common tools/frl
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM0C1104 LaunchPad development kit | 1 | the board |
| Micro-USB data cable | 1 | power and programming |

### Before you start

- A Mac with the free tools from step 1: CCStudio 21.0.1 (1.9 GB download, 8.1 GB installed, free myTI login; it brings TI Arm Clang 5.1.1), the MSPM0 SDK 2.11.00.07 (0.4 GB, 2.2 GB installed), SysConfig 1.26.3 (0.96 GB installed) and xPack OpenOCD 0.12.0-7 (2 MB). Checked on an Intel MacBook on macOS 15.7.9 only.
- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`. It holds the helper scripts `sdkmake.sh` and `tools/ocd.sh`.

### Two ways to do this

**Build it yourself.** Install the tools, build the image from source and flash it, step by step. The steps follow.

**Or ask an AI assistant.** Paste this prompt:

```text
Run git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git, then read fox-red-labs/START.md and follow it to build docs/builds/blink.md on my {kit}. Ask me before installing anything or writing to the board.
```

### Steps

1. **Mac, terminal.** Install the tools once, all free: CCStudio 21.0.1 from [TI's CCStudio download page](https://www.ti.com/tool/download/CCSTUDIO) (1.9 GB download, 8.1 GB installed, free myTI login) (it brings TI Arm Clang 5.1.1), the MSPM0 SDK 2.11.00.07 from [TI's MSPM0 SDK download page](https://www.ti.com/tool/download/MSPM0-SDK) (0.4 GB download, 2.2 GB installed), and xPack OpenOCD 0.12.0-7 from [its GitHub release page](https://github.com/xpack-dev-tools/openocd-xpack/releases/tag/v0.12.0-7) (2 MB; the file `xpack-openocd-0.12.0-7-darwin-x64.tar.gz` on an Intel Mac, `xpack-openocd-0.12.0-7-darwin-arm64.tar.gz` on Apple Silicon). The helper script `sdkmake.sh` in this repository expects the SDK at `/Applications/ti/mspm0_sdk_2_11_00_07`, SysConfig 1.26.3 at `/Applications/ti/sysconfig_1.26.3` and the compiler under `/Applications/ti/ccs2101`. SysConfig 1.26.3, the version this repo's scripts point at, came with the SimpleLink Wi-Fi SDK installer (10.20.00.39, which you do not need otherwise; you can get SysConfig alone from [TI's SysConfig page](https://www.ti.com/tool/download/SYSCONFIG), 0.96 GB installed) on the test Mac (it put it in `/Applications/ti/sysconfig_1.26.3`); CCStudio 21.0.1 carries its own 1.28.1, which this repo's scripts do not point at.

   You should see: `/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin/tiarmclang --version` prints 5.1.1, and `openocd --version` prints 0.12.0.

   If your tools live elsewhere, set `FRL_MSPM0_SDK`, `FRL_SYSCONFIG` and `FRL_TICLANG` to where they are; `sdkmake.sh` reads them through `tools/frl/toolenv.py`.

2. **Bench.** Plug the board into the Mac with a micro-USB data cable. The board shows up as an XDS110 probe with two serial ports; there is no driver to install. Read your probe's serial number with `ls /dev/cu.usbmodem*` (the text after `usbmodem`, minus the last digit), or with `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e` from CCStudio.

   You should see: The Mac lists the probe and two serial ports. The LP-MSPM0C1104 shows its red LED on or blinking before you flash anything.

   If nothing appears, try another cable; a charge-only cable looks the same.

3. **Terminal.** Copy TI's example out of the SDK and build it, from the `mspm0c1104` folder of this repository. The build took 3.5 s.

   ```text
   cd <repository folder>/mspm0c1104
   cp -R /Applications/ti/mspm0_sdk_2_11_00_07/examples/nortos/LP_MSPM0C1104/driverlib/gpio_toggle_output .
   ../mspm0g3507/mcu-bench/sdk-common/sdkmake.sh gpio_toggle_output/ticlang
   ```

   You should see: A build with no errors that leaves `gpio_toggle_output/ticlang/gpio_toggle_output.out`.

   On an LP-MSPM0C1104 board older than version E3 the LED does not blink: the red LED moved from PA4 to PA22, and TI's example toggles PA22. Check the version printed on the board (and that jumper J7 is on).

4. **Terminal.** Flash it with OpenOCD. Replace `<serial>` with your probe's (0.64 s at a 2.5 MHz adapter speed).

   ```text
   openocd -f interface/xds110.cfg -c "adapter serial <serial>" -f target/ti_mspm0.cfg -c init -c "reset halt" -c "program gpio_toggle_output/ticlang/gpio_toggle_output.out verify" -c "reset run" -c shutdown
   ```

   You should see: OpenOCD ends with `** Verified OK **`, and the red LED blinks.

   OpenOCD prints `Error: checksum mismatch - attempting binary compare` first; that is normal, and the next line is the real result.

5. **Terminal.** Change the delay in `gpio_toggle_output.c`, then rebuild and flash again with the two commands above. The run changed the delay constant from 12,000,000 to 3,000,000.

   You should see: The blink rate changes. On the C1104 the proof was reading the LED pin: the toggle period went from 496 ms to 124 ms. A person did not watch it.

   If the rate does not change, check the build finished with no errors before flashing; the old image keeps running.

### Expected result

The board's red LED blinks the shipped pattern after a flash that takes under a second, then blinks at the rate you set after you change the delay and flash again. Build under 5 s, flash under 2 s.

### What this shows

- TI's gpio_toggle_output example builds with one make wrapper and flashes with OpenOCD on the LP-MSPM0C1104, with no CCStudio window and no AI step.
- On the C1104 a changed delay changed the pin's toggle period from 496 ms to 124 ms, read from the pin.

### Limits

- That an AI assistant can drive this route: no AI step was timed on either board.
- A person's first-hour time: the C1104 number is a scripted run's, with tools installed and hands-on time zero.
- That the C1104's change was seen by a person; it was read from the pin.
- The helper scripts hard-code the Mac paths in step 1.
- Nothing here ran through CCStudio on these two boards.

### Numbers and sources

| | Value | Source |
|---|---|---|
| C1104 build and flash | builds 3.1, 3.5 and 4.3 s; OpenOCD flash 0.58 to 0.67 s at a 2.5 MHz adapter speed, 1.3 s at the default 100 kHz; first command to proof 77 s |  |
| Tools | MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, xPack OpenOCD 0.12.0-7, Intel Mac on macOS 15.7.9 | `mspm0l1306/README.md` |

## CCStudio and Claude Code, on the LP-MSPM0G3507

One board, one USB cable, and Claude Code in CCStudio to build and flash TI's example.

Status: Works. Boards: g3507.

Folders this version uses (in a sparse clone, fetch them first, then read `mspm0g3507/AGENTS.md`):

```bash
git sparse-checkout add mspm0g3507 portability/toolchains
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM0G3507 LaunchPad development kit | 1 | the board |
| Micro-USB data cable, plus a USB-C adapter if your Mac has only USB-C ports | 1 | power and programming |

### Before you start

- A Mac with at least 12 GB free. Checked on an Intel MacBook on macOS 15.7.9.
- Free TI downloads with a free myTI login: CCStudio 21.0.1 (1.9 GB download, 8.1 GB installed) and the MSPM0 SDK 2.11.00.07 (0.4 GB download, 2.2 GB installed), both the current versions on ti.com on 2026-10-08. The links are in steps 2 and 3.
- Claude Code with a Claude subscription, for steps 6, 8 and 9. The blink itself needs no AI; TI's example blinks as shipped.

### Two ways to do this

**Build it yourself.** Install the tools, build the image from source and flash it, step by step. The steps follow.

**Compiler.** Where a TI MSPM0 SDK example ships a gcc makefile, GCC builds it as well as TI Arm Clang does. All 224 no-RTOS DriverLib examples that ship one built unchanged for the LP-MSPM0G3507 and LP-MSPM0L1306, and 87 of 87 ran on the L1306 with no fault. GCC 12.2 from the Zephyr SDK ran through three small wrapper scripts named `arm-none-eabi-*`; TI names Arm GNU 9-2019-q4. GCC code was a median 15 percent larger. Copy the SDK's example folder somewhere first, go into its `gcc` folder, and put the folder of a GCC install (the one that holds `bin/arm-none-eabi-gcc`) in place of `<GCC folder>`. Details: `portability/toolchains/gcc-mspm0/README.md`.

```text
make MSPM0_SDK_INSTALL_DIR=/Applications/ti/mspm0_sdk_2_11_00_07 SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh GCC_ARMCOMPILER=<GCC folder>
```

**Or ask an AI assistant.** Paste this prompt:

```text
Run git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git, then read fox-red-labs/START.md and follow it to build docs/builds/blink.md on my {kit}. Ask me before installing anything or writing to the board.
```

### Steps

1. **Mac.** Check you have at least 12 GB free (Apple menu, System Settings, General, Storage). Make a free myTI account on ti.com if you do not have one. Delete each installer file once it has finished installing.

   You should see: At least 12 GB free. CCStudio and the SDK take 10.3 GB once installed.

   If the Mac runs short during the CCStudio install, the installer file is probably still on disk; delete it.

2. **Browser, then installer.** Download CCStudio 21.0.1 for macOS from [TI's CCStudio download page](https://www.ti.com/tool/download/CCSTUDIO) (1.9 GB) and install it. It goes to `/Applications/ti/ccs2101`. Open it once when the installer finishes, then delete the installer file.

   You should see: CCStudio opens to its start page. About 19 minutes in all on the test Mac, 7 of them your time.

   If TI shows a short export approval form before the download, fill it in (about a minute). If CCStudio quits without a window on first launch, open it yourself from Applications.

3. **Browser.** Open [TI's MSPM0 SDK download page](https://www.ti.com/tool/download/MSPM0-SDK) (0.4 GB download, 2.2 GB installed) and download the macOS file, `mspm0_sdk_2_11_00_07.app.zip` (0.4 GB). On that page it is the macOS `.app.zip` entry for version 2.11.00.07.

   You should see: The zip lands in your downloads folder after about 3 minutes.

   If nothing seems to download, look in your browser's downloads. The SDK downloads in the browser, not inside CCStudio.

4. **Installer.** Unzip the file, open the installer and click through it. It installs to `/Applications/ti/mspm0_sdk_2_11_00_07` (2.2 GB). Then delete the zip and the installer.

   You should see: The folder `/Applications/ti/mspm0_sdk_2_11_00_07` exists. 1 minute of clicks and 2 min 48 s of waiting on the test Mac.

   If CCStudio lists no SDK afterwards, quit CCStudio and open it again.

5. **CCStudio.** Choose Help, Get Started with AI. Set Agent to Claude Code and tick Enable. Provider: Anthropic. Scope: pick Workspace. Leave Auth Token and Base URL blank, tick "Enable CCStudio IDE ecosystem MCP servers for Claude Code", click Apply, and log in to Claude in the chat panel if it asks. Keep CCStudio open from here on: its build and flash servers only run while it is open.

   You should see: The chat panel answers, and all four of CCStudio's tool servers (project, debug, SysConfig, serial) respond.

   If several chat panels open, close the extras; one is enough. Scope starts on Global; Workspace keeps the change out of every other Claude Code session on your Mac.

6. **Terminal.** Install Claude Code if you do not have it (`curl -fsSL https://claude.ai/install.sh | bash`, then `claude` once to log in). Save the file below as `ccs-mcp.json` in the folder you will work in, and start Claude Code with `claude --mcp-config ccs-mcp.json`. In VS Code, open that folder and start Claude Code from its panel the same way.

   ```text
   {"mcpServers": {
     "ccs-project":   {"command": "/usr/bin/env", "args": ["-u", "ELECTRON_RUN_AS_NODE", "/Applications/ti/ccs2101/ccs/tools/node/node", "/Applications/ti/ccs2101/ccs/Code Composer Studio.app/Contents/Resources/app/plugins/ccs-ai/extension/dist/mcp-server-proxy.js", "project"]},
     "ccs-debug":     {"command": "/usr/bin/env", "args": ["-u", "ELECTRON_RUN_AS_NODE", "/Applications/ti/ccs2101/ccs/tools/node/node", "/Applications/ti/ccs2101/ccs/Code Composer Studio.app/Contents/Resources/app/plugins/ccs-ai/extension/dist/mcp-server-proxy.js", "debug"]},
     "ccs-sysconfig": {"command": "/usr/bin/env", "args": ["-u", "ELECTRON_RUN_AS_NODE", "/Applications/ti/ccs2101/ccs/tools/node/node", "/Applications/ti/ccs2101/ccs/Code Composer Studio.app/Contents/Resources/app/plugins/ccs-ai/extension/dist/mcp-server-proxy.js", "sysconfig"]},
     "ccs-serial":    {"command": "/usr/bin/env", "args": ["-u", "ELECTRON_RUN_AS_NODE", "/Applications/ti/ccs2101/ccs/tools/node/node", "/Applications/ti/ccs2101/ccs/Code Composer Studio.app/Contents/Resources/app/plugins/ccs-ai/extension/dist/mcp-server-proxy.js", "serial"]}
   }}
   ```

   You should see: Claude Code starts with the four servers ccs-project, ccs-debug, ccs-sysconfig and ccs-serial registered. Saving this file and starting Claude Code by hand, as written here, has not been timed on a fresh Mac.

   If Claude Code says a server timed out, check CCStudio is open with a workspace and that step 5's Apply was pressed. If CCStudio is not in `/Applications/ti/ccs2101`, change that path in all four lines.

7. **Bench.** Plug the board into the Mac with the USB cable, at the micro-USB port on the debugger end of the board. There is no driver to install.

   You should see: The Mac sees a debug probe and two serial ports; `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e` lists the probe. A board that was used before may already be blinking its old program.

   If the Mac sees nothing, try another micro-USB cable: a charge-only cable looks the same. A USB-C-only Mac needs an adapter.

8. **Terminal or VS Code, Claude Code.** Type the line below. Claude Code imports TI's example through CCStudio, cuts the delay, builds it, flashes it and starts it. Look at the RGB LED. If Claude Code asks permission to use a `ccs-project` or `ccs-debug` tool, say yes; it asks once per tool.

   ```text
   I plugged in an LP-MSPM0G3507 and CCStudio is open. Import TI's gpio_toggle_output example for LP_MSPM0G3507, make the blink 5 times faster, then build it, flash it and run it.
   ```

   You should see: The RGB LED blinks about five times a second. The delay went from 0.5 s to 0.1 s (the constant 16000000 to 3200000).

   If the board blinked before you typed anything, that was a program already on it; this step replaces it, so a faster blink is your proof.

9. **Terminal or VS Code, Claude Code.** Now make it yours. Type the line below. Claude Code edits the loop, rebuilds and flashes again.

   ```text
   Change the program so the RGB LED shows blue for 2 seconds, then red for 2 seconds, then green for 2 seconds.
   ```

   You should see: The LED shows blue, then red, then green, two seconds each. You check the LED; the AI cannot.

   If the colors are wrong, say what you see in plain words and ask again. The AI edits the same file and flashes again.

### Expected result

The RGB LED blinks about five times a second within about 3 minutes of plugging in, once the tools are installed, then shows blue, red and green two seconds each when you ask for that.

### What this shows

- TI's gpio_toggle_output example imports, builds, flashes and runs on the LP-MSPM0G3507 from one typed sentence to Claude Code, through CCStudio's tool servers.
- A change you ask for in plain English reaches the board: the delay went from 0.5 s to 0.1 s and a person saw the LED change.

### Limits

- A clean-machine setup time: the Mac already had git, Claude Code and Node, and was logged in to Claude and myTI. About 30 minutes of clearing disk space was not counted.
- That CCStudio's own chat panel does the same: it answers, but no blink has been run through it.
- That step 6 works as written on a fresh Mac: saving the file by hand was not timed.
- Any result on Apple Silicon, Windows or Linux, or with an assistant other than Claude Code.
- The board was already programmed, so this is not a first-flash-from-new run.
- CCStudio must stay open: its build and flash servers only run while it is.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Time, plug-in to a changed blink | 2 min 29 s on the wall clock, 09:13:32 to 09:16:01 on 2026-10-05, with the tools already installed: import 2 s, build 6 s, flash and start 38 s, about a minute before any prompt was typed |  |
| Time, first-time setup | about 29 min on 2026-10-04 (11 of your time, 18 waiting); the Mac already had git, Claude Code and Node and was logged in to Claude and myTI, so this is not a clean-machine number |  |
| Software | CCStudio 21.0.1 (/Applications/ti/ccs2101), MSPM0 SDK 2.11.00.07, Intel MacBook, macOS 15.7.9 |  |
| Disk | CCStudio and the SDK take 10.3 GB once installed (CCStudio 1.9 GB download, SDK 0.4 GB download becoming 2.2 GB) |  |
