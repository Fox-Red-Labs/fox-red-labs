# Camera AI on a TI Linux board, timed

Boot TI's Edge AI Linux on a starter kit and run TI's ready-made models on its C7x accelerator. Read each model's time per picture over a serial cable.

The page: [site/project-linux-vision.html](../../site/project-linux-vision.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## TI's model zoo on the C7x, over the serial console

The demonstrated build: the SK-TDA4VM, its serial cable, and no network or display.

Status: Works. Boards: tda4.

Folders this version uses (in a sparse clone, fetch them first, then read `tda4/AGENTS.md`):

```bash
git sparse-checkout add tda4
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| SK-TDA4VM starter kit (TDA4VM Jacinto 7 vision processor, J721EXSKG01EVM) | 1 | the board |
| USB-C power supply that gives 20 V at 3 A or more (60 W class), for the SK-TDA4VM | 1 | power for the board |
| USB stick, FAT32, 4 GB or more | 1 | carries the image to the board when you have no card reader |

### Before you start

- A Mac, checked on an Intel Mac, macOS 15.7.9. It needs Homebrew (from brew.sh, which needs the Xcode command line tools: 1.8 GB installed here) and Python 3 with pip (the command line tools bring both). Step 1 installs lrzsz 0.13.1 (0.3 MB installed), xz 5.8.4 (2.6 MB installed) and pyserial 3.5 (under 1 MB). The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`.
- A second computer with 15 GB free for the unpacked image. The 1.1 GB download unpacks to 13.8 GB; a second Mac was used. Not needed if you have a micro-SD card reader and use TI's route.
- About an hour: 62 counted minutes from the first console check to the Linux login, 11 of them hands-on and 51 waiting. The serial driver install the evening before is not included.

### Steps

1. **Mac, browser and terminal.** One-time setup before the board: install the CP210x VCP driver on the Mac, a free download from the bridge chip's maker. Then in System Settings, Privacy and Security, click Allow. Then install the helpers the card-writing scripts use with Homebrew and pip. Checked with lrzsz 0.13.1, pyserial 3.5 and xz 5.8.4 on an Intel Mac, macOS 15.7.9.

   ```text
   brew install lrzsz xz
   pip install pyserial
   ```

   You should see: After step 2 plugs the board in, four serial ports appear: `/dev/cu.SLAB_USBtoUART`, `...UART1`, `...UART2` and `...UART4`.

   If System Settings never shows the Allow prompt, scroll to the bottom of Privacy and Security, where a "System software ... was blocked" notice names the driver. **Third-party behavior.**

2. **Bench.** Check SW1 (BOOT CTL) is all four sliders OFF, the micro-SD card is seated under the board, and plug a micro-B cable from J4 to the Mac. Leave the board unpowered for now. The 20 V USB-C supply goes to J14, the power port at the board's edge. Never plug it into J12, the data port.

   You should see: The four serial ports from step 1 appear on the Mac. The Linux console is `/dev/cu.SLAB_USBtoUART1` on the test Mac; the chip's UART-boot port is `...UART2`.

   If the console later goes silent after you move the cable to another USB port, probe all four ports again: the console can come back under another name. A 5 V charger can reset the board during boot.

3. **Terminal (Mac).** Open the console first, then power the board: log all four ports with timestamps, one per port, using `python3 tda4/tools/conlog.py <port> <log>`, at 115200 baud, 8N1. Power on the board from the 20 V supply.

   You should see: On a kit whose card carries no image, silence on all four ports before and after a press of SW2 (reset). That was the case on the test kit, so the card in the box was probably blank.

   If you connect after the board has booted you may see nothing even after Enter: press SW2 with the console open. Silence also looks like a wrong SW1 setting or the wrong port, so check both before writing the card.

4. **Browser, second computer.** On a second computer with 15 GB free, download `tisdk-edgeai-image-j721e-evm.wic.xz` from TI's SK-TDA4VM software page. The file is 1.1 GB, from Processor SDK Linux Edge AI 11.00.00.08, and no login is needed. Check it and record your own hash, because the page shows no checksum.

   ```text
   xz -t tisdk-edgeai-image-j721e-evm.wic.xz
   ```

   You should see: `xz -t` prints nothing and exits cleanly. The test file was 1 GB.

   With a micro-SD card reader, TI's route is shorter: write the file to the card with balenaEtcher, or from a terminal with `tda4/tools/write-card.sh diskN path/to/image.wic.xz`, then skip to step 8. That route was not run on this kit.

5. **Terminal (second computer).** No card reader, so the card is written from the board itself. On the second computer, unpack the image and mount the boot partition, using the disk number `hdiutil` prints in place of `diskN`. Then split the image into compressed pieces. Then copy `tiboot3.bin`, `sysfw.itb`, `tispl.bin` and `u-boot.img` from `/tmp/boot` to the folder with `tda4/tools/uart_boot.py`. Keep the pieces that are not all zeros. For this image they are 000 to 030, 032, 040, 041 and 048, 1.95 GB in all. Copy them into a `tda4/` folder on a FAT32 USB stick of 4 GB or more.

   ```text
   xz -dk tisdk-edgeai-image-j721e-evm.wic.xz
   hdiutil attach -imagekey diskimage-class=CRawDiskImage -nomount tisdk-edgeai-image-j721e-evm.wic
   diskutil mount -mountPoint /tmp/boot /dev/diskNs1
   split -b 268435456 -a 3 -d tisdk-edgeai-image-j721e-evm.wic wic.
   gzip -1 wic.*
   ```

   You should see: A 13.8 GB raw image after `xz -dk`; `hdiutil` prints a disk number to use for `diskN`.

   Edit the console port in the scripts in `tda4/tools/` and the piece list in `write_from_stick.py` for your Mac and image. Skip all-zero pieces: `gzcat f | tr -d '\0' | wc -c` is not 0 for the ones to keep.

6. **Bench, then terminal (Mac).** Power off. Set SW1: 1 ON, 2 OFF, 3 ON, 4 OFF (UART boot). Put the stick in a USB-A port. Run the script, then power on or press SW2.

   ```text
   python tda4/tools/uart_boot.py
   ```

   You should see: A stream of `C` on the UART-boot port, then the four boot files go in: xmodem, then ymodem, about 3.5 minutes at 9.7 kB/s. Then `U-Boot 2025.01` and a `=>` prompt on the console port.

   If it stops at the third file (`spl: ymodem err - Timed out`), press SW2 and run it again. Use the current version of the Python script, and no shell loop. Do not try U-Boot's `ums` command: it crashes U-Boot on this kit and you are back at this step.

7. **Terminal (Mac).** Write the card from the stick, piece by piece.

   ```text
   python tda4/tools/write_from_stick.py unzip
   ```

   You should see: For each piece, `load usb 0:1`, `unzip` and `mmc write` ending in `blocks written: OK`; 323 seconds for the whole image; at the end `mmc part` lists TI's two partitions.

   A piece that takes more than a minute is stuck: reset with SW2 and begin again from step 6. If the write was interrupted, run `write_from_stick.py unzip` again from U-Boot: every piece goes to a fixed block address, so a rerun overwrites the same places.

8. **Bench, then terminal (Mac).** Power off. Set all four sliders of SW1 to OFF (SD boot). Make sure the console is being logged, then power on.

   You should see: `U-Boot SPL`, `Starting kernel`, `Welcome to Arago`, then `tda4vm-sk login:` at 27.7 seconds on the first boot. The root file system grows to the card during it. Log in as `root`, no password.

   The red `Direct firmware load for j7-mcu-r5f0_1-fw failed with error -2` lines and `Failed to start Start psplash boot splash screen` are harmless: three R5F cores have no firmware on the image and there is no display.

9. **Terminal (Mac).** Push TI's model-timing script to the board over the console after making its folders. The board has no network in this route.

   ```text
   python3 tda4/tools/lcmd.py "mkdir -p /root/push /root/night"
   python3 tda4/tools/push_serial.py tda4/bench/zoo/zoo_bench.py /root/push/zoo_bench.py
   ```

   You should see: The push prints a matching sha256 for the file when it arrives.

   Push files as 76-column base64 only, which `push_serial.py` does; a single long line hangs the console at a `>` prompt.

10. **Terminal (Mac).** Run it in the background so the console's Ctrl-C cannot kill it. The script times every precompiled model in `/opt/model_zoo` through its runtime with TI's TIDL delegate on the C7x and its matrix accelerator, 30 timed runs each. Then read the log. These command lines are assembled from the script, its log and the FAQ rule; the recorded run typed them in pieces.

   ```text
   python3 tda4/tools/lcmd.py "setsid nohup python3 /root/push/zoo_bench.py 30 > /root/night/zoo_tidl.log 2>&1 < /dev/null &"
   python3 tda4/tools/lcmd.py "cat /root/night/zoo_tidl.log"
   ```

   You should see: One line per model, such as `ONR-KD-7060-human-pose-yolox-s-640x640 | keypoint_detection | onnxrt | tidl-c7x | [1, 3, 640, 640] | median 13.76 ms | min 13.72 | max 13.86 | n 30 | rss 284 MB`. The 14 zoo models came out between 2.17 ms (MobileNetV1) and 13.76 ms (YOLOX-s pose 640). The TVM models need `tda4/bench/zoo/tvm_bench.py` the same way.

   If a model takes hundreds of milliseconds, it ran on the CPU. Load the model's precompiled `artifacts` folder instead of the bare ONNX file. Then check the runtime log lists every node on the C7x, 274 of 274 for the pose model. Start long jobs with `setsid nohup`; a plain `nohup ... &` died at the next connect.

### Expected result

A Linux login prompt 27.7 seconds after power-on, and one timing line per zoo model, each between 2.17 and 13.76 ms on the C7x. The pose model, the biggest, reads 13.76 ms.

### What this shows

- TI's Edge AI Linux boots on the SK-TDA4VM from a card written with no card reader, Ethernet or display. That was one kit and one Mac.
- All 14 of TI's zoo models run on the C7x and its matrix accelerator in 2.17 to 13.76 ms each. TI's four published figures are within 10 percent of the measured values.
- The same eight ONNX models take 13 to 135 times longer on the two Arm cores with the CPU provider. A model that falls back to the CPU costs that much on this image.

### Limits

- No picture was shown to the models: the input is random numbers, and outputs were not compared. These are times only; accuracy was not measured.
- The CPU comparison is fp32 on the CPU against int8 on the accelerator. It shows the cost of falling back to the CPU and is no like-for-like comparison of the two cores.
- These timings come from one run. No independent builder has repeated them.
- Sustained load was not shown to be safe. The board reset about 60 seconds into a two-thread CoreMark plus TIDL loop. A five-minute pose graph took the hottest zone to 91.5 C.
- A card reader, an Ethernet link, a display or a camera were never used, so the camera and display paths are untested.
- The board had no card reader, Ethernet or display; the whole route is the serial cable at 9.7 kB/s for the boot files. TI's own route, which needs a reader and a display, was not run.
- TI's getting-started page assumes a display and Ethernet; the model-zoo layout was learned by listing folders.
- The board resets under heavy sustained load; it runs on a 20 V supply of unknown wattage with a passive heatsink and no fan.
- The TVM build of ssdLite mobDet 320 is 12.97 ms against 6.40 ms for the TFLite build. **Unknown (suspected: the input conversion inside the graph).**

### Numbers and sources

| | Value | Source |
|---|---|---|
| Board and image | SK-TDA4VM (J721EXSKG01EVM) revision PROC112A1, TI Edge AI SDK 11.00.00.08, Linux 6.12.17-ti, gcc 13.3, Python 3.12 | `tda4/README.md` |
| Runtimes | ONNX Runtime 1.15.0 with the TIDLExecutionProvider, TFLite through libtidl_tfl_delegate, DLR 1.13.0 for the three TVM models | `tda4/RESULTS-tda4.md` |
| Method | Precompiled int8 artifacts from /opt/model_zoo, random input, one warm-up, 30 timed runs, median wall time in Python; timing only, outputs not compared |  |
| C7x time per picture | 2.17 ms (MobileNetV1 mlperf 224) to 13.76 ms (YOLOX-s pose 640); regNetx-200mf 2.89 ms, YOLOX-nano-lite 416 4.66 ms, ssd-lite-mobv2 512 6.10 ms |  |
| Two A72 cores, same ONNX models | fp32 graphs on the CPU provider, 3 timed runs: 37.4 ms to 1,767 ms, 13 to 135 times slower; the zoo ships no int8 CPU build | `docs/experiments/2026-10-10-tda4-c7x-vs-a72.md` |
| Against TI's published figures | TI's four published C7x figures (2.03, 3.04, 5.03 and 5.9 ms) are within 10 percent of the measured values | `docs/experiments/2026-10-10-tda4-c7x-vs-a72.md` |
| First boot | 27.7 s from the first bootloader byte to the login prompt, with the root file system growing to the card; warm boot 24.0 to 24.5 s | `tda4/README.md` |
| Cores and memory | Two Cortex-A72 at 1.998 GHz measured by perf; Linux sees 2,170 MiB of the 4 GiB | `tda4/README.md` |
