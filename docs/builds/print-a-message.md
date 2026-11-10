# Print a message from the chip to your Mac

Flash TI's console example and read its text in a terminal on your computer, over the board's own USB cable. No extra wire.

The page: [site/project-print-a-message.html](../../site/project-print-a-message.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## TI's console example on the LP-MSPM0L1306

Copy TI's UART console example, build it, flash it, and read its text in a terminal on the Mac.

Status: Works. Boards: l1306.

Folders this version uses (in a sparse clone, fetch them first, then read `mspm0l1306/AGENTS.md`):

```bash
git sparse-checkout add mspm0l1306 mspm0g3507/mcu-bench/sdk-common tools/frl
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM0L1306 LaunchPad development kit | 1 | the board |
| Micro-USB data cable | 1 | power, programming and the serial text |

### Before you start

- A Mac with CCStudio 21.0.1 (1.9 GB download, 8.1 GB installed), the MSPM0 SDK 2.11.00.07 (0.4 GB, 2.2 GB installed), SysConfig 1.26.3 (0.96 GB installed) and xPack OpenOCD 0.12.0-7 (2 MB). Links are in step 1. Checked on an Intel MacBook on macOS 15.7.9 only.
- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`. It holds the helper scripts. About 12 GB free for the tools.

### Steps

1. **Mac, terminal.** Install the tools once, all free. Get CCStudio 21.0.1 from [TI's CCStudio download page](https://www.ti.com/tool/download/CCSTUDIO) (1.9 GB download, 8.1 GB installed, free myTI login). It brings TI Arm Clang 5.1.1. Get the MSPM0 SDK 2.11.00.07 from [TI's MSPM0 SDK download page](https://www.ti.com/tool/download/MSPM0-SDK) (0.4 GB download, 2.2 GB installed). Get SysConfig 1.26.3 ([TI's SysConfig page](https://www.ti.com/tool/download/SYSCONFIG), 0.96 GB installed). Get xPack OpenOCD 0.12.0-7 from [its GitHub release page](https://github.com/xpack-dev-tools/openocd-xpack/releases/tag/v0.12.0-7) (2 MB; the file `xpack-openocd-0.12.0-7-darwin-x64.tar.gz` on an Intel Mac, `xpack-openocd-0.12.0-7-darwin-arm64.tar.gz` on Apple Silicon). Then clone this repository into `<repository folder>`. The helper scripts `sdkmake.sh` and `tools/ocd.sh` are in it. This repo's scripts point at SysConfig 1.26.3. It came with the SimpleLink Wi-Fi SDK installer on the test Mac and sits in `/Applications/ti/sysconfig_1.26.3`. CCStudio 21.0.1 carries its own 1.28.1. This repo's scripts do not point at it.

   You should see: `/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin/tiarmclang --version` prints 5.1.1.

   If your tools live somewhere other than `/Applications/ti`, edit the paths at the top of `mspm0g3507/mcu-bench/sdk-common/sdkmake.sh`.

2. **Bench.** Leave the jumpers as the board shipped: J16 and J17 on the XDS side, the right-hand position, which carries the chip's UART to the USB cable. Plug the board into the Mac with a micro-USB data cable. Find the probe's serial number: the port names are `/dev/cu.usbmodem<serial>1` and `...4`.

   ```text
   ls /dev/cu.usbmodem*
   ```

   You should see: Two ports. The one ending in 1 is the chip's UART0 at 115200 8N1; the one ending in 4 is not used here. With probe serial <probe serial>, the port is `/dev/cu.usbmodem<probe serial>1`. Or read the serial with `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e`.

   If no ports appear, try another cable; a charge-only cable looks the same. If you moved J16 and J17 to BP for a loop-back test, move them back to XDS or the serial window stays silent.

3. **Terminal.** Copy TI's console example out of the SDK, from the `mspm0l1306` folder of this repository, and build it.

   ```text
   cd <repository folder>/mspm0l1306
   cp -R /Applications/ti/mspm0_sdk_2_11_00_07/examples/nortos/LP_MSPM0L1306/driverlib/uart_tx_console_multibyte_repeated_fifo_dma .
   ../mspm0g3507/mcu-bench/sdk-common/sdkmake.sh uart_tx_console_multibyte_repeated_fifo_dma/ticlang
   ```

   You should see: A build with no errors that leaves `uart_tx_console_multibyte_repeated_fifo_dma/ticlang/uart_tx_console_multibyte_repeated_fifo_dma.out`.

   If the make line cannot find SysConfig or the compiler, the paths in step 1's `sdkmake.sh` do not match your install.

4. **Terminal.** Flash it with OpenOCD. `tools/ocd.sh` picks the probe by serial number and refuses to run if it is missing; set `PROBE` to your serial first (`<probe serial>` below stands for it). To find yours, run `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e` if you have CCStudio (it prints `Serial Num:`), else `system_profiler SPUSBDataType | grep -A3 XDS110`. Run it from the same folder as the build step.

   ```text
   export PROBE=<probe serial>
   tools/ocd.sh "reset halt" "program uart_tx_console_multibyte_repeated_fifo_dma/ticlang/uart_tx_console_multibyte_repeated_fifo_dma.out verify" "reset run"
   ```

   You should see: OpenOCD ends with `** Verified OK **` after about 2 s. In the sweep this example flashed in 2.3 s.

   OpenOCD prints `Error: checksum mismatch - attempting binary compare` first; that is normal, and the line after it is the real result.

5. **Terminal.** Open the chip's serial port. To leave `screen`, press Ctrl-A then K. This step is assembled, not recorded: the sweep read the port with a script, and this example's port was read by a script, not opened in `screen`.

   ```text
   screen /dev/cu.usbmodem<probe serial>1 115200
   ```

   You should see: The line `==== MSPM0 Console Test ====` appears on your Mac, sent by the chip. TI's README says the example writes to the console once a second.

   If the window stays empty, check the port ends in 1 and the baud is 115200.

### Expected result

A line of text from the chip appears in a terminal window on your Mac every time the example sends it, at 115200 baud, with no extra wires.

### What this shows

- The LP-MSPM0L1306 sends text to a terminal on the Mac over the board's own USB cable, using TI's unmodified example.
- The same example printed readable text on the LP-MSPM0G3507 in the example sweep.

### Limits

- That text typed on the Mac reaches the chip: this example only transmits. The one-wire loop-back tests check both directions and are separate demos. They use J4 pin 34 to pin 33 on the G3507 and J1 pin 4 to pin 3 on the L1306.
- That `screen` shows the text as the sweep script did: the sweep read the port with a script, and `screen` is the method this repository's LVGL demo uses. The two were not compared with this example.
- A speed or reliability figure: the loop-back demos give those: every byte right from 9.6 kbaud to 4.5 Mbaud.
- The route is written for the LP-MSPM0L1306. The G3507 has its own flash command and probe file, and was not run by hand in this form.
- TI's example needs the board's shipped jumper settings; the loop-back tests move J16 and J17 and the serial window goes silent until they are put back.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Example | TI's uart_tx_console_multibyte_repeated_fifo_dma for LP_MSPM0L1306: UART0 on PA8 (TX) and PA9 (RX) through the DMA, timer-driven; verdict running, 195 bytes read, flash 2.3 s, in the sweep |  |
| Serial settings | UART0, 115200 8N1, on the probe port whose name ends in 1 | `mspm0l1306/FAQ.md` |
| Tools | MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, xPack OpenOCD 0.12.0-7, Intel Mac on macOS 15.7.9 | `mspm0l1306/README.md` |

## From a terminal, no AI step, on the LP-EM-CC2340R5

One command backs the board up, builds TI's example with your message in it, flashes it and reads the message back on the serial port.

Status: Works. Boards: cc2340r5.

Folders this version uses (in a sparse clone, fetch them first, then read `cc2340r5/AGENTS.md`):

```bash
git sparse-checkout add cc2340r5 tools/frl
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-EM-CC2340R5 LaunchPad development kit | 1 | the board (LP-EM-CC2340R5) |
| LP-XDS110ET debugger | 1 | debug probe |
| USB data cable | 1 | power and programming |

### Before you start

- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`; `start.sh` and `tools/frl` are in it. Checked on an Intel Mac on macOS 15.7 only.

### Steps

1. **Mac, terminal.** Install the tools once, all free: [SimpleLink Low Power F3 SDK 9.21.00.36](https://www.ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK), [TI Arm Clang 5.1.1.LTS](https://www.ti.com/tool/download/ARM-CGT-CLANG), [SysConfig 1.28.0](https://www.ti.com/tool/download/SYSCONFIG), [TI OpenOCD (CCSTUDIO-OPENOCD) 1.5.0](https://www.ti.com/tool/download/CCSTUDIO-OPENOCD/1.5.0), [DSLite (in CCStudio or UniFlash) 21.0.0.4166](https://www.ti.com/tool/download/UNIFLASH). The tool sizes were not all recorded. `python3 tools/frl/frl.py doctor --board <board id>` lists anything missing, with links. If your tools live elsewhere, `tools/frl/toolenv.py` reads the `FRL_` variables from the start page.

   You should see: `python3 tools/frl/frl.py doctor --board cc2340r5` ends with every tool found.

   If a tool is missing, the doctor command names it and prints its download link.

2. **Bench.** Connect the LP-XDS110ET probe to the LP-EM board and plug the probe into the Mac with a USB data cable. The probe's serial port that carries the message is the first of its two. Read its serial number with `ls /dev/cu.usbmodem*` or with `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e`.

   You should see: The Mac lists the probe and two serial ports.

   If nothing appears, try another cable; a charge-only cable looks the same.

3. **Terminal.** From the root of this repository, run the route with your probe's serial number and your message (1 to 40 plain characters, no quotes or backslashes). It backs the board up, copies TI's example (examples/nortos/LP_EM_CC2340R5/drivers/display), puts your text in one line of its source, builds it, flashes it and reads the board's serial port at 115200 baud.

   ```text
   ./start.sh cc2340r5 print-a-message --probe <serial> --message "Hello from my LaunchPad"
   ```

   You should see: The run ends with a line that starts `OK:` and says the board printed your message on the serial port, counting how many times it saw it.

   If it stops with FRL-E002, several probes are plugged in: name yours with `--probe`. Every write erases the whole chip, CCFG included; frl says so before writing and the restore writes the backup back.

4. **Terminal.** Run it again with different words. The check looks for the message you asked for, so a message the board is not printing fails.

   ```text
   ./start.sh cc2340r5 print-a-message --probe <serial> --message "Second message"
   ```

   You should see: A new `OK:` line with the new words.

   If it answers FRL-E040, the serial port shows other text than the message you asked for.

### Expected result

The board prints your message on its serial port, and the one-command run ends with an OK line.

### What this shows

- TI's display example builds, flashes and prints your own words on the LP-EM-CC2340R5 from one command.
- The run reads the board's serial port and checks the words you asked for.

### Limits

- That an AI assistant can drive this route: no AI step was timed for it.
- A person's first-hour time: the run times are scripted runs with the tools already installed.
- The route lists the CC2340R5 print recipe as tested on the bench the page was written on, with one probe.
- Checked on an Intel Mac on macOS 15.7 only.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Write | 9.2 s median of three DSLite writes (9.8, 9.2, 8.8 s); build of TI's drivers/display about 10 s | `docs/frl.md` |

## From a terminal, no AI step, on the LP-MSPM33C321A

One command backs the board up, builds TI's UART console example with your message in it, flashes it and reads the message back on the serial port.

Status: Works. Boards: mspm33c321a.

Folders this version uses (in a sparse clone, fetch them first, then read `mspm33c321a/AGENTS.md`):

```bash
git sparse-checkout add mspm33c321a tools/frl
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM33C321A LaunchPad development kit | 1 | the board (LP-MSPM33C321A) |
| USB data cable | 1 | power and programming |

### Before you start

- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`; `start.sh` and `tools/frl` are in it. Checked on an Intel Mac on macOS 15.7 only.

### Steps

1. **Mac, terminal.** Install the tools once, all free: [MSPM33 SDK 1.04.00.00](https://www.ti.com/tool/download/MSPM33-SDK), [TI Arm Clang 5.1.1.LTS](https://www.ti.com/tool/download/ARM-CGT-CLANG), [SysConfig 1.28.0](https://www.ti.com/tool/download/SYSCONFIG), [TI OpenOCD (CCSTUDIO-OPENOCD) 1.5.0](https://www.ti.com/tool/download/CCSTUDIO-OPENOCD/1.5.0). The tool sizes were not all recorded. `python3 tools/frl/frl.py doctor --board <board id>` lists anything missing, with links. If your tools live elsewhere, `tools/frl/toolenv.py` reads the `FRL_` variables from the start page.

   You should see: `python3 tools/frl/frl.py doctor --board mspm33c321a` ends with every tool found.

   If a tool is missing, the doctor command names it and prints its download link.

2. **Bench.** Plug the board into the Mac with a data cable. The board shows up as an XDS110 probe with two serial ports; the first carries the message. Read your probe's serial number with `ls /dev/cu.usbmodem*` (the text after `usbmodem`, minus the last digit), or with `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e`.

   You should see: The Mac lists the probe and two serial ports.

   If nothing appears, try another cable; a charge-only cable looks the same.

3. **Terminal.** From the root of this repository, run the route with your probe's serial number and your message (1 to 40 plain characters, no quotes or backslashes). It backs the board up, copies TI's example (examples/nortos/LP_MSPM33C321A/driverlib/uart_tx_console_multibyte_repeated_fifo_dma), puts your text in one line of its source, builds it, flashes it and reads the board's serial port at 115200 baud.

   ```text
   ./start.sh mspm33c321a print-a-message --probe <serial> --message "Hello from my LaunchPad"
   ```

   You should see: The run ends with a line that starts `OK:` and says the board printed your message on the serial port, counting how many times it saw it.

   If it stops with FRL-E002, several probes are plugged in: name yours with `--probe`. xPack OpenOCD 0.12.0-7 has no MSPM33 flash driver; the route needs TI's OpenOCD, unpacked to `~/ti/ti-openocd` or named with `FRL_TI_OPENOCD`. TI's example sends five lines, one a second, then stops, so the check counts a few repeats.

4. **Terminal.** Run it again with different words. The check looks for the message you asked for, so a message the board is not printing fails.

   ```text
   ./start.sh mspm33c321a print-a-message --probe <serial> --message "Second message"
   ```

   You should see: A new `OK:` line with the new words.

   If it answers FRL-E040, the serial port shows other text than the message you asked for.

### Expected result

The board prints your message on its serial port, and the one-command run ends with an OK line.

### What this shows

- TI's UART console example builds, flashes and prints your own words on the LP-MSPM33C321A from one command.
- The run reads the board's serial port and checks the words you asked for.

### Limits

- That an AI assistant can drive this route: no AI step was timed for it.
- A person's first-hour time: the run times are scripted runs with the tools already installed.
- The route lists the MSPM33C321A print recipe as tested on the bench the page was written on, with one probe.
- Checked on an Intel Mac on macOS 15.7 only.
- The recorded runs used a build of TI's OpenOCD `ti-release` branch at commit cb22a31e.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Write | 0.7 to 0.8 s for the OpenOCD write and read-back (0.7, 0.8, 0.7 s); build 3.2 to 4.2 s; whole route 28 to 30 s with the backup | `data/results/mspm33c321a.csv` |

## From a terminal, no AI step, on the LP-EM-CC35X1

One command backs the board up, builds TI's example with your message in it, flashes it and reads the message back on the serial port.

Status: Works. Boards: cc35x1.

Folders this version uses (in a sparse clone, fetch them first, then read `cc35x1/AGENTS.md`):

```bash
git sparse-checkout add cc35x1 tools/frl
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-EM-CC35X1 LaunchPad development kit | 1 | the board (LP-EM-CC35X1) |
| LP-XDS110ET debugger | 1 | debug probe |
| USB data cable | 1 | power and programming |

### Before you start

- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`; `start.sh` and `tools/frl` are in it. Checked on an Intel Mac on macOS 15.7 only.

### Steps

1. **Mac, terminal.** Install the tools once, all free: [SimpleLink Wi-Fi SDK 10.20.00.39](https://www.ti.com/tool/download/SIMPLELINK-WIFI-SDK), SimpleLink Wi-Fi Toolbox 4.3.23, [TI Arm Clang 5.1.1.LTS](https://www.ti.com/tool/download/ARM-CGT-CLANG), [SysConfig 1.28.0](https://www.ti.com/tool/download/SYSCONFIG). The tool sizes were not all recorded. `python3 tools/frl/frl.py doctor --board <board id>` lists anything missing, with links. If your tools live elsewhere, `tools/frl/toolenv.py` reads the `FRL_` variables from the start page.

   You should see: `python3 tools/frl/frl.py doctor --board cc35x1` ends with every tool found.

   If a tool is missing, the doctor command names it and prints its download link.

2. **Bench.** Connect an LP-XDS110ET or LP-XDS110 probe to the LP-EM-CC35X1 and power the board from its own USB-C cable. Plug the probe into the Mac with a USB data cable. Read its serial number with `ls /dev/cu.usbmodem*` or with `xdsdfu -e`.

   You should see: The Mac lists the probe and two serial ports.

   If nothing appears, try another cable; a charge-only cable looks the same.

3. **Terminal.** From the root of this repository, run the route with your probe's serial number and your message (1 to 40 plain characters, no quotes or backslashes). It backs the board up, copies TI's example (examples/rtos/LP_EM_CC35X1/drivers/uart2echo), puts your text in one line of its source, builds it, flashes it and reads the board's serial port at 115200 baud.

   ```text
   ./start.sh cc35x1 print-a-message --probe <serial> --message "Hello from my LaunchPad"
   ```

   You should see: The run ends with a line that starts `OK:` and says the board printed your message on the serial port, counting how many times it saw it.

   If it stops with FRL-E002, several probes are plugged in: name yours with `--probe`. The chip keeps its program in external flash behind its security and the Toolbox has no read-back, so the backup saves the fuse report, the chip's unique ID and the running program's banner, and the restore reprograms the image the board had.

4. **Terminal.** Run it again with different words. The check looks for the message you asked for, so a message the board is not printing fails.

   ```text
   ./start.sh cc35x1 print-a-message --probe <serial> --message "Wi-Fi 6 board says hi"
   ```

   You should see: A new `OK:` line with the new words.

   If it answers FRL-E040, the serial port shows other text than the message you asked for.

### Expected result

The board prints your message on its serial port, and the one-command run ends with an OK line.

### What this shows

- TI's uart2echo example builds, flashes and prints your own words on the LP-EM-CC35X1 from one command.
- The run reads the board's serial port and checks the words you asked for.

### Limits

- That an AI assistant can drive this route: no AI step was timed for it.
- A person's first-hour time: the run times are scripted runs with the tools already installed.
- The chip has no read-back, so the check proves the message on the serial port, not the flash contents.
- The recipe's status for this board is partial for that reason.
- Checked on an Intel Mac on macOS 15.7 only.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Write | Toolbox write 23.3 s median of three (23.9, 24.2, 23.3 s); build and sign about 45 s; whole runs 66, 61 and 71 s | `docs/frl.md` |

## From a terminal, no AI step, on the LAUNCHXL-F2800137

One command backs the board up, builds TI's launchxl_f2800137 demo with your message in it, flashes it and reads the message back on the serial port.

Status: Works. Boards: f2800137.

Folders this version uses (in a sparse clone, fetch them first, then read `c2000-launchpad/AGENTS.md`):

```bash
git sparse-checkout add c2000-launchpad tools/frl
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LAUNCHXL-F2800137 LaunchPad development kit | 1 | the board (LAUNCHXL-F2800137) |
| USB data cable | 1 | power and programming |

### Before you start

- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`; `start.sh` and `tools/frl` are in it. Checked on an Intel Mac on macOS 15.7 only.

### Steps

1. **Mac, terminal.** Install the tools once, all free: [C2000Ware 26.02.00.00](https://www.ti.com/tool/download/C2000WARE), TI C2000 compiler 25.11.1.LTS, [SysConfig 1.28.1](https://www.ti.com/tool/download/SYSCONFIG), [CCStudio 21.0.1](https://www.ti.com/tool/download/CCSTUDIO). OpenOCD has no C28x support, so this route needs CCStudio installed for its scripting; the doctor command lists what is missing. `python3 tools/frl/frl.py doctor --board <board id>` lists anything missing, with links. If your tools live elsewhere, `tools/frl/toolenv.py` reads the `FRL_` variables from the start page.

   You should see: `python3 tools/frl/frl.py doctor --board <board id>` ends with every tool found.

   If a tool is missing, the doctor command names it and prints its download link.

2. **Bench.** Plug the board into the Mac with a USB data cable. Its onboard XDS110 probe shows up with serial ports; there is no driver to install. Read the probe's serial number with `ls /dev/cu.usbmodem*` or with `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e`.

   You should see: The Mac lists the probe and two serial ports.

   If nothing appears, try another cable; a charge-only cable looks the same.

3. **Terminal.** From the root of this repository, run the route with your probe's serial number and your message (1 to 40 plain characters, no quotes or backslashes). It backs the board up, copies TI's example (driverlib/f280013x/examples/launchxl_f2800137), puts your text in one line of its source, builds it, flashes it and reads the board's serial port at 115200 baud.

   ```text
   ./start.sh f2800137 print-a-message --probe <serial> --message "Hello from my LaunchPad"
   ```

   You should see: The run ends with a line that starts `OK:` and says the board printed your message on the serial port, counting how many times it saw it.

   If it stops with FRL-E002, several probes are plugged in: name yours with `--probe`. TI's demo prints a line a second and never stops, so the check counts a few repeats.

4. **Terminal.** Run it again with different words. The check looks for the message you asked for, so a message the board is not printing fails.

   ```text
   ./start.sh f2800137 print-a-message --probe <serial> --message "Second message"
   ```

   You should see: A new `OK:` line with the new words.

   If it answers FRL-E040, the serial port shows other text than the message you asked for.

### Expected result

The board prints your message on its serial port, and the one-command run ends with an OK line.

### What this shows

- TI's LaunchPad demo, edited by the route, prints your own words on the LAUNCHXL-F2800137 from one command.
- The run reads the board's serial port and checks the words you asked for.

### Limits

- That an AI assistant can drive this route: no AI step was timed for it.
- A person's first-hour time: the run times are scripted runs with the tools already installed.
- The route lists the F2800137 print recipe as tested on the bench the page was written on, with one probe.
- Checked on an Intel Mac on macOS 15.7 only.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Write | 11.2 to 14.0 s for the CCStudio flash write and read-back (14.0, 11.2, 12.9 s); build 1.1 to 2.9 s; whole route 67 to 73 s with the backup | `data/results/f2800137.csv` |
