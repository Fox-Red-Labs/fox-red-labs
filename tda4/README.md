# SK-TDA4VM starter kit: boot TI's Edge AI Linux with no card reader, and what it measures

> Disclosure: Fox Red Labs is a lab run by a TI employee on personal time. Contributors' projects are their own. Not affiliated with, sponsored by, or endorsed by Texas Instruments or any other manufacturer.

The SK-TDA4VM boots TI's Edge AI Linux in 27.7 s on its first boot, first bootloader byte to the login prompt, from a card written from the board itself, with no card reader, Ethernet or display. TI quotes under 20 s to a desktop wallpaper, a different endpoint. Every number below was measured over the serial cable on this bench; TI's figures are marked as TI's. This page shows how to write the card from the board itself.

Terms: [vocab](../docs/vocab.md). Problems and fixes: [FAQ](FAQ.md). Every number: [results](RESULTS-tda4.md). 1 kB = 1,024 bytes; GB and MB below are the decimal units the files report.

Words this page uses: the **A72** cores run Linux; the **C7x** is the chip's DSP with its **MMA** matrix unit, where **TIDL** (TI's deep-learning runtime) runs models; the **R5F** cores are small real-time cores running TI's firmware, listed under **remoteproc**; **U-Boot** is the bootloader, **SPL** its first stage, and the chip's **ROM** the fixed first-stage boot code in silicon that loads them; **xmodem** and **ymodem** are the old serial file transfers the ROM and U-Boot accept. **DMIPS** is Dhrystone's score unit, millions of Dhrystone instructions per second; **MTU** is the largest packet a network link carries; **RoCE** is a packet format for writing straight into a remote machine's memory; a **Holoscan Sensor Bridge** is NVIDIA's camera-to-GPU Ethernet device, and here the board pretends to be one. Other words are defined where they first appear.

Contents: [The board](#the-board), [Ten-minute path](#ten-minute-path-measured-62-counted-minutes-with-no-card-reader), [One-command build](#one-command-build), [What was measured](#what-was-measured), [Running the standard tests](#running-the-standard-tests-over-the-console), [Serial toolbox](#serial-toolbox-the-board-had-no-network), [Files](#files), [If you see this](#if-you-see-this), [Limits](#limits), [Where these facts come from](#where-these-facts-come-from).

## What is in this folder, and what is not

<!-- folder-contents -->
**In this folder:** this README; `AGENTS.md` (notes for an AI assistant, every line from a run); `FAQ.md` (every error hit, with its exact text and the fix); `RESULTS-tda4.md` (the measured results). Folders: `hsb/` Holoscan Sensor Bridge emulation, `img/` pictures, `tools/` helper scripts for this board, `bench/zoo/` the model benchmark. Results are rows in `data/results/tda4.csv` at the top of the repository, not in this folder.

**Not in this folder:** TI's SDK, CCStudio, SysConfig and the compilers (you install them; the versions are under "Where these facts come from"); built images (`.out`, `.hex`, `.bin`, `.elf`), which are not cleared for redistribution, so you build them with the command below.

**Taking this folder alone:** the commands in this README run from the top of a clone and call `tools/` and `data/`; the benchmarks also borrow sources from folders that several boards share. A sparse clone fetches only those, about 10 to 60 MB, instead of the whole repository (about 500 MB):

```bash
git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git
cd fox-red-labs
git sparse-checkout add tda4 tools data third-party/holoscan-sensor-bridge
```

`docs/` is left out on purpose (236 MB, mostly pictures). Read a page a link points to there with `git show HEAD:docs/<file>`, for example `git show HEAD:docs/vocab.md`.


## The board

SK-TDA4VM, orderable J721EXSKG01EVM (box: SPRZ520), board revision PROC112A1 (001). Confirmed from photos: `img/tda4-top.jpg`, `img/tda4-underside.jpg`. SW1 (BOOT CTL) reads all OFF, the SD-boot setting in SPRUJ21 Table 2-3. The kit includes a micro-SD card (SPRUJ21 2.3.8); the one in this box booted nothing, which is inferred to mean blank. Silicon: J721E, two Cortex-A72 at about 2.0 GHz (1.998 GHz measured by perf, the Linux profiler). Linux sees 2,170 MiB of the 4 GiB (`free -m`); the kernel log shows the rest reserved for the DSPs, the R5F cores and DMA pools. Under `/sys/class/remoteproc` the C7x, both C66x (TI's older DSP cores) and three R5F cores run TI's firmware; three R5F cores stay offline (the console shows `Direct firmware load for j7-mcu-r5f0_1-fw failed with error -2` and the like for them; the image ships no firmware for those cores, FAQ).

### Serial console (J4, micro-B)

- CP2108 four-port bridge, 10c4:ea71. macOS needs the CP210x VCP driver: Apple's own driver covers only the single-port CP2102. After the Allow click the ports are `/dev/cu.SLAB_USBtoUART`, `...1`, `...2`, `...4`.
- The Linux console (MAIN_UART0) is `/dev/cu.SLAB_USBtoUART1` on this Mac; the chip's UART-boot port (MCU_UART0) is `/dev/cu.SLAB_USBtoUART2`. The user guide's table order (WKUP_UART0, MCU_UART0, UART0 which is the console, UART1) did not match the Mac's names; if in doubt, log all four with `tools/conlog.py`.
- 115200 8N1, login `root`, no password. Open the port before powering the board; connecting after boot can show nothing even with Enter.

## Ten-minute path (measured: 62 counted minutes with no card reader)

**TI's route, not run on this kit.** With a micro-SD reader: write `tisdk-edgeai-image-j721e-evm.wic.xz` (a compressed disk image) to the card with balenaEtcher (`tools/write-card.sh diskN path/to/image.wic.xz` does the same from a terminal), SW1 all OFF, console open first, power on. TI's Edge AI getting-started page for this kit (read 2026-10-09) says under 20 s to the wallpaper on a display.

**The route run here, with no reader.** Install the CP210x VCP driver (Allow in Privacy and Security), `brew install lrzsz` and pyserial on the Mac, then the "No-reader path" below. Measured: 11 minutes hands-on and 51 minutes waiting. Of the waiting, 15 minutes went on the bootloader transfers at 9.7 kB/s (four runs: two reached U-Boot, two failed because of the script. FAQ), 12 on splitting the image into chunks, 5 on the card write and 4 on the download; the other 15 went on the silent-console check, the Ethernet and USB-gadget attempts, copying to the stick and the first look around.

### No-reader path: write the card from U-Boot

Needs: the console cable, a USB stick of 4 GB or more (FAT32; files already on it can stay), the SD image on a second computer with 15 GB free (the 1.1 GB download plus its 13.8 GB unpacked form), `lrzsz` and pyserial on the Mac. The scripts in `tools/` name the console port of the Mac used here, `/dev/cu.SLAB_USBtoUART1`, and `write_from_stick.py` lists this image's chunks (000 to 030, 032, 040, 041, 048); edit both for your Mac and your image.

1. Boot files. On the second computer (a Mac in this run): `xz -dk tisdk-edgeai-image-j721e-evm.wic.xz`, then `hdiutil attach -imagekey diskimage-class=CRawDiskImage -nomount tisdk-edgeai-image-j721e-evm.wic`, which prints a disk number; `diskutil mount -mountPoint /tmp/boot /dev/diskNs1` mounts the first (FAT) partition. Copy `tiboot3.bin`, `sysfw.itb`, `tispl.bin` and `u-boot.img` from it to the folder with `tools/uart_boot.py`. On Linux, `losetup -P` and `mount` do the same.
2. Chunks. Split the unpacked image into 256 MiB pieces and gzip each (`split -b 268435456 -a 3 -d image.wic wic.`; `gzip -1 wic.*`). Keep the pieces that are not all zeros (`gzcat f | tr -d '\0' | wc -c` is not 0); for this image that is 000 to 030, 032, 040, 041 and 048, 1.95 GB in all. Copy them to a `tda4/` folder on the stick. Eject.
3. Board. Power off. SW1: 1 ON, 2 OFF, 3 ON (UART boot); leave 4 OFF. Stick into a USB-A port. Run `python tools/uart_boot.py`, then power on or press SW2 (reset). It finds the port streaming "C", sends the four files (xmodem, then ymodem; about 3.5 minutes at 115200) and stops U-Boot's autoboot at the `=>` prompt.
4. Write. `python tools/write_from_stick.py unzip`. For each chunk: `load usb 0:1 0x82000000 /tda4/wic.NNN.gz`, `unzip 0x82000000 0xB0000000`, `mmc write 0xB0000000 <block> <count>`. 323 s for this image, each write answering "blocks written: OK". It ends with `mmc part` showing TI's two partitions.
5. Boot. Power off, SW1 all four OFF, power on. The root file system grows to the card during the first boot.

What did not work: U-Boot's `ums` (USB mass storage) on this kit. Both USB controllers come up host-only, and rebinding one to the gadget driver crashed U-Boot. FAQ. Use the stick.

## One-command build

TI's route, with a micro-SD reader (not run on this kit): `tools/write-card.sh diskN path/to/image.wic.xz` writes the image to the card from a terminal. The route run here, with no reader, is two commands after the boot files and chunks are prepared: `python tools/uart_boot.py` loads U-Boot over the console cable (about 3.5 minutes at 115200), then `python tools/write_from_stick.py unzip` writes the card from the stick (323 s for this image, each write answering "blocks written: OK"). Neither command installs a driver, splits the image or formats the stick: `tools/prep_stick.sh diskN` formats a spare stick and copies the chunks (it erases the stick).

## What was measured

Every number was read over the serial cable on this bench with the kit's own tools; TI's published figures are rows marked claim. Every number, how it was made, what did not run, TI's published figures against the measured ones and what an engineer should still measure: [RESULTS-tda4.md](RESULTS-tda4.md). These are starter-kit numbers: a micro-SD card, a passive heatsink with no fan, a supply of unknown rating and TI's demo image, measured with simple tools. The chip can do better in a product; the results page lists each limit and how.

### Boot

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| First boot, bootloader to login prompt | 27.7 s | the root file system grows to the card from about 22 to 26.7 s | measured | `linux_boot_time` first-boot in `data/results/tda4.csv` |
| Warm boot, three runs | 24.0 to 24.5 s bootloader to login; 27.9 to 28.3 s from the `reboot` command | stamps on the Mac's clock from the first `U-Boot SPL` byte | measured | `linux_boot_time` warm |
| TI's boot time | under 20 s to a desktop wallpaper | a different endpoint than the rows above; TI's Edge AI getting-started page for this kit, read 2026-10-09 | claim | TI's page |

### AI and video

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| Pose model YOLOX-s 640x640 | 13.8 ms per inference on the C7x/MMA; 1,767 ms on the two A72s | fp32 on the A72s | measured | `dl_inference_ms` |
| TI model zoo, 11 models on the C7x | 2.2 to 13.8 ms each | the four figures TI publishes for them (its data-sheet page, read 2026-10-09) are within 10 percent | measured | `dl_inference_ms` |
| TI model zoo, the three TVM models through the DLR runtime | MobileNetV2 2.93 ms, ssdLite mobDet 320 12.97 ms, deeplabv3lite cocoseg21 8.85 ms; all 14 zoo models have a C7x number; the C7x against the A72s is 13x to 135x | **TVM** and **DLR** are a model compiler and its runtime, a third way TI's model zoo (the ready-made models on the image) ships models besides ONNX and TFLite (two model file formats). The mobDet is twice its TFLite build (6.40 ms), cause not tested apart (FAQ); [the C7x against the A72s](../docs/experiments/2026-10-10-tda4-c7x-vs-a72.md) | measured | `dl_inference_ms` |
| Whole pose graph, file to encoded file | 17.8 fps | the seconds-long `left-1280x768` clip, the app's own metric, where start-up weighs on the average; the same graph on a looping clip holds that clip's 30 fps (next row) | measured | `gst_pipeline_fps` file-pose-encode |
| Pose graph CPU share, looping `video0` clip | 55 percent of the two A72s at the clip's 30 fps | 36.5 ms of A72 time per frame; the app's own figures: total 33.4 ms, inference 19.0 ms per frame; 5 minute run | measured | `gst_pipeline_fps` video0 |
| [Zero-copy decode into the C7x](../docs/experiments/2026-10-10-tda4-zero-copy-decode.md) | 110.8 fps at 33 percent of one A72 | TI's `capture-io-mode=5 ! tiovxmemalloc` path; without it the A72 copies every frame: 56.2 fps at 65 percent. Decode alone about 500 fps | measured | `gst_pipeline_fps` zerocopy, plain |
| TI's demo configs, headless | 8 of the 15 shipped configs run for the full 30 s with no error | the display sink swapped for a file sink and a 20-copy clip (a sweep script run on the board). The other 7 need hardware the bench lacks: the imx219, imx390 and ov5640 camera configs, two configs whose logs end with `" /dev/video-usb-cam0 " doesn't exist`, and the http and rtsp configs, which exit clean with no frames. The bench has no camera and no network (FAQ). "No error" is not a rate: image_classification logged 1.0 fps | measured | `examples_ran_clean` |
| Hardware H.264 decode, 1280x768 clip | about 450 fps aggregate over one to four streams | pipeline start-up inside the time, so a lower bound; not a 1080p figure | measured | `video_decode_fps` |
| Hardware H.264 encode | 720p 131 fps | bounded by the software test source; 1080p60 caps refused. FAQ | measured | `video_encode_fps` |

### CPU, memory and crypto

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| CoreMark, one and two A72 threads | 9.8k and 19.6k iterations per second | validated, three runs each | measured | `coremark` |
| Dhrystone 2.1, one A72, by the benchmark's rules | 4.83 DMIPS per MHz, 9,666 DMIPS at 2.0 GHz | three runs; 117.8 cycles per Dhrystone counted by perf (1,757 Dhrystones per second is 1 DMIPS). At -Os: 3.09. With inlining allowed, which the rules forbid and vendor tables often quote: 5.26 | measured | `dhrystone` |
| TI's Dhrystone figure for the TDA4VM | works out to 6.0 per MHz per core | TI's TDA4VM data sheet, read 2026-10-09, figure for the pair of cores | claim | TI data sheet |
| Memory bandwidth, one A72 | copy 5.85 GB/s, triad 4.79 GB/s | decimal units | measured | `mem_bandwidth` |
| openssl on one A72 | AES-128-CBC 1.49 GB/s, AES-128-GCM 1.56 GB/s | Arm crypto extension; the kernel crypto engine route did not bind, a kernel option is off. FAQ | measured | `openssl_speed_mbps` |
| [SA2UL hardware crypto](../docs/experiments/2026-10-10-tda4-sa2ul-crypto.md) from user space | AES-128-CBC 185 MB/s at 16 kB blocks, 886 MB/s at 64 kB; SHA-256 165 and 837 MB/s | **SA2UL** is the chip's hardware crypto block, reachable only through `/dev/crypto` with this repo's program (a small C program run on the board). The A72's crypto instructions beat it at every size (1,490 and 969 MB/s), and the ioctl copies cost as much CPU as doing the work on the core | measured | `openssl_speed_mbps` sa2ul |

Not run: Cortex-R5F CoreMark (`coremark` r5f). Three of the six R5F cores are free, but no J721E RTOS SDK is installed on any machine here. Install TI's MCU+ SDK for J721E and build its CoreMark example for an R5F (FAQ).

### Sensor bridge and sustained load

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| [Holoscan Sensor Bridge](../docs/experiments/2026-10-10-tda4-holoscan-sensor-bridge.md) device emulation (NVIDIA's emulator on the A72s, build in [hsb/README.md](hsb/README.md)) | host-side checker saw 200 of 200 frames intact at 720p 10 fps; over a real 1 GbE link to the checker on a Mac, 720p arrived 300 of 300 intact at 30 fps with the sender on 21.9 percent of one A72 (11.1 µs per packet) | at 30 fps the Python checker drops packets, FAQ; on loopback the same sender saturates one A72 near 42 fps at 1080p (15.8 µs per packet, suspected loopback overhead), and with a 9,000-byte MTU it holds 1080p60 on 37 percent of a core, FAQ | measured | `sensor_bridge_*` |
| Sustained load | the board reset about 60 s into a 2-thread CoreMark plus TIDL loop; hottest zone rose from 89.6 to 92.6 C | last six samples 89.6, 90.7, 91.7, 92.6, 91.9, 92.6; the reset came about 4 s after the last reading. FAQ | measured | `thermal_steady_c` |

## Running the standard tests over the console

Everything in the tables above ran from the serial prompt with the image's own tools (gcc 13.3, Python 3.12, GStreamer, perf). Scripts were pushed with `tools/push_serial.py`, each wrote its log under `/root/` on the board, and the logs were pulled back with `tools/pull_tar.py`. How each number was measured:

| Test | How it is measured |
|---|---|
| Dhrystone | sources from `mspm0l1306/bench/dhrystone/src`, built on the board; the score is perf's cycle count per Dhrystone, because the program's own clock has 1 s resolution and `-DTIMES` does not compile against glibc |
| TVM zoo models | `bench/zoo/tvm_bench.py`: `dlr.DLRModel` on each `TVM-*` folder's `artifacts`, input shape from its `param.yaml`, 30 timed runs |
| Demo sweep | every config in `/opt/edgeai-gst-apps/configs` for 30 s, `kmssink` swapped for a file sink, `.h264` input swapped for the 20-copy clip |
| Pose graph CPU | `/proc/stat` over a 50 s window 30 s into the run; uses the `video0` clip, because the `left` clip ends the pipeline at its first end |
| Zero copy | TI's pipeline shape from the app's `-v` print, with and without `capture-io-mode=5 ! tiovxmemalloc`, `fpsdisplaysink` for the rate, `/usr/bin/time` for the CPU |
| SA2UL crypto | `/dev/crypto` for AES, an AF_ALG hash socket for SHA-256, the process's own CPU time |
| Warm boot | `tools/warm_boot.py` types `reboot`, stamps the first `U-Boot SPL` byte and the login prompt on the Mac's clock |

Heavy runs wait until every thermal zone is under 80 C and stop at 90 C. The board idles at 72 to 76 C when cold and 77 to 80 C just after a load, in a warm room with its passive heatsink. A 20 s pose run took the hottest zone to 81.1 C; a five-minute pose run took it to 91.5 C with no reset; under a heavier load it once reset about 4 s after a 92.6 C reading, cause not found ([FAQ](FAQ.md)).

## Serial toolbox (the board had no network)

Everything above went through the console cable at 115200. The helpers are in `tools/`; their traps are in the FAQ.

| Tool | Does | Trap it avoids |
|---|---|---|
| `tools/conlog.py <port> <log>` | timestamped console logger, one per port | finding which of the four ports talks |
| `tools/lcmd.py "<cmd>" ...` | one-shot commands at the Linux prompt, `LCMD_T=<s>` sets the wait | it sends Ctrl-C on connect, so start long jobs with `setsid nohup ... &` |
| `tools/ucmd.py "<cmd>" ...` | the same at the U-Boot prompt | prompt detection |
| `tools/push_serial.py` | push a file as 76-column base64 with a hash check | a single long line hangs the console |
| `tools/pull_tar.py <remote> <local.tgz>` | pull a directory as a hash-checked tar | lost or garbled output on long reads |
| `tools/uart_boot.py` | UART-boot U-Boot: xmodem then ymodem on the right ports | the ROM port is not the console port; ymodem's retry phase, where U-Boot sends `C` and waits for the sender to answer |
| `tools/write_from_stick.py unzip` | write the card from gzip chunks on a USB stick | no card reader |
| `tools/prep_stick.sh diskN` | format a spare stick and copy the chunks (erases it) | |
| `tools/write-card.sh diskN` | TI's route from a terminal, with a reader | |

## Files

- `hsb/`: the board as a Holoscan Sensor Bridge device (NVIDIA's emulator, this repo's build files and host-side checker). `bench/zoo/`: the model benchmark.
- `data/results/tda4.csv`: the numbers. The sensor-bridge experiment: [docs/experiments/2026-10-10-tda4-holoscan-sensor-bridge.md](../docs/experiments/2026-10-10-tda4-holoscan-sensor-bridge.md).

## If you see this

- If you see `Direct firmware load for j7-mcu-r5f0_1-fw failed with error -2` (and the same for r5f1_0 and r5f1_1) in the boot log, ignore it: TI's image ships no firmware for three of the six R5F cores, and the board boots to the prompt regardless ([FAQ](FAQ.md#the-boot-log-shows-red-failed-lines-and-firmware-errors)).
- If you see `Retry 0: Got ...` lines during the first UART-boot transfer, let it run: it is normal and the transfer completes ([FAQ](FAQ.md#uart-boot-prints-retry-0-got--lines-during-the-first-transfer)).
- If you see no serial port on the Mac, install the CP210x VCP driver, click Allow in System Settings, Privacy and Security, then unplug and replug the cable ([FAQ](FAQ.md#why-does-the-mac-show-no-serial-port-when-i-plug-in-the-micro-b-cable)). If the console shows nothing after the board was already running, open the console first, then power the board, or press SW2 (reset) with the console open ([FAQ](FAQ.md#i-opened-the-console-after-the-board-was-already-running-and-see-nothing-even-after-pressing-enter)).

The rest: [FAQ](FAQ.md).

## Limits

- No Ethernet at the bench: the sensor bridge ran on loopback and one 1 GbE link to a Mac checker, not against NVIDIA's own receiver, and there are no network rows.
- No display, camera or power meter: no GPU, camera or power rows.
- Sustained load ran without a fan on a supply of unknown rating (suspected to be an Apple 65 W USB-C charger, not confirmed).
- No J721E RTOS SDK is installed, so there is no R5F CoreMark row. Only TI's prebuilt zoo models were run; compiling your own models for the C7x needs TI's edgeai-tidl-tools on an x86 Linux machine, which was not used.

## Where these facts come from

Checked 2026-10-10. Mac: Intel, macOS 15.7.9, lrzsz 0.13.1, pyserial 3.5, xz 5.8.4; board: TI Edge AI SDK 11.00.00.08, Linux 6.12.17-ti, gcc 13.3. The board runs TI's Edge AI Linux (Processor SDK Linux Edge AI 11.00.00.08) from the kit's own micro-SD card, with a root login on the serial console. Bench limits that shaped everything here: no card reader, no Ethernet, no display. The card in the box booted nothing, so it is inferred to have been blank (it was not read first), and it was written from the board itself: U-Boot loaded over the console cable, the image read from a USB stick. 62 counted minutes from the first console check to the Linux login, 11 of them hands-on and 51 waiting, idle gaps left out (66 by the clock); the serial driver install the evening before is not included.

Files: `data/results/tda4.csv`, `tda4/RESULTS-tda4.md`, `tda4/AGENTS.md`, `tda4/FAQ.md`.
