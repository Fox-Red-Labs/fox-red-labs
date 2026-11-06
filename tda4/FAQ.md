# What goes wrong with the TI SK-TDA4VM starter kit on a Mac, and how do I fix it?

The problems hit, and a reader could hit, with the TI SK-TDA4VM starter kit (TDA4VM chip, J721EXSKG01EVM) on an Intel Mac, macOS 15.7.9, with the board reached through its serial console. Each answer starts with the fix and says what was observed.

**What caused it**, one tag per answer:

| Tag | Meaning |
|---|---|
| Our mistake | A mistake in this repo's own run or script. Listed so you do not repeat it |
| Setup or handling | How the hardware was plugged in or set up |
| Tool or docs | A TI tool limit, a third-party driver, or a missing or wrong instruction |
| Chip limit | The silicon rules it out |
| Third-party behavior | macOS, NVIDIA's code or upstream software, not TI |
| Unknown | Not found |

Contents: [Getting a console](#getting-a-console), [The blank card and the first boot](#the-blank-card-and-the-first-boot), [Working over the serial cable](#working-over-the-serial-cable), [Running things on the board](#running-things-on-the-board), [The sensor-bridge emulator](#the-sensor-bridge-emulator).

Words used below (more in [docs/vocab.md](../docs/vocab.md)): the **console** is the Linux text terminal the board puts on its micro-B serial port. **U-Boot** is the bootloader that runs before Linux. **UART boot** is the chip's built-in way of taking its bootloader over the serial port instead of from a card. **MTU** is the largest packet a network link carries.

## Getting a console

### Why does the Mac show no serial port when I plug in the micro-B cable?

Install the CP210x VCP driver, click Allow in System Settings, Privacy and Security, then unplug and replug the cable. The board's serial bridge is a CP2108 with four ports. Suspected, not checked: macOS's built-in driver covers only the single-port CP2102. Observed: no ports until the vendor driver was installed and allowed. After the Allow click the ports appear as `/dev/cu.SLAB_USBtoUART`, `...1`, `...2` and `...4`.

### Which of the four ports is the Linux console?

On this Mac, `/dev/cu.SLAB_USBtoUART1`. TI's user guide lists the ports in the order WKUP_UART0, MCU_UART0, UART0 (the console, the same port the README calls MAIN_UART0), UART1, which suggests the third name. The Mac numbers them differently: the console was the second name and the chip's UART-boot port (MCU_UART0) was the third, `...UART2`. If in doubt, log all four with `tools/conlog.py` and see which one talks. 115200, 8 data bits, no parity, one stop bit; login `root`, no password.

### I opened the console after the board was already running and see nothing, even after pressing Enter

Open the console first, then power the board. Connecting after boot can show nothing at all; TI's own forum answer for this kit says the same. If the board is already up, press SW2 (reset) with the console open.

### The driver installed but System Settings never showed an Allow prompt

Open System Settings, Privacy and Security, and scroll to the bottom; the prompt sits there as a "System software ... was blocked" notice that names the driver. After the click `systemextensionsctl list` still said "activated waiting for user" on this Mac, and the ports worked anyway, suspected: the older kernel driver in the same package is what loads.

## The blank card and the first boot

### Where is the image, and is there a checksum?

On TI's SK-TDA4VM software page under Processor SDK Linux Edge AI 11.00.00.08, file `tisdk-edgeai-image-j721e-evm.wic.xz`, 1.1 GB, no login needed. The page labels a SHA-256 but shows no value; check the download with `xz -t` and record your own hash. The copy downloaded here: 1.12 GB (1,123,540,588 is the exact byte count on the download page), SHA-256 `7bebcff62930c93bc0d10462385a936305ffdfd2457c4bbd85c2b2f216076da0`.

### Is the card in every kit blank, or was the one in this kit unusual?

Unknown for other kits. The card in this kit was silent and booted once written, so it is inferred to have been blank; TI's getting-started page tells every reader to write the image and nothing says a preloaded card exists, so expect to write it.

### What size USB stick, and do my files on it survive?

4 GB or more; the chunks are 1.95 GB. Your files survive: the chunks go into a `tda4/` folder beside them and the stick is not reformatted. `tools/prep_stick.sh` is only for a spare stick you want erased.

### The write was interrupted. Is the card damaged?

Probably not; a write was not interrupted to test it. Run `tools/write_from_stick.py unzip` again from U-Boot; every chunk is written to a fixed block address, so a rerun overwrites the same places. The card is only read by Linux once SW1 is back to all OFF and the board reboots.

### Can this be done from Windows or Linux instead of a Mac?

Everything here was done from a Mac. The tools are Python with pyserial and `lrzsz`, so a Linux machine needs the same packages and should show the console without a driver.

### The board is powered, all four ports are silent, and a reset changes nothing

Check SW1 is all OFF and that you are on the console port, then write TI's image to the micro-SD card. The board was silent with the card from the box in the slot and booted after the write; the card is inferred to have been blank, since it was not read first. The user guide says a card is included and nothing says it is empty; the Edge AI getting-started page tells you to write the image but does not say the included card needs it. Suspected, from TI's documentation and not tested here: the boot ROM prints nothing when it finds no image on the card, so silence is what a blank card looks like, and also what a wrong boot switch or the wrong port looks like. Observed: silence, then a boot after the write. The image is `tisdk-edgeai-image-j721e-evm.wic.xz` from the SK-TDA4VM software download page; it downloaded with no login.

### I have no card reader. Can the card still be written?

Yes, from the board itself: UART-boot U-Boot over the console cable, then write the image from a USB stick. No reader at the bench; worked around. Set SW1 to 1 ON, 2 OFF, 3 ON (UART boot), run `tools/uart_boot.py`, power on. It sends the four boot files (xmodem, then ymodem) in about 3.5 minutes and stops at the U-Boot prompt. Put the image on a FAT32 stick as gzip chunks of 256 MiB (`README.md`, "No-reader path"), stick into a USB-A port, run `tools/write_from_stick.py unzip`: 323 s for the whole card. Then SW1 all OFF and power cycle.

### UART boot stalled at the third file and the board needed a reset

Use `tools/uart_boot.py`, which accepts either byte as "ready". U-Boot sends a run of "C" bytes and then NAK bytes. A detector that waits for a clean run of "C" never fires once the NAKs start, and after about a minute and a half SPL stops waiting (`spl: ymodem err - Timed out` on the console).

### Why not use U-Boot's USB mass storage command to expose the card to the Mac?

Because it crashes U-Boot on this kit; use the stick. Both USB controllers come up as host-only in TI's U-Boot, `ums 0 mmc 1` answers "No USB device found" and "Couldn't init USB controller", and rebinding the controller to the gadget driver followed by `ums` takes a synchronous abort and resets the chip (observed 2026-10-09, U-Boot text not saved). In UART-boot mode that means loading U-Boot again.

### How long is the first boot, and why is the second shorter?

27.7 s from the first bootloader byte to the login prompt on the first boot; 24.0 to 24.5 s on the three warm boots after it. Not a fault; suspected: the first-boot root file system resize. The first boot grows the root file system to the whole card, which ran from about 22 s to 26.7 s after the first byte and accounts for most of the difference (suspected: the resize window is 4.7 s, the gap 3.2 to 3.7 s). From the `reboot` command a warm boot is 27.9 to 28.3 s, the extra being, suspected, the shutdown and the ROM's card read before the first byte. TI's getting-started page says "less than 20sec", but that is to a wallpaper on a display, a different end point from the login prompt measured here, and it was not timed.

### Why is there no CoreMark number for the R5F cores?

No J721E RTOS SDK is installed on any machine here, so nothing could be built for them. Three of the six R5F cores are free (the image ships no firmware for them). To fill the row: install TI's MCU+ SDK for J721E on a Linux or Windows machine, build its CoreMark example for an R5F, and load it through `remoteproc` from Linux on the board.

### The boot log shows red FAILED lines and firmware errors

Ignore them on a bench with no display: `Failed to start Start psplash boot splash screen` is, suspected, the splash screen with nowhere to draw, and `Direct firmware load for j7-mcu-r5f0_1-fw failed with error -2` (and the same for r5f1_0 and r5f1_1) is TI's image shipping no firmware for three of the six R5F cores. The board boots to the prompt regardless.

### UART boot prints "Retry 0: Got ..." lines during the first transfer

Normal; let it run. Suspected: the xmodem sender reports every byte that is not the acknowledgement it expects while the ROM is still printing its own text (observed on 2026-10-09, not saved). The transfer completes.

## Working over the serial cable

### The first UART-boot script waited two minutes and saw no "C" at all

Use `tools/uart_boot.py`, which holds the ports open. Reading each port from a shell loop that opens and closes the device on every poll fails: macOS appears to reset the port to 9600 baud on close, so the ROM's 115200-baud stream never decodes. Holding the ports open worked.

### My long-running job on the board died when I ran the next command

Start anything long with `setsid nohup <command> > log 2>&1 &`. The console helper sends Ctrl-C when it connects, to clear a stuck line. A job started with plain `nohup ... &` from the console shell shares its process group and takes that Ctrl-C; jobs started with `setsid` survived every connect. `setsid` puts the job in its own session.

### My script sat idle for 25 minutes after starting some background pipelines

Write `wait $pid` for each pipeline, never a bare `wait`. A bare `wait` also waits for a background logger loop that never exits.

### A file pushed over the console arrived empty or the shell got stuck at a ">" prompt

Wrap base64 at 76 columns before pushing (`base64 -b 76` on macOS). In this case a 107 kB single line overran the console's line buffer, the heredoc never saw its end marker, and the shell waited at the continuation prompt. `tools/push_serial.py` and `tools/pull_tar.py` do the wrapping, the chunking and a hash check.

### A loop that waits for a marker in a file on the board never ends

Strip carriage returns from console output before comparing (`tr -d '\r'`). The console sends `1\r\n`; `grep -x 1` does not match it.

### Which program's CPU number do I trust, /proc ticks or perf?

perf. Suspected: tick accounting misses a sender that sleeps until a timer tick and then works; `perf stat -e task-clock -p <pid> sleep 10` read 1.9 times higher than `/proc/<pid>/stat` on the same stream (523 ticks against 9,814 ms over 10 s, and the tick rows in `data/results/tda4.csv`). perf is on the Edge AI image and works on the A72.

## Running things on the board

### The pose app prints frame rates and then exits with "[ERROR] Error pulling frame from GST Pipeline"

That is the end of the clip; the sample videos are seconds long. In these runs the config's `loop: True` did not loop a raw `.h264` file (suspected: the loop applies to container formats; not checked). For a longer run, concatenate the clip to itself: `for i in $(seq 20); do cat clip.h264; done > /tmp/long.h264`, and point the config at it.

### The board rebooted on its own during a heavy run

Cause not found. Suspected: a thermal trip or the supply collapsing under load. With a 2-thread CoreMark loop and a TIDL inference loop at once, the hottest zone over the last six samples (10 s apart, the last one 4 s after the one before) read 89.6, 90.7, 91.7, 92.6, 91.9 and 92.6 C and the board reset the next moment; no log of the reset survived. The user guide warns that a weak USB-C supply resets the board under load; the supply is an Apple USB-C charger recalled as 65 W (label not read; Apple's 61, 67 and 70 W adapters all offer 20 V at 3 A or more, per Apple's 61 W user guide and published tests of the other two), which meets TI's 20 V, 60 W class if that recollection is right, so heat is the stronger suspect here. The kit has a heatsink and no fan. Not done: a rerun with a fan, the label read to confirm the supply, and the kernel's thermal trip points recorded.

### `kmscube` says "failed to initialize legacy DRM"

It needs a display, and the bench has none, so there are no GPU rows.

### `openssl speed -engine afalg` fails with "socket bind failed"

Use the CPU's crypto instructions for bulk data, or drive the hardware block yourself through `/dev/crypto` with a small C program. The full line is `ALG_PERR: /usr/src/debug/openssl/3.2.4/engines/e_afalg.c(463): Failed to bind socket : No such file or directory`. The cause is the kernel's configuration, not the chip: `/proc/config.gz` has `CONFIG_CRYPTO_USER_API_HASH=m` but `CRYPTO_USER_API_SKCIPHER` off, so an AF_ALG socket can bind a hash or the random number generator and no cipher. The SA2UL drivers are loaded and preferred (`cbc-aes-sa2ul` at priority 30,000 in `/proc/crypto`), and the image loads the `cryptodev` module with `/dev/crypto`, but this openssl has no engine for it. Measured through `/dev/crypto`, the hardware AES is slower than the A72's own instructions at every block size (185 against 1,490 MB/s at 16 kB) and saves no CPU time per byte through that path, so through this path the CPU was faster at every block size measured ([the experiment](../docs/experiments/2026-10-10-tda4-sa2ul-crypto.md)).

### CoreMark will not compile: "'FLAGS_STR' undeclared"

Build with `-DFLAGS_STR='"-O3"'` and include EEMBC's `core_portme_posix_overrides.h`. The posix port expects the makefile to pass the flags string; the first build pushed the port files without the overrides header and without the define.

### A Python receiver drops packets at 20,000 packets a second

Raise `net.core.rmem_max` (128 MB was used) and run slower; the kernel's "receive buffer errors" counter tells you when it is the receiver. The kernel's default socket buffer is too small for the burst. The receiver held 10 fps at 720p (200 of 200 frames intact) after the change; at 30 fps with the default buffer it dropped 11 percent of packets and 0 of 300 frames arrived intact. The counter rose while frames broke.

### The 1080p H.264 encode pipeline exits at once with status 1

Add `h264parse` after `v4l2h264enc` and send the stream to a file sink; with that, 1080p at 30 and 60 fps caps ran (42 fps, bounded by the software test source). The failing run was `videotestsrc ! NV12 1920x1080 at 60/1 ! v4l2h264enc ! fakesink`, which returned 1 in 0.14 s with no log captured; the working runs changed two things at once, so the cause is not isolated.

### The sensor-bridge sender falls short of 60 fps at 1080p

Raise the MTU to 9000 where every card and switch on the link allows it: on loopback the sender then held 1080p60 on 37 percent of one A72, while at the normal 1.5 kB MTU it saturated one A72 near 42 fps. NVIDIA's emulator sends each data plane from one thread (`DataPlane` in `third-party/holoscan-sensor-bridge`). On a real 1 GbE link the cost per 1.4 kB packet was 11.1 µs (720p30, 21.9 percent of a core, 300 of 300 frames intact); on loopback 15.8 µs, suspected because loopback does the receive work inside the send call. At 1080p30 over the wire the sender averaged about 21.5 fps on 37 percent of a core, cause not found. Jumbo frames were not run on the wire: the switch on the bench runs 1.5 kB.

### The pose graph on the `left-1280x768.h264` clip stops after one copy even when the file holds twenty

Use `video0_1280_768.h264` for anything that has to run longer than a few seconds. Twenty copies of `left-1280x768.h264` concatenated play as one clip and end the pipeline at the first copy's end (14 s, 56 frame-rate lines), while twenty copies of `video0_1280_768.h264` play through (300 frames each, 8,987 lines over five minutes). Suspected, not checked: the two sample clips differ in how they begin (stream headers or key frames); neither TI document says which to use for a long run. The pose CPU test builds its long clip from `video0`.

### `gst-launch-1.0 -q ... fpsdisplaysink` prints no frame rate

Run with `-v` instead of `-q`, and read the `rendered: N, dropped: M, current: X, average: Y` lines. Observed: with `-q` no rate line appeared and with `-v` the lines did. Suspected: `-q` hides the element messages `fpsdisplaysink` reports through. The zero-copy test script does this; its first version with `-q` had the CPU numbers and no rate.

### Dhrystone will not compile with `-DTIMES`: "conflicting types for 'times'"

Build with `-DTIME` and take the score from `perf stat` cycles. `dhry_1.c` declares `extern int times();`, which clashes with glibc's prototype in `sys/times.h`; the sources stay unmodified by the benchmark's rules, so the 1-second `time()` clock is the one that builds. Its resolution is too coarse for a 6 s run (it quantizes 100 million runs to 20.0 or 16.7 million a second), so the Dhrystone script reports cycles per Dhrystone from perf and DMIPS per MHz as 1e6 divided by (cycles per Dhrystone times 1,757; 1,757 is the Dhrystones per second that count as one DMIPS, the VAX 11/780 reference), which needs no clock at all.

### Where is the model zoo, and what is the first command to run a model on the C7x over the serial console?

`/opt/model_zoo`, one folder per model, each with its `artifacts` folder (the compiled C7x form) and a `param.yaml`. The layout was learned by listing folders; TI's getting-started page assumes a display and Ethernet. A working first command for the TVM models is `python3 bench/zoo/tvm_bench.py` pushed to the board; for the ONNX models, onnxruntime with the `TIDLExecutionProvider` pointed at the folder's `artifacts`. The check is the runtime's log listing every node on the C7x (274 of 274 for the pose model).

### ONNX Runtime on the CPU warns that an output shape does not match on the detection models

The run timed only and did not compare outputs. The line is `execution_frame.cc:857 VerifyOutputSizes] Expected shape from model of {51} does not match actual shape of {200} for output labels`. It appears on the detection models when they run on the two A72 cores with the CPU provider; whether the output is wrong or the model's declared shape is loose was not checked.

### My model takes hundreds of milliseconds to seconds per inference on the SK-TDA4VM

It is probably running on the CPU, not the C7x. Check that the runtime's log lists every node on the C7x (TI's pose model shows 274 of 274 with the TIDL provider), and load the zoo model's precompiled `artifacts` folder, not the bare ONNX file. On this image the zoo models take 2.2 to 13.8 ms on the C7x and 37 to 1,767 ms on the two A72 cores ([the C7x against the A72s](../docs/experiments/2026-10-10-tda4-c7x-vs-a72.md)).

### My GStreamer pipeline into TIDL runs far below the model's rate, and one A72 is nearly full

Add `capture-io-mode=5` on `v4l2h264dec` and `tiovxmemalloc pool-size=12` right after it, the two settings TI's own pipelines carry. TI describes the A72 as copying each decoded frame without them; the copy itself was not profiled. Measured here: 110.8 fps at 33 percent of a core with them, 56.2 fps at 65 percent without ([zero-copy decode](../docs/experiments/2026-10-10-tda4-zero-copy-decode.md)).

### The TVM/DLR build of a zoo model is slower than the TFLite build of the same network

ssdLite mobDet 320 measured 12.97 ms through TVM and DLR against 6.40 ms through TFLite, both on the C7x. The runtime and the input format (fp32 NHWC against uint8) both differ between the two builds and were not tested apart ([the C7x against the A72s](../docs/experiments/2026-10-10-tda4-c7x-vs-a72.md)).

### Which of TI's demo configs run on a bare bench, and why do the others exit?

Eight of the fifteen configs in `/opt/edgeai-gst-apps/configs` run with no camera and no network once the display sink is swapped for a file sink: display_and_encode, face_detection, image_classification, keypoint_detection, object_detection, remote_display, semantic_segmentation and single_input_multi_infer. The imx219, imx390 and ov5640 configs exit with status 1 in about 2 s with no camera attached; multi_input_multi_infer and shared_model_instance exit the same way with `" /dev/video-usb-cam0 " doesn't exist` in their logs; http_src_example and rtsp_src_example exit with status 0 after 8 to 10 s and an empty output file with no network. Not a fault count: five misses show a missing-camera message, and the two network-source configs exit clean with no frames and no reason in their logs (suspected: no network at the bench).

### A heavy run drove the zones past 90 C

Stop it and let the board fall under 80 C before the next one. The board idled at 63 to 67 C at night and 73 to 77 C by day, and sits at 77 to 80 C just after a load; a 20 s pose run took the hottest zone to 81.1 C, the five-minute pose run took it to 91.5 C with no reset, and the board once reset about 4 s after a 92.6 C reading under a heavier load, cause not found. The zero-copy test script carries a guard: it waits for every zone under 80 C before a run and kills the run at 90 C. The bench has no fan, and the supply's label is unread.

### `kill` does not stop an app started under `/usr/bin/time`

Signal the app's own pid, not the wrapper's: `pgrep -x python3` with the pattern, or `$!` of the process you started. `pgrep -f "app_edgeai.py ..."` matches the `/usr/bin/time -v python3 app_edgeai.py ...` wrapper too, and `head -1` picked it; the app kept running for five minutes at full load after the signal went to the wrapper (suspected: `time` does not pass it on).

### 1080p decode fails with `v4l2h264dec: Failed to allocate required memory` and `Buffer pool activation failed`

Drop `capture-io-mode=5 ! tiovxmemalloc` at 1080p; the plain decoder then runs. On the 1080p clip used here the zero-copy pipeline fails within 0.2 s at `tiovxmemalloc` pool sizes 4, 8 and 12 alike, while the same pipeline decodes 1280x768 at about 500 frames a second. Without it, one 1080p stream decodes at 66.7 fps and two or four streams at about 90 fps in total, below TI's rating of four 1080p60 streams. The cause is unknown: the same plain path at 1280x768 reaches about 2.4 times that pixel rate, so the buffers alone do not explain it, and the 1080p clip is the output of this repo's own encoder, not TI's.

### cyclictest shows a maximum of 41 µs idle and 59 µs under load. Is this a real-time kernel?

No. The Edge AI image's kernel is 6.12.17-ti without PREEMPT_RT. One SCHED_FIFO thread at a 1 ms period on core 1 saw an average of 4 µs and a maximum of 41 µs over 60 s idle, 59 µs with `stress-ng --cpu 2` on both cores. A product that needs a bound below that is expected to need a PREEMPT_RT build; none was tried here, and whether TI's SDK offers one was not checked.

### Why does Linux see only 2,170 MiB of the 4 GiB?

Because the rest is reserved before Linux starts, by design. `dmesg` lists the reserved regions: 624 MiB for the vision firmware's heap, 512 MiB for the CMA pool the codecs and TIDL allocate from, 24 plus 8 MiB for the R5F's virtual Ethernet, and the remaining about 758 MiB is not itemized there (suspected: the remote cores' firmware regions and the kernel's own reservations). `/proc/meminfo` shows MemTotal 2,222,672 kB.

### CoreMark scores higher at gcc -O2 than at -O3 on this board

Build at -O2, or at `-O3 -mcpu=cortex-a72 -funroll-loops` for the best score found. With the EEMBC files in this repo and the image's gcc 13.3, -O2 reads 10,466 and -O3 9,832, both validated, three runs each; adding `-mcpu=cortex-a72` gives 10,450 and 10,334, and `-O3 -mcpu=cortex-a72 -funroll-loops` 10,782. Quote the flags with any CoreMark score from this board.

### The board reset on its own at idle

Confirm the supply's label and find a way to log the reset cause; a fan has not been tried as a fix. Keep results on the card so a reset costs only the run in flight. Once, after 25 minutes idle with the zones at 74 to 77 C and only a serial connection open, the console came back in U-Boot; no command asked for a reboot. The supply was a 65 W USB-C charger; a weak supply is a less likely cause, and heat at 74 to 77 C idle is below the load reset's 92.6 C. Nothing on the image logs the reset cause.

### TI's perf_stats and /proc disagree about the A72 load, in opposite directions on two pipelines

Use `perf stat` task-clock for A72 time, with `/proc/stat` deltas as the whole-system check (ticks can under-read a process that sleeps between them, see the entry above); use perf_stats for the C7x, the scalers and the DDR, where it agrees with the timings here. Beside the zero-copy yolox pipeline TI's `mpu1_0` counter read 17.5 to 19.5 percent while `/usr/bin/time` and `/proc/stat` read 33 percent of one core (1.7 to 1.9 times lower); beside the pose app it read 68 against 55 percent of two cores (1.2 times higher). The cause was not found.

### Why is the message round trip measured on only one remote core?

Because only the MCU R5F0_0 firmware answers `rpmsg_char_simple`'s echo (endpoint 14, 74 µs round trip). The vision firmware on the main-domain R5F pair, the two C66x and the C7x exposes endpoint 13 for TI's IPC library and fails or times out the echo test at endpoints 13 and 14. TI's `vx_app_arm_ipc.out` echoes to every core and passes but prints no timing; a timed version needs TI's `app_ipc` API from the vision-apps SDK, not on the image.

### Can custom models run on the C7x?

Not on this bench. Compiling a model for TIDL needs TI's `edgeai-tidl-tools` on an x86 Linux machine with the version pair for the board's image (11.00.00.08); no machine here runs it. The plain-C versions run on the A72.

### Where are the power numbers?

Nowhere: the bench has no meter. Power at the wall, energy per inference and per CoreMark iteration are `waiting` rows until a USB-C power meter sits between the supply and J14.

### Why is Dhrystone 4.83 per MHz when TI quotes 24K DMIPS for the pair?

Part of the gap is inlining, which the benchmark's rules forbid (5.26 per MHz with it allowed); the rest to TI's 6.0 per MHz per core is unexplained. With inlining allowed the build reads 5.26, so inlining is part of the gap and not all of it.

### I ran two copies of the same pipeline and got one frame-rate file

Give each copy its own output file name (a counter or the shell's `$$`), not a name built from a property both copies share. Naming each copy's output after its model folder makes two yolox-nano copies write the same file, and only one line survives.

### How do I know the A72 clock when cpufreq does not show it?

Count cycles against time with `perf stat -e cycles,task-clock` on any program: cycles over task-clock read 2.000 GHz on every run here that counted them. This image exposes no `/sys/devices/system/cpu/cpu0/cpufreq`, so the frequency is not readable directly.

### Why are there no camera or Embench numbers?

Because the bench had no camera and no time for a slow transfer. Ethernet came late on 2026-10-10, and its throughput and the sensor-bridge wire test are now measured; the camera rows need a webcam or a CSI camera; Embench and CoreMark-PRO were not reached and their sources would have to go over the serial cable at about 10 kB/s. Each is a `waiting` or `not_run` row with its reason in `data/results/tda4.csv`.

### Are these the best numbers the TDA4VM can do?

No. They are what TI's starter kit does with TI's demo image and simple tools: a micro-SD card instead of eMMC, a passive heatsink with no fan, a supply of unknown rating, a stock kernel, the image's gcc, plain C where assembly paths exist, and Python callers for the models. The results page lists each limit and where the chip could do better (`RESULTS-tda4.md`, "A starter kit, not the chip's ceiling").

### Why do two or three models at once give 1.5 to 1.95 times the frames of one, not two or three times?

Because something they share is filling up; which one was not profiled. One yolox-nano pipeline ran at 110.8 fps with the C7x 45 percent busy; two copies ran at 91.2 each (182.4 in all) and three at 72.1 each (216.4); yolox-nano with ssd-lite ran at 81.7 each.

### How fast does the hardware encoder go at 1080p?

At least 53.7 fps here, which is below TI's rating of one 1080p60 stream but is a lower bound. The input was a hardware decode of the 1080p clip made here, which alone runs at 66.7 fps, so the decoder feeding it limits the figure; with the software test pattern as input the limit was the pattern generator at 42 fps.

### Where do I buy one, and what does it cost?

From TI (ti.com/tool/SK-TDA4VM). The price was not shown to a reader who is not logged in to TI.com on 2026-10-09. The power supply, a camera and a Wi-Fi card are not in the box; TI recommends a 20 V USB-C supply of the 60 W class.

### Why does the pose graph read 17.8 fps in one place and 30 in another?

They are two runs on two clips. The first ran TI's short `left-1280x768.h264`, which ends the pipeline after 14 s, and read 17.8 to 18.1 fps at 55 to 56 ms a frame. The later run used 20 copies of `video0_1280_768.h264` and held the clip's own 30 fps for five minutes, 33.4 ms a frame by the app's counter. The second is the one to quote; why the first was slower was not isolated.

### Why does the board reach only 5.4 GB/s when its DDR is rated 17.1 GB/s?

The two numbers measure different things. 17.1 GB/s is the bus rate (4,266 MT/s on 32 bits, data sheet). 5.4 GB/s triad and 6.3 GB/s copy are what two CPU threads reached with simple loops; the accelerators and the DMA engines use the same DDR, and TI's `perf_stats` saw 4.3 GB/s moving during the pose app.

### Why does a load from DDR take 91 ns in one test and 190 ns in another?

The working set differs. The pointer-chase test read 91 ns at a 2 MB working set and climbed to 192 ns at 64 MB; from L1 it is 2.5 ns and from the 1 MB L2 14 to 25 ns. Quote the working set with any latency from this board.

### After moving the USB cable to another port, the console is silent

Probe all four ports again: the Linux console can come back under another name. On this Mac it was `/dev/cu.SLAB_USBtoUART1` and became `/dev/cu.SLAB_USBtoUART2` after the cable moved to another USB port. The helpers in `tools/` take the port from the `TDA4_PORT` environment variable.

### TI's HTTP-source demo runs at under 2 fps

Check the network path first; the demo streams a public video from freedesktop.org. With the board wired at 1 Gb/s it ran for 40 s and printed frame rates of 1.78 fps; the same model on a local file runs at the clip's 30 fps. Not isolated between the download, the container's software demux and decode, and the app.

### How fast is the Ethernet?

At line rate both ways: about 940 Mbit/s sending and receiving with iperf3 against a wired host, 870 to 934 each way at once, 0.31 ms round trip on the same switch. Receiving costs the A72s far more than sending: 52 to 58 percent of the CPU against 4 percent, with about 200 TCP retransmits per 10 s run when receiving.

## The sensor-bridge emulator

Words in this section: NVIDIA's **Holoscan Sensor Bridge** is a camera-to-GPU Ethernet device, and the emulator makes the board behave as one. It announces itself with **BOOTP** broadcasts, takes register reads and writes over **ECB** (NVIDIA's control messages on UDP port 8192), and sends frames on its **data plane** as **RoCE** packets, a format a network card can write straight into GPU memory; each packet carries a **PSN**, a sequence number.

### NVIDIA's emulator's CMake wants CUDA and the board has none

Build it with a plain Makefile instead of CMake. The Linux target's CMake turns CUDA on unconditionally, but the only CUDA call is a copy for GPU-resident buffers. A nine-line stub header covers it; the include order must put the platform directory first, as NVIDIA's CMake does.

### The emulator aborts with "Broadcast address not provided in DataPlane initialization"

Give the interface a broadcast address: `ip addr add 192.0.2.2/24 brd + dev hsb0`. Observed on 2026-10-10; the emulator's own text was not saved. It looks up its interface by IP and needs the broadcast address to send BOOTP.

### The emulator reports frames sent but the CPU cost reads near zero

Configure it from the host side first; in these runs it did not transmit until a host had written its data-plane registers. A host-side script that writes those registers does the configuration the way NVIDIA's library does.

### The metadata fields decode as nonsense frame numbers

Read them big-endian. NVIDIA's FPGA document says little-endian; the emulator writes them with `htonl`.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-24. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| No card reader: UART boot (SW1 1 ON, 2 OFF, 3 ON), tools/uart_boot.py, then write the image from a USB stick; SW1 all OFF and power cycle (switch settings are hands; the transfer is remote) | hands | proven | `tda4/FAQ.md`, date not recorded |

<!-- recovery:end -->
