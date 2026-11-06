# SK-TDA4VM starter kit results: what it measures, what did not run, what to measure next

On TI's Edge AI Linux 11.00.00.08 the SK-TDA4VM (TDA4VM: two Cortex-A72 cores at 2.0 GHz, a C7x DSP with the MMA deep-learning accelerator, two C66x DSPs, six Cortex-R5F cores, hardware video codecs) runs the 14 TI zoo models tried on the accelerator in 2.2 to 13.8 ms, boots to a login prompt in 24.2 s, and decodes 1280x768 H.264 at about 490 frames a second in hardware (499.6 by GStreamer's own counter). One A72 scores 9,835 CoreMark at gcc -O3, 10,466 at -O2 and 10,782 (5.39 per MHz) with tuned flags (`-O3 -mcpu=cortex-a72 -funroll-loops`), and 4.83 Dhrystone DMIPS per MHz by the rules; the two Arm cores take 13 to 135 times longer than the accelerator on the same models. A dependent load costs 2.5 ns from L1 and up to 190 ns from DDR (64 MB working set; 91 ns at 2 MB), and the worst timer wake-up in a 60 s run under load was 59 µs. This is TI's starter kit running TI's general demo image, measured with simple tools: [what that limits](#a-starter-kit-not-the-chips-ceiling) comes before the tables, and [TI's published figures against measured values](#tis-published-figures-against-measured-values) follows them. Every number measured on this board came over the serial console, apart from the Ethernet rows, which ran once a cable arrived at the end; the bench had no display, camera, card reader or power meter, and what those would have added is listed at the end. Wired Ethernet runs at line rate, about 940 Mbit/s each way. Figures from other boards and TI's published figures appear only as comparisons and are marked as such.

Terms: [vocab](../docs/vocab.md) (the "Linux vision boards" section). Problems and fixes: [FAQ](FAQ.md). Start here: [README](README.md). kB = 1,024 bytes; MB and MB/s = 1,000,000; MiB = 1,048,576. Every row measured here is in `data/results/tda4.csv` with its conditions, log and how it was checked. The board page and the experiments on the site: project "Run a vision model on a TI Linux board".

Measured 2026-10-09 and 2026-10-10 on an Intel Mac (macOS 15.7.9) talking to the board at 115,200 baud. Board: SK-TDA4VM (J721EXSKG01EVM), revision PROC112A1 (001), Processor SDK Linux Edge AI 11.00.00.08 (Linux 6.12.17-ti, gcc 13.3, Python 3.12, ONNX Runtime 1.15.0, DLR 1.13.0), 20 V class USB-C supply of unknown wattage, passive heatsink, no fan, room temperature not measured. The A72 clock is not exposed by cpufreq on this image; perf's cycles over task-clock read 2.000 GHz on every run that counted them.

Contents: [A starter kit, not the chip's ceiling](#a-starter-kit-not-the-chips-ceiling), [Processor](#processor), [Memory](#memory), [Deep learning](#deep-learning), [Video](#video), [Crypto and security](#crypto-and-security), [Linux](#linux), [Heat and power](#heat-and-power), [TI's published figures against measured values](#tis-published-figures-against-measured-values), [What was expected](#what-was-expected-and-where-the-numbers-disagree), [What did not run](#what-did-not-run), [What an engineer should still measure](#what-an-engineer-should-still-measure-on-a-board-like-this), [How these numbers were made](#how-these-numbers-were-made).

## A starter kit, not the chip's ceiling

The SK-TDA4VM is an evaluation board, and these are evaluation numbers: what a developer gets from this kit, TI's demo image and simple tools in a first week, not the most the TDA4VM can do in a product. Read every row with these limits in mind; each one is a known way the chip could do better than the number here.

| Limit on this bench | What it holds back | Where the chip could do better |
|---|---|---|
| Boot and storage from a micro-SD card (44 MB/s read, 30 MB/s write) | Boot time, cold start of the vision app, model loading | A product would boot from eMMC or OSPI flash; not measured here |
| Passive heatsink, no fan, room temperature not measured; TI's own advisory says the SDK has no thermal management | Any sustained load: the board reset once at 92.6 C and a five-minute pipeline reached 91.5 C | TI's user guide recommends a fan for heavy use; sustained numbers are not taken until one is fitted |
| A 20 V USB-C supply of unknown rating; TI warns a weak supply limits processing and can reset the board | Heavy runs; one unexplained reset at idle | A supply TI lists as tested, read before any sustained run |
| TI's Edge AI demo image as shipped | A stock kernel without PREEMPT_RT (timer jitter), a large memory carve-out for the vision firmware (Linux sees 2,170 MiB of 4 GiB), the remote cores running TI's demo services | A product image tuned for one job: an RT kernel, a smaller carve-out, the R5F and C66x cores given real work |
| The image's own gcc 13.3, simple flags | CoreMark moves 9.7 percent between flag sets in one batch (9,832 at -O3 to 10,782 tuned); every A72 row is one compiler | Other compilers, profile-guided builds and link-time optimization were not run |
| Plain C where faster code exists | The post-quantum rows use mlkem-native and mldsa-native without their Arm assembly back ends; the room-occupancy models are plain C | Both libraries ship AArch64 code paths that would be several times faster; not built here |
| Python harnesses and random inputs for the models | Each per-inference time includes Python's call overhead; outputs were not checked against labelled images | A C or C++ caller and an accuracy set (TI's own figures come from its C++ apps) |
| fp32 models on the CPU side | The 13x to 135x C7x speed-up is against unquantized CPU graphs | An int8 CPU build would narrow it; none was built |
| Decode at 1080p | `tiovxmemalloc` fails to allocate at 1080p here, and plain 1080p decode stops near 90 fps aggregate; the same plain path at 1280x768 reaches about 450 fps (about 440 against 185 megapixels a second), so the buffer path does not explain the gap | TI rates the decoder at four 1080p60 streams (data sheet Rev. L); cause unknown, and the 1080p clip is this repo's own encoder's output, not TI's |
| No display, camera or power meter; Ethernet only at the end, on a home network | No GPU, camera-latency or energy rows; network tests on a 1.5 kB MTU switch, with a macOS host as the peer | Listed under what an engineer should still measure |

## Processor

| Test | Result | Note |
|---|---|---|
| CoreMark, one A72, gcc -O3 | 9,835 per second (4.92 per MHz), three runs within 0.2 percent | EEMBC sources, posix port, validated by CoreMark's CRCs |
| CoreMark, one A72, gcc -O2 | 10,466 (5.23 per MHz), this repo's own files, three runs within 0.1 percent | 6.4 percent above the same files at -O3 (9,832 in that batch): on this gcc -O2 beats -O3 |
| CoreMark, one A72, with `-mcpu=cortex-a72` | 10,450 at -O2, 10,334 at -O3, 10,782 at `-O3 -funroll-loops` (5.39 per MHz) | Medians of three. The best of five flag sets tried, 9.7 percent above plain -O3 in the same batch; flag-tuned, while the other boards' CoreMark rows use each vendor's usual flags; other compilers and profile-guided builds not run |
| CoreMark, two threads | 19,580 | 1.99 times one thread |
| Dhrystone 2.1, by the rules (no inlining) | 4.83 DMIPS per MHz, 9,666 DMIPS on one core; 3.09 at -Os; 4.68 with `-mcpu=cortex-a72` | 117.8 cycles per Dhrystone from perf; the program's own 1 s clock is too coarse. `-mcpu` helps CoreMark and costs Dhrystone 3 percent |
| Dhrystone with inlining allowed | 5.26 to 5.68 DMIPS per MHz across three runs (5.26 the median) | Not rules-compliant; closer to the figure TI's table implies. TI's "24K DMIPS" for the pair works out to 6.0 per core |
| Cortex-R5F CoreMark | not run | Three of six R5F cores are free, but no J721E RTOS SDK is installed here |

## Memory

| Test | Result | Note |
|---|---|---|
| Load latency, dependent pointer chase | 2.5 ns (5 cycles) up to 16 kB; 5.1 ns at 32 kB; 8 to 14 ns from 64 kB to 512 kB; 24.5 ns at 1 MB; 91 ns at 2 MB; 125 to 192 ns from 4 MB to 64 MB | L1 data cache is 32 kB per core, L2 1 MB shared. Suspected, not profiled: 91 ns at 2 MB is DDR with most address translations still cached, and the climb to 190 ns is translation misses on top (no TLB counter was read; the `p=` in the log is only the chase's final index) |
| STREAM-style bandwidth, one thread (second pass) | copy 5,776, scale 5,068, add 5,086, triad 4,749 MB/s | 64 MB double arrays, best of 5 inside the program, two passes within 0.3 percent; the earlier one-thread program read 4,789 to 4,795 for triad; this repo's own loops, not the official STREAM program |
| STREAM-style bandwidth, two threads (second pass) | copy 6,344, scale 5,482, add 6,054, triad 5,385 MB/s | A second core adds 8 to 19 percent on the second pass (the first pass gave 3 to 19 percent; scale alone moved from 3 percent to 8 between passes, 5,227 to 5,482). Suspected, not profiled: one core already uses most of the DDR path for this pattern (no DDR counter was read during STREAM) |
| Memory for Linux | 2,170 MiB of the 4 GiB (`/proc/meminfo` MemTotal 2,222,672 kB) | dmesg's reserved regions name 624 MiB for the vision firmware's heap, 512 MiB for the CMA pool and 32 MiB for the R5F virtual Ethernet; the remaining about 758 MiB is not itemized there (suspected: remote-core firmware regions and the kernel's own reservations) |

## Deep learning

| Test | Result | Note |
|---|---|---|
| TI model zoo on the C7x and MMA, 14 models | 2.17 ms (MobileNetV1) to 13.76 ms (YOLOX-s pose 640); TI's four published figures within 10 percent | ONNX Runtime, TFLite and DLR with TI's TIDL delegates, 30 timed runs, random input, timing only |
| The same eight models on the two A72 cores (fp32) | 37 ms to 1,767 ms, 13 to 135 times slower | The cost of a CPU fallback on this image; [experiment](../docs/experiments/2026-10-10-tda4-c7x-vs-a72.md) |
| TVM build against TFLite build, ssdLite mobDet 320 | 12.97 against 6.40 ms | Runtime and input format both differ; suspected: the in-graph input conversion |
| Pose graph, file to encoded file | 29.98 fps paced by the clip on the long video0 clip (an earlier run on TI's short `left-1280x768.h264` read 17.8 to 18.1 fps at 55 to 56 ms a frame, a different clip and app setup), 55 percent of both A72s (36.5 ms of A72 per frame); app's own figures: 33.4 ms total, 19.0 ms inference per frame | `app_edgeai.py`, YOLOX-s pose on the C7x, hardware decode and encode |
| Zero-copy decode into a model | 110.8 fps at 33 percent of one A72 with `capture-io-mode=5` and `tiovxmemalloc`; 56.2 fps at 65 percent without: twice the frames at half the CPU per second, a quarter per frame | [experiment](../docs/experiments/2026-10-10-tda4-zero-copy-decode.md) |
| Two and three models on the C7x at once | yolox-nano alone 110.8 fps, ssd-lite alone 91.6; both together 81.7 plus 81.9 (163.6); two yolox-nano 91.2 plus 91.2 (182.4); three yolox-nano 72.1 each (216.4) | The mixed pair gets 1.5 times the frames of yolox-nano alone; two copies 1.65 times, three 1.95 times. What limits each copy is not profiled (suspected: the accelerator, which was 45 percent busy with one copy, the decoder or the scaler) |
| Vision app cold start to the first frame | 3.0 to 3.8 s from process start to the first frame-rate line (YOLOX-s pose, file in, file out) | The whole of model load, pipeline build and hardware start, not split; the first of three back-to-back launches was the slow one (3.84 against 3.03 and 3.09 s), none was right after a boot |
| Accelerator and DDR load during inference (TI perf_stats) | yolox-nano zero-copy at 111 fps: C7x 43 to 45 percent, scaler 26 percent, DDR 1,957 MB/s (1,486 read, 471 write); pose app at 30 fps: C7x 35 percent, DDR 4,134 to 4,322 MB/s; idle: DDR 23 to 26 MB/s | TI's `perf_stats` built from the image's own sources; its C7x figures agree with the per-inference times measured here within 15 percent. Its A72 counter (`mpu1_0`) disagrees with `/proc/stat` and `/usr/bin/time` in opposite directions: 17.5 to 19.5 percent against 33 percent of one core for yolox (1.7 to 1.9 times lower), 68 against 55 percent of two cores for the pose app (1.2 times higher); the cause was not found, and the CPU rows use `/proc` and perf |
| TI's demo configs, headless | 9 of 15 run clean with the display sink swapped for a file sink: the 8 file-only configs for 30 s, and the HTTP-source config once the board had Ethernet (but at only 1.78 fps, cause not isolated); 5 exit with a missing-camera message; the RTSP config points at a stream on another network and needs an RTSP server of its own | A sweep script run on the board over the serial console |
| The four room-occupancy models of this repo, plain C on one A72, gcc -O3 | 17.4 µs (500 parameters), 44.7 µs (2k), 193 µs (8k), 686 µs (30k); 20 of 20 vectors on every run; -Os 2.3 to 2.6 times slower | The MCU ladder's models and vectors. The 8k model: 0.193 ms here, 0.630 ms on the LP-AM13E230's NPU, 5.06 ms in the same plain C at -O3 on its Cortex-M33 at 200 MHz (5.54 ms through TI's NNC CPU build) |

## Video

| Test | Result | Note |
|---|---|---|
| Hardware H.264 decode, 1280x768, plain | 449 to 461 fps aggregate over 1 to 4 streams on a 263-frame clip with start-up inside | Suspected, not profiled: the decoder sets this, not the A72 |
| Hardware H.264 decode, 1280x768, zero copy | about 490 fps by wall clock, 499.6 by `fpsdisplaysink`, on a 6,000-frame run, one stream | `capture-io-mode=5 ! tiovxmemalloc` |
| Hardware H.264 encode, 720p | 131 fps lower bound | Bounded by the software test source |
| Hardware H.264 encode, 1080p | 53.7 fps lower bound, hardware decode feeding hardware encode on a 6,000-frame clip (decode alone runs at 66.7); 42 fps with the software test source as the input, which is the source's own limit | An earlier attempt exited with status 1 at once; the suspected cause is its 60 fps cap on the source with no parser, not isolated (the runs that worked added `h264parse` and a file sink). |
| 1080p H.264 decode, plain | 66.7 fps for one stream; 91.1 fps aggregate for two, 89.4 for four, on this repo's own 1080p clip | Without `tiovxmemalloc`, which fails to allocate at 1080p (`v4l2h264dec: Failed to allocate required memory`, at pool sizes 4, 8 and 12 alike). TI rates the decoder at four 1080p60 streams (240 fps). Cause of the ceiling unknown: the same plain path at 1280x768 reaches 2.4 times the pixel rate, so the buffers alone do not explain it |
| Software JPEG encode, 1080p | 41 fps on the A72 | libjpeg, for comparison with the hardware codecs |
| GPU | not run | Needs a display |

## Crypto and security

| Test | Result | Note |
|---|---|---|
| AES-128-CBC, AES-128-GCM, SHA-256 on one A72 (openssl) | 1,490, 1,560, 969 MB/s at 16 kB blocks | Arm crypto extensions |
| SA2UL hardware AES through /dev/crypto | 185 MB/s at 16 kB for 16 percent of a core; 886 MB/s at 64 kB with the core busy | Slower than the CPU at every block size; [experiment](../docs/experiments/2026-10-10-tda4-sa2ul-crypto.md) |
| SA2UL hardware SHA-256 through AF_ALG | 165 MB/s at 16 kB, 837 at 64 kB | `openssl speed -engine afalg` cannot bind: the kernel has a user-space hash interface but no skcipher one |
| ML-KEM-768 keygen, encaps, decaps (mlkem-native, plain C, gcc -O3) | 148.5k, 159.7k, 190.9k cycles (74, 80, 95 µs), process start-up spread over the 2,000 operations | The round-trip and tamper checks pass (no known-answer test was run here). The LP-CC1354P10's Cortex-M33 takes 996.5k cycles for keygen from the same mlkem-native source (TI Arm Clang -O3, code in flash, 48 MHz): 6.7 times more cycles, 280 times longer |
| ML-DSA-44 keygen, sign, verify (mldsa-native, plain C, gcc -O3) | 306k, 1.18M (1.14M to 1.25M, randomized), 331k cycles (153, 590, 166 µs) | Signature verifies; a tampered signature is rejected |

## Linux

| Test | Result | Note |
|---|---|---|
| First boot, bootloader to login | 27.7 s | Includes the root file system growing to the card |
| Warm boot, three runs | 24.0 to 24.5 s bootloader to login; 3.8 s more from a typed `reboot` | U-Boot 1.5 s, kernel start 6.5 s, systemd 13.6 s after the first bootloader byte |
| Boot breakdown (journal of a warm boot, kernel clock) | kernel 6.1 s, userspace 10.9 s to the multi-user target (17.0 s after the kernel started); card seen at 5.1 s, basic system 9.1 s, network target 10.3 s, remote cores attached 10.8 to 11.4 s, serial getty started 11.3 s, a USB stick mounted 15.9 s | The kernel's clock starts about 6.5 s after the first bootloader byte, so the 24.2 s login prompt of the timed row comes about 0.6 s after the multi-user target on this clock |
| Timer wake-up latency, cyclictest, 60 s, idle | max 41 µs, average 4 µs | `-m -p 80 -i 1000`, one thread on core 1; a stock kernel, no PREEMPT_RT |
| Same under a two-core CPU load | max 59 µs, average 4 µs | stress-ng --cpu 2 on both cores |
| Micro-SD card (the kit's SanDisk SC128, 128 GB) | 43.8 MB/s sequential read (hdparm), 44.8 MB/s (fio 1 MB direct), 3,823 random 4 kB reads a second, 30.5 MB/s sequential write (dd, direct, fsync) | The card is the root file system; the write test used a 256 MB file in /root |
| Wired Ethernet, TCP (iperf3 against a wired Mac on the same switch) | about 940 Mbit/s sending (4 percent CPU, 0 retransmits) and receiving (52 to 58 percent CPU, about 200 retransmits per 10 s); 870 to 934 each way at once; 0.31 ms round trip | 1000 Mb/s full duplex; receiving costs the A72s over 10 times what sending does |
| Linux to remote core message round trip | 74 to 75 µs to the MCU R5F0_0 and back (rpmsg echo, 2,000 messages, three runs) | Only that core's firmware answers TI's echo test; the vision firmware on the other five cores exposes TI's IPC endpoint instead, so their numbers need TI's SDK |
| Remote cores | C7x, both C66x and three R5F cores run TI's firmware (remoteproc "attached"); main-domain R5F1 pair and MCU R5F0_1 are free | From `/sys/class/remoteproc` |

## Heat and power

| Test | Result | Note |
|---|---|---|
| Idle, passive heatsink, daytime room | 73 to 77 C across the five zones | 63 to 67 C the night before |
| Five-minute pose graph at 30 fps | hottest zone rose to 91.5 C, no reset | Killed by hand; heavy runs carry a guard since (start under 80 C, stop at 90 C) |
| Sustained 2-thread CoreMark plus a TIDL loop | the board reset after about 60 s at 92.6 C | Cause not split between heat and supply; not rerun until the supply is known |
| Idle reset | once, at 74 to 77 C, idle, at the end of a run | Cause unknown; TI's guide names a weak supply or heat; the supply is an Apple USB-C charger recalled as 65 W, label not read |
| Cooling | 91.5 to 81 C in three minutes after the load stopped; 77 C after twelve | From the 10 s thermal log |
| Power at the wall, energy per inference | not run | Needs a meter |

## TI's published figures against measured values

Every TI figure here was read on 2026-10-09 from the source named; the expectations were written from them before the runs. "Like for like" says whether the two numbers measure the same thing.

| What | TI's figure | Source | Measured here | Like for like? |
|---|---|---|---|---|
| Boot | under 20 s to a wallpaper on a display | Edge AI Getting Started 11.00 | 24.2 s to a login prompt on the console, no display | No: different end points |
| MobileNetV1 mlperf 224 on the C7x | 2.03 ms | TI Edge AI datasheet page | 2.17 ms (+7 percent) | Close: the measurement used a Python call with random input, TI's a C++ pipeline |
| regNetx-200mf | 3.04 ms | same | 2.89 ms (-5 percent) | Close, same caveat |
| yolox-nano-lite 416 | 5.03 ms | same | 4.66 ms (-7 percent) | Close, same caveat |
| ssdLite mobDet 320 | 5.9 ms | same | 6.40 ms (+8 percent) | Close, same caveat |
| C7x load, yolox-nano-lite at 30 fps | 13 percent | same | 45 percent at 111 fps, 12 percent if scaled linearly to 30 fps | Partly: scaled linearly, and the tool behind TI's 13 percent is not named on the page that was read |
| DDR traffic of a 30 fps vision pipeline | about 1.4 to 1.5 GB/s for TI's four-model demos | same | 1.96 GB/s for yolox-nano at 111 fps; 4.1 to 4.3 GB/s for the pose app at 30 fps | No: different pipelines and rates |
| Dhrystone | "up to 24K DMIPS" for the dual core, 6.0 per MHz per core | TI E2E answer on the TRM statement | 4.83 per MHz by the rules, 5.26 to 5.68 with inlining | Partly: inlining closes most of the gap on the best run (5.68, 5 percent short), the rest is unexplained |
| CoreMark on the A72 | no TI figure; community A72 results at -O2 about 5.4 per MHz | zephray.me/coremark (not TI) | 5.23 per MHz at -O2, 5.39 flag-tuned | Yes, against the community figure |
| DDR bandwidth | 17.1 GB/s theoretical (4,266 MT/s on 32 bits) | Data sheet; SPRUJ21 | 5.4 GB/s triad and 6.3 GB/s copy from the two A72 cores | No: a theoretical bus figure against what two CPU cores reach; triad at 5.4 GB/s is below the 6 to 11 GB/s estimate (made before the run, not a source), copy at 6.3 inside it |
| H.264 decode | four 1080p60 streams (240 fps), or one 4K60 | Data sheet Rev. L | about 90 fps aggregate at 1080p, plain; about 490 fps at 1280x768 with TI's zero-copy path (499.6 by `fpsdisplaysink`) | No: different paths and this repo's own 1080p clip; the 1080p ceiling's cause is unknown (see the starter-kit table) |
| H.264 encode | one 1080p60 or three 1080p30 | Data sheet | at least 53.7 fps at 1080p, bounded by the decoder feeding it | Lower bound only |
| ResNet-50 and SSD MobileNet-V1 (MLPerf) | 162 and 385 fps | App note SPRACZ2 (2021, older tools) | not run: not found under `/opt/model_zoo` (`ls /opt/model_zoo`, 14 model folders) | n/a |
| Power | no figure for the kit; design allocations of 15 W core and 3 W peripherals | SPRUJ21 Table 2-2 | not run: no meter | n/a |
| TI's demo configs | most configs that need only a file or a USB camera run | Edge AI Getting Started | all 8 file-only configs and the HTTP-source config ran with a file sink in place of the display (image classification at only 1.0 fps, HTTP at 1.78 fps); 5 camera configs and the RTSP config could not run | Partly: the two USB-camera configs ran and exited at once with no camera attached; none was tried with a camera |

## What was expected, and where the numbers disagree

Expectations were written before the runs.

- **Boot under 20 s expected (TI's wallpaper claim): 24.2 s to a login prompt with no display.** Different end points; TI's was not timed here.
- **Zoo models within TI's published times: yes, within 10 percent on all four.**
- **C7x 10 to 100 times the A72s: 13 to 135.** Suspected, not profiled: the top end is above the guess because the CPU ran fp32 graphs through a generic runtime.
- **SA2UL slower than the CPU for bulk but lighter on CPU: slower, and no lighter per byte through the ioctl path.** The CPU-load half of the expectation was wrong for AES and held only for SHA-256 at 16 kB.
- **Zero copy as TI describes: the settings TI describes doubled the frame rate at half the CPU time per second, a quarter per frame.** That the copy is what disappeared is TI's description and was not profiled.
- **CoreMark 5.4 per MHz expected (community A72 figures at -O2): 5.23 at -O2, 5.39 at best.** The first recipe used -O3, which is 6.1 percent lower than -O2 on this gcc (-O2 reads 6.4 percent higher); this repo's own files at -O2 confirmed it.
- **Dhrystone against TI's 6.0 per MHz: 4.83 by the rules, 5.26 inlined.** Part of the gap is the inlining rule (4.83 to 5.26), as on the other boards in `docs/comparisons.md` whose Dhrystone rows carry both variants; the rest to TI's 6.0 is unexplained (suspected: TI's compiler flags or counting method; not checked).

## What did not run

| Test | Why |
|---|---|
| The Holoscan sensor-bridge test against NVIDIA's own receiver | Needs a Linux host with NVIDIA's `hololink` package; the stream works over the wire to this repo's checker |
| The RTSP-source demo | Needs an RTSP server on the network; TI's config points at an address on another network |
| GPU (kmscube, glmark2) | No display |
| Camera pipelines, end-to-end latency | No webcam, no CSI camera |
| Power at the wall, energy per inference, per CoreMark iteration | No meter |
| Cortex-R5F CoreMark, an R5F sensor-bridge port | No J721E RTOS SDK installed |
| Occupancy models of this repo on the C7x through TIDL | Needs TI's edgeai-tidl-tools on an x86 Linux machine |
| Sustained load rerun with a fan | No fan at the bench; the board reset once under that load at 92.6 C |
| Embench, CoreMark-PRO | Not reached; the sources would have to go over the serial cable (minutes) and CoreMark-PRO's build system is heavy |
| Sustained thermal run, energy, and anything above the starter-kit limits | Held until a fan, a known supply and a meter are available |
| 1080p H.264 encode as a true encoder figure | Needs a hardware-decoded 1080p source instead of videotestsrc |

## What an engineer should still measure on a board like this

What the bench lacked decides most of this list; the rest is what no single-board run can give.

1. **Power and energy.** Idle at the prompt, per inference, per decoded frame, per CoreMark iteration, at the wall and at the board's input, with the supply's negotiated voltage logged. The reset question (supply or heat) cannot be closed without it.
2. **Thermal with a fan and a known room.** The 10-minute sustained run with the kernel's trip points read, the cooling time constant, and the sustained frame rate once the chip settles. TI's own advisory says the SDK has no thermal management.
3. **Camera to display latency.** Glass to glass with a stopwatch app, at 30 and 60 fps, with and without the model in the loop. The file-based numbers here are throughput, not latency.
4. **Network.** iperf3 ran at line rate on a home switch; still to do: jumbo frames on a switch that allows them, a Linux peer, and the sensor-bridge stream against NVIDIA's receiver.
5. **The remote cores as compute.** CoreMark and a control loop on an R5F, the C66x DSPs on a kernel they are built for, and the cost of the message path between them and Linux at several message sizes.
6. **Accuracy as well as time.** The zoo models' labels on a fixed image set against TI's own sample results; the rows here timed random input.
7. **Model concurrency and scheduling.** More than one model on the C7x, a model plus a codec plus a scaler at once, and what the frame time variance looks like, as well as the mean.
8. **Storage and boot as a product sees them.** eMMC or OSPI boot against the micro-SD card, read-only root, boot to a running pipeline rather than a login prompt.
9. **Memory at scale.** Latency and bandwidth with both cores and the accelerator active, since the DDR is shared with everything on the chip.
10. **The developer experience.** How long a fresh engineer takes from box to a camera demo on a Linux PC with a reader, a display and Ethernet, the path TI's guide assumes; here it was about 3 h 10 min of working time from the sealed box to a first model inference on a Mac with none of them (2 h 30 min to a Linux prompt; the 90 minutes of driver time and the 40 minutes from login to first inference are estimates, not saved).

## How these numbers were made

Everything ran from the serial prompt with the tools on the image. Files went over the cable with `tools/push_serial.py` and came back with `tools/pull_tar.py`; the model benchmark is `bench/zoo/tvm_bench.py`. Heavy steps wait for every thermal zone under 80 C and stop at 90 C. CPU time comes from `/usr/bin/time` and `/proc/stat` deltas (ticks, which can under-read a process that sleeps between them) and from `perf stat` task-clock where a row says so; cycles from `perf stat`; frame rates from the app's own metric or `fpsdisplaysink`. Nothing here was read from the board by a second person.
