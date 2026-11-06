# How fast is the TI LP-EM-CC35X1, and what fits on it?

**Short answer.** CoreMark runs at 718.91 per second at 160 MHz with TI Arm Clang -O3 (4.49 per MHz), the four room-occupancy models fit and take 0.6 to 24.4 ms, and an edit to a blinking LED takes about a minute from the command line. The Wi-Fi join, throughput, MQTT, over-the-air update and UART loop were measured too (below). Power, Embench and the control loop have no number.

Terms: [docs/vocab.md](../docs/vocab.md) (CoreMark, FreeRTOS, activation) and the board's [FAQ](FAQ.md). 1 kB = 1,024 bytes. Start-here guide: [README](README.md).

Measured 2026-10-08 on an Intel Mac, macOS 15.7.9 (busy with other work, load average 4 to 40; the timings are counted on the chip's own tick, so Mac load does not move them). Board: LP-EM-CC35X1 Rev A with the CC3551E, on an LP-XDS110ET (TGT VDD on EXT, firmware 3.0.0.22), USB power on the board and the probe, chip antenna, activated with TI's SDK example key. SimpleLink Wi-Fi SDK 10.20.00.39, Toolbox 4.3.23, SysConfig 1.28.0, TI Arm Clang 5.1.1.LTS. Every program runs as a FreeRTOS thread with its code in the external 8 MB flash (IS25WJ064F) behind the chip's cache, and no Wi-Fi or Bluetooth stack running unless stated.

**How time is counted.** The chip's cycle counter (`DWT->CYCCNT`) reads 0 in programs loaded this way, so every timing uses FreeRTOS's 1 ms tick. For the 11 and 22 second CoreMark runs that is good to about 0.01 percent. The occupancy runs loop each vector for at least 100 ms and divide.

## Setup: edit to a changed blink

| Step | Time |
|---|--:|
| Edit `drivers/empty` (toggle period 1 s to 200 ms) | |
| Build (SysConfig and TI Arm Clang) | 6 s |
| Sign the images with the Toolbox image builder (SDK example key) | 16 s |
| Program with the Toolbox | 20 s |
| **Edit to programmed** | **60 s**, including one failed try (relative paths in `tool_settings.json`) |
| Activation, once | about 5 s |

Proof: the red LED toggled about five times a second; it was seen by eye. The blink image is 17.6 kB of external flash and 36.9 kB of RAM (TI's FreeRTOS `empty`; RAM is shared with the Wi-Fi and Bluetooth stacks when they run).

## CoreMark

| Build | Per second | Per MHz | Time for 8k iterations |
|---|--:|--:|--:|
| TI Arm Clang -O3 | 718.91 | 4.49 | 11.128 s |
| TI Arm Clang -Oz | 354.11 | 2.21 | 22.592 s |

Three resets each gave the same figure, and all four CoreMark checksums (`seedcrc` 0xe9f5, `crclist` 0xe714, `crcmatrix` 0x1fd7, `crcstate` 0x8e3a) match. The -O3 figure is above the 500 to 670 expected from the CC1354P10's 4.17 per MHz; **the cause is unexplained.** A second check on the clock: from the end of a reset to the printed result took 23.4 s on the -Oz build, against 22.59 s counted, the difference being the 0.8 s the chip takes to start. Whether the cache holds CoreMark's loop completely was not tested.

## Room-occupancy models (plain C integer code)

All 20 test vectors match in class and every logit, for all four models, at both flags. Time is the fastest of the 20 vectors, one pass of the whole model.

| Model | -Os | -O3 |
|---|--:|--:|
| 500 parameters | 0.840 ms | 0.602 ms |
| 2k | 2.318 ms | 1.538 ms |
| 8k | 9.454 ms | 6.250 ms |
| 30k | 36.333 ms | 24.400 ms |

Both the test harness and the model code are built at the same flag here, unlike the other boards' runs (harness at -O2). Code: `cc35x1/occ/`.

## Over-the-air update

TI's `ota_example` queried an HTTPS server on the Mac (TI's own Python server and playground certificates), found a newer vendor image, downloaded 27.7 kB in about a second, staged it and rebooted into it; the new image (TI's `uart2echo`, version 0.0.2.0) printed its banner and echoed typed characters. Resetting without accepting rolled the board back to the OTA app, as TI describes. Not accepted, so the old image stayed the committed one. One attempt.

## Dhrystone and ADC

Dhrystone 2.1, unmodified sources, 4,000,000 runs, checks pass: 377,786 per second at -O3 without inlining (215 DMIPS, 1.344 DMIPS per MHz), 369,890 at -Os, and 463,177 with inlining allowed (not rules-compliant, experiment only). The ADC converts 170,330 times a second with blocking reads from the CPU; that is the driver's rate, not the converter's limit. Code: `cc35x1/dhrystone/`, `cc35x1/adc/`.

## Radio, as far as it ran without a person

TI's Network Terminal (`upper_mac_3.0.10.78`, firmware 1.7.0.298, service pack 1.8.0.64) starts the Wi-Fi stack and `wlan_scan` lists 18 access points. It joined the home WPA2 network on a 5 GHz channel in 5.74 s from the `wlan_connect` command to an IPv4 address (ten runs, all 5.74 s at the serial port's 10 ms resolution; association at about 4.9 s). Across the router, a Mac pinging the board saw 3.08 ms fastest, 16.95 ms mean and 206 ms slowest over 100 packets with no loss. The board sent TCP to the Mac at 12.1 Mbit/s and UDP (20 Mbit/s offered) at 16.0 Mbit/s, one 10 second run each, uplink only; the data sheet says up to 20 Mbit/s UDP. TI's `mqtt_client` example (home network, a throwaway Mosquitto broker with no login, 2026-10-09) connected, subscribed to its four topics and answered a request published from the Mac with the board's temperature and accelerometer readings: 720 of 720 requests answered over an hour, median 167 ms from publish to reply (fastest 38 ms, slowest 1.2 s), no loss. The board used QoS 2, the Mac QoS 0. Home Assistant discovery was not tested: Home Assistant has no MQTT integration. Joining by Bluetooth provisioning took 7.13 s after the connect request (two runs; [experiment](../docs/experiments/2026-10-09-cc35x1-ble-provisioning.md)). The 2.4 GHz join was not measured separately: the board picks the band from one shared network name. Bluetooth LE advertises (legacy, 100 ms); the Mac sees the board at -34 dBm and connects in 0.52 seconds (first connect; the Mac's Bluetooth stack is part of that number).

## Voice front end: beamformer, echo canceller, noise suppressor (2026-10-26)

The same front end as on the LP-MSPM33C321A (`portability/voice-fe/`: four-mic beamformer, SpeexDSP 1.2.1 echo canceller with a 200 ms tail, noise suppressor) takes 13.3 ms of every 10 ms frame on this board: it does not run in real time. FreeRTOS thread, Wi-Fi and Bluetooth not started, 32 s of 16 kHz audio from a test signal in external flash, three runs each, cycles from the SysTick count.

| Build | Mean ms per frame | Worst ms | Beamformer, canceller, suppressor (ms) |
|---|--:|--:|---|
| -O2, code in external flash | 13.312 | 14.117 | 0.34, 9.86, 3.12 |
| -O3, code in external flash | 13.281 | 14.038 | 0.41, 9.84, 3.03 |
| -O2, hot code in the 32 kB CRAM | 12.713 | 13.498 | 0.31, 9.46, 2.94 |

The output CRC-32 of every pass equals the Mac build, and the canceller takes 26.3 dB off the echo. RAM 90.0 kB (heap high-water 85.6 kB, buffers 2.4 kB, stack 1.9 kB); code 48.9 kB at -O2. The same code needs 1.41 times the cycles of the LP-MSPM33C321A at the same clock. Moving the canceller, FFT and beamformer code (29.4 kB) into CRAM saves only 4.5 percent, so code fetch from external flash is not the main cause; data in the chip's DRAM is the untested suspect. Beside TI's Wi-Fi `ota_example` the linker maps leave about 74 kB of the 637 kB app RAM free (11 percent), counting the Wi-Fi heap reservation as used (`radio_ram_left` row, computed). Rows: `voice_fe_*` in `data/results/cc35x1.csv`.

## Not run, and why

| Test | Why |
|---|---|
| MQTT discovery into Home Assistant | Home Assistant has no MQTT integration; adding one is a decision for the owner |
| Power | needs a measurement setup and a person at the bench |
| Embench, control loop | built and signed; the first run on 2026-10-09 printed nothing, and the retry on 2026-10-17 was not flashed because another demo held the board |
| Example sweep, last two examples | `demos/indigo` and `demos/power_measurement` were not judged |
| Example sweep, run pass | 31 of 32 examples build (the bootloader example fails on a missing linker file) and 29 of 31 booted; silent examples cannot be judged without a person watching |
| CoreMark in RAM against flash | not run |
| Zephyr | no CC35xx board in Zephyr 4.4.2 |

## Wi-Fi presence, TI's CSI model (2026-10-09)

Rows `ai_presence_accuracy` and `ai_presence_first_result` in `data/results/cc35x1.csv`. The shipped model called "presence" in 533 of 543 scored windows across four 5-minute phases, and in 135 of 136 in the only phase with a trustworthy label (room empty, door closed): 0.7% right. The other three phases are unlabelled (the voice cue did not carry through the door and the tester did not follow the later cues), so the still and walking cases are not measured. First result 7.4 s after the start-streaming frame, then one every 2.0 s. Two TI gaps logged on the way: the console drops a long line sent in one burst at 921600 baud (pace the characters) and the README quotes a ready message the firmware does not print. Page: [Wi-Fi presence experiment](../docs/experiments/2026-10-09-cc35x1-wifi-csi-presence.md).

## Speed run, 2026-10-17: built, not run

Images for the core, control and security classes are built and signed (the sizes are in the `embench_fit` and `control_loop` rows of `data/results/cc35x1.csv`). No timed number exists: the board was held by the tool-telemetry demo, so those rows are `waiting` with that reason. Two findings stand: the chip's DWT cycle counter does not count (CTRL 0x40000001, CYCCNT 0, read by `probe.c`), so SysTick is the cycle counter; and the harness and UART work on a clean board (the probe image printed its report at 115200 on the first reset), so the 2026-10-09 silence came from two programs sharing one probe, not from the images.
