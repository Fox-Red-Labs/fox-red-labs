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
