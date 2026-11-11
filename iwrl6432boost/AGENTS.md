# AGENTS.md for the TI IWRL6432BOOST radar: what an AI coding assistant needs to know

Know this first: what the board sends depends on its image. The first unit's image (SDK 05.03.03.00) streamed frames with no setup; the unit on the bench from 2026-11-03 ships `Presence_Demo` on SDK 05.04.00.01, which sends nothing until it gets a config, and runs TI's people tracker from a config alone (`iwrl6432boost/live/README.md`). Read `version` on the port ending in 1 first. TI's build route has no Mac host, so nothing custom has been built or flashed here. This file is the short list. Each item comes from runs on one board unless it says otherwise.

Terms: [docs/vocab.md](../docs/vocab.md) in the Fox Red Labs repo. In short: a **TLV** is a type-length-value block, the way the radar packs data inside each frame; **NRST** is the reset button; **SOP** is the switch pair that sets the boot mode; **JTAG** is a debug wiring scheme. 1 kB = 1,024 bytes. Checked 2026-10-09 and 2026-10-10 on a Mac with the board's shipped Motion and Presence demo (RFS 07.01.00.04, mmWave SDK 05.03.03.00) and DSLite from CCStudio 21 (`iwrl6432boost/README.md`, `data/results/iwrl6432boost.csv`).

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths below in backticks starting `iwrl6432boost/` are in the Fox Red Labs repo, where the evidence for each line lives; they are not in your copy, and links in this file only work inside that repo.

Safety: keep 20 cm between the board and a person while it transmits (`iwrl6432boost/README.md`).

## The board

| Part | Fact |
|---|---|
| Board | Silkscreen "PROC117 REV. B"; stickers IWRL6432BOOST, ASSY REV A1, Silicon REV ES2.0 (`iwrl6432boost/README.md`) |
| Chip ID over JTAG | CFGAP IDCODE 0x2BB8902F, DPIDR 0x5BA00477, core Cortex-M4 r0p1 (CPUID 0x410FC241) |
| Power | Micro-USB J5. D6 (5 V) and D3 (power good) light |
| Switches | S1 six positions (SOP0, SOP1 are 1 and 2), S4 four, S5 two (XDS_I2C_SW). SOP0 ON and SOP1 OFF is application mode, so the demo runs from flash at power-up |
| Reset | S2 (RESET_SW) is NRST |
| Ports | Two serial ports, `/dev/cu.usbmodem*` ending in 1 and 4; the one ending in 1 carries the data |

## Start here

This is a radar board. Its first result is radar frames from the shipped demo, not a blink; nothing here has been built or flashed from a Mac (see "Which routes reach this chip"). Built and checked with `python3` and pyserial (`python3 -m venv <scratch>/venvs/iwrl6432 && <scratch>/venvs/iwrl6432/bin/pip install pyserial`), the shipped Motion and Presence demo (RFS 07.01.00.04, mmWave SDK 05.03.03.00), and DSLite from CCStudio 21 with probe firmware 3.0.0.43.

| Step | Command or fact |
|---|---|
| Backup | No tested backup command. No tool was found that reads the board's 16 Mbit serial flash back, so the shipped image cannot be saved |
| Build | `bash portability/radar-family/build/build.sh <work dir>` builds TI's presence and tracking demo (MMWAVE-L-SDK 05.05.04.02) on a Mac for every family variant, boot images included (Docker for TI's image tools). Built, not flashed (`portability/radar-family/logs/build-2026-11-03.log`). TI's prebuilt `.appimage` files in Radar Toolbox 4.00.00.05 need no build |
| First result | Plug into J5, leave the switches as shipped, `ls /dev/cu.usbmodem*`, capture 15 s at 115,200 baud from the port ending in 1, then `python3 iwrl6432boost/tools/tlv_parse.py capture.bin`. `--selftest` needs no board |
| Flash another image (prepared, not run) | Start `python3 arprog_cmdline.py -p /dev/cu.usbmodem<serial>1 -f <image>.appimage -s SFLASH -t META_IMAGE1` only after S1 position 1 is OFF and S2 is pressed (the data port goes silent). This repo's wrapper `iwrl6432boost/tools/flash_arprog.py <arprog_dir> <port> <image.appimage> <log.txt>` refuses to start if the port is not silent. Then S1 position 1 back ON and press S2 |
| Configure a demo | `python3 iwrl6432boost/tools/send_cfg.py <port> <file>.cfg`, one line at a time |
| Check | `tlv_parse.py` prints one line per frame; the shipped demo gives 4.00 frames a second |
| Restore | Not tested. The candidate is TI's Motion and Presence 2D image (221 kB, SHA-256 starting 3c2803ea509906a8); whether it equals the shipped one is unknown |

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| Both ports send nothing and the CLI answers nothing, after a JTAG reset-and-halt failed with `-2063` and `-2062` | Link to the failure is a guess | Press S2 (NRST) | Frames resume |
| The probe shows `NOserial` | DSLite updated the probe firmware to 3.0.0.43 on first use | `xdsdfu -n RI32` | Ports `usbmodemRI32RI321` and `4` appear (first unit; the second unit is named `RI32` and its ports are `usbmodemRI321` and `4`) |
| `Error in setting up range profile:-40107` | The demo refuses a 1x1 antenna setting; the CLI hung | Press S2 and use the 2x3 configuration | The CLI answers |
| arprog says the connection succeeded but nothing is written | It reads a 0xCC byte in the streaming frames as the board's answer and sends its download command to the demo's CLI | Put the board in flashing mode first (S1 position 1 OFF, S2) | The port is silent for 2 s before you start |
| A 05.04 cfg on a 05.05 image tracks oddly, with no error | 05.05 `trackingCfg` reads 4 values plus `boresightFilteringEnable`; the 05.04 line's fifth value (61.4) turns that filter on | Cut `trackingCfg` to 4 values (`portability/radar-family/port_cfg.py`) | `cli_check.py <demo source> <cfg>` prints OK |
| A Python reader built on `read(in_waiting)` gets 0 bytes on macOS | The XDS110 data port reports 0 bytes waiting until a read is pending | Read first, then count (`ports/radar-host/sdk3-python-reader/run.py --mac-glue`) | Headers decode at 4 a second |
| ti_mmwave_rospkg starts and publishes nothing | It waits for parameters from an xWR cfg, then drops every frame when frames are more than 100 ms apart | Set its parameters (`ports/radar-host/ti-mmwave-rospkg/in_container.sh`) and feed frames under 100 ms apart | Points on `/ti_mmwave/radar_scan_pcl` |
| `REFUSED: ... bytes arrived on ... in 2 s; the board is not in flashing mode (reset it with S1.1 OFF first)` | `flash_arprog.py` found frames still arriving | Set S1 position 1 OFF, press S2, run again | The script prints `FLASH_OK` |

Traps shared with other boards: TRAPS.md.

## How to reach it

Plug into J5, leave the switches as shipped, find the ports, and capture 15 seconds at 115,200 baud from the port ending in 1 (`iwrl6432boost/README.md`, "Ten-minute path"):

```bash
ls /dev/cu.usbmodem*
```

```bash
python3 iwrl6432boost/tools/tlv_parse.py capture.bin
```

It prints one line per frame. `--selftest` runs it with no board. Frames start with the magic word `02 01 04 03 06 05 08 07`, then a 40-byte header, then TLVs; platform field `0x000A6432`.

## What ran here

- 2026-11-03, second unit (probe `RI32`, firmware 3.0.0.13, ports `usbmodemRI321` and `4`), image `Presence_Demo` SDK 05.04.00.01: people tracking from `iwrl6432boost/live/cfg/people-count-250ms.cfg` counted one person right in 89.5 percent of in-view frames with 0 overcounts on a spoken-cue walk; the TI tracker lines without our static-box change gave 25.2 percent (`radar_count_exact` rows).
- 2026-11-08, second unit, `Presence_Demo` 05.04 left running from 2026-11-03: a 10 s read of `usbmodemRI321` at 115,200 baud gave 40 contiguous frames of 96 bytes (frame counter 1735092 to 1735131), about 4 a second, no banner (the stream was already running). After an `xds110reset` and `iwrl6432boost/live/cfg/people-count-250ms.cfg` (0 config errors), an empty lab for 60 s gave 240 frames, 0 with a tracked person, 7 with points (`data/results/iwrl6432boost.csv`, `radar_false_alarm` tracker-250ms-0504-60s-empty). Not done: the person-walk rows (they need someone 20 cm or more from the radar), the Industrial Visualizer on this Mac.
- On that image `trackingCfg` takes 7 values though its help lists 6; `sigProcChainCfg` takes 8; `rangeSNRCompensation` is gone; `guiMonitor` point cloud 1 sends TLVs 1 and 7, 2 sends TLV 301. TI's 100 ms tracking frame hung it after about 31 frames; `xds110reset -s RI32` recovers it with no hands (`iwrl6432boost/live/README.md`).
- The shipped demo sends frames with no configuration: 384 bytes each, TLVs 315 and 319, 4.00 frames a second over 60 s, 240 contiguous frames, 1.51 kB/s of the roughly 11.25 kB/s a 115,200 baud link carries (`data/results/iwrl6432boost.csv`, rows `radar_frame_rate`, `radar_uart_bandwidth`).
- Re-configured Motion and Presence 2D demo, 2x3 antennas, 250 ms frame: walk-in at 6.53 m, seated person held at 4.0 m, 6,230 ms from the voice cue to detection (includes the person's reaction), 14.12 s to clear, 0 false alarms in a 60 s empty room. One run each, not the 20-of-20 rule (`data/results/iwrl6432boost.csv`, rows `presence-*-2x3`).
- Overnight, empty room, 6.08 h: 0 presence frames until a flag at 05:54, 95 percent upper bound 0.49 false alarms an hour; whether that flag was a person is not verified (`data/results/iwrl6432boost.csv`, row `presence-2x3-hold12-overnight`).
- Home Assistant bridge `iwrl6432boost/tools/ha_bridge.py` passed its self test and a replay of the 15 s capture. The live latency run was not finished (`iwrl6432boost/README.md`).
- 2026-11-03, radar family: the 05.05.04.02 demo builds on the Mac for the xwrL64xx-evm (IWRL6432, AWRL6432), -aop, -wcsp and xwrL14xx-evm (IWRL1432, AWRL1432) variants; boot images 192 B smaller than TI's prebuilt ones, same header layout. Every build writes platform 0xA6432, so the host cannot tell the chips apart (`portability/radar-family/README.md`, matrix X59 to X63).
- 2026-11-03, third-party readers on `guiMonitor 1` streams (re-encoded walk recordings): ibaiGorordo's SDK 3 Python reader read every point (NumPy 1.x); radar-lab/ti_mmwave_rospkg published every point with frames 50 ms apart and none at 250 ms; the top gesture recognizer read no frame (`ports/radar-host/README.md`, E299 to E301, `radar_host_points_delivered` rows).
- 2026-11-10, CC1407P radio on this board: a transmit app for the M4F (TI's host driver, `portability/cc1407p-host/hal_bare.c`, `iwrl6432boost/cc1407p/hooks_iwrl6432.c`) builds on a Mac with `iwrl6432boost/cc1407p/build.sh`: 47,876 B boot image, 42,360 B text, 1,598 B data, 24,280 B bss (`host_fit` rows). The SDK's `mcspi_loopback` example and its device data put McSPI A on balls D10 (CLK), B12 (MOSI), C11 (MISO), D11 (CS0); the schematic carries the same nets to header J2 (pins 2 to 6) and sockets J8 and J9. Built, not flashed (`iwrl6432boost/cc1407p/README.md`).
- 2026-11-10: `arprog_cmdline.py` has erase, write and format options and no read option, so the flash image cannot be backed up before a write. `preflight_image.py iwrl6432boost <image>.out` refuses every segment because it assumes an MSPM0 memory map (`iwrl6432boost/FAQ.md`).
- Not done: flashing TI's other demos, the 1.25 Mbaud rows, power rows, CoreMark, flashing or running any custom firmware (the CC1407P app is built only).

## Which routes reach this chip

`frl` has no route for this board (`data/routes.json`): TI flashes this chip with `arprog_cmdline.py` over the UART in a boot mode set by S1, not over SWD, so the probe-based backup, write and restore steps that `frl` uses do not apply; use the flashing steps in `iwrl6432boost/README.md`.

On a Mac, TI's SDK makefiles build the demo when the Unix helpers are passed in (`portability/radar-family/build/build.sh`, 2026-11-03; built, not flashed). Only TI's own route, and its build host is Windows or Ubuntu, not macOS: mmWave L SDK 05.05.00 wants Windows 10 64-bit or Ubuntu 18.04 64-bit or higher, TI Arm Clang 3.2.2.LTS, CCS 12.7.1, SysConfig 1.20.0. Zephyr 4.4.2 has no xWRL6432 board or SoC. No Arduino, PlatformIO or MicroPython support was found (absence of search results, not a vendor statement). A community Rust crate, `iwrl64xx-pac`, exists and was not built here (`iwrl6432boost/README.md`).

## Traps

1. **Radar Toolbox has no flasher;** `arprog_cmdline.py` ships with the mmWave L SDK (`iwrl6432boost/README.md`).
2. **`trackingCfg` changed between SDKs:** 7 values on the shipped 05.04 image, 4 plus an optional boresight flag on 05.05.04.02 (`portability/radar-family/README.md`).
3. **Frame period cuts both ways:** ti_mmwave_rospkg needs frames under 100 ms apart, and TI's 100 ms tracking cfg hung the shipped 05.04 image after about 31 frames. A faster cfg needs a hang check before any reader run (`docs/experiments/2026-11-03-iwrl6432boost-ti-mmwave-rospkg.md`, `iwrl6432boost/live/README.md`).
4. **Do not trust `preflight_image.py` on this board.** It refuses every radar image (it assumes an MSPM0 map); check the load ranges against the SDK's `linker.cmd` instead (`iwrl6432boost/FAQ.md`).
5. **Do not try 1.25 Mbaud blind.** It sits behind `ENABLE_UART_HIGH_BAUD_RATE_DYNAMIC_CFG`; a failed switch could leave the link at a rate the Mac cannot follow (`iwrl6432boost/README.md`).

<!-- ports:start (tools/render_ports.py) -->
## Ports that ran here

Code written for other chips and tools, brought to this board. The files to copy, the command and what you see for each are in `docs/ports.md`; the method and a prompt to port your own are there too. Works ran and did what it should; Could work ran in part.

| Port | Status | Result | Evidence |
|---|---|---|---|
| MCU+ SDK examples on the IWRL6432BOOST radar | Works | MCU+ SDK examples build in a radar SDK project, and uart_echo and gpio_led_blink ran on the IWRL6432BOOST. | `portability/everywhere/mcuplus-compat/README.md` |
| Open radar readers on the IWRL6432BOOST: the ROS driver, the Python SDK 3 reader, the gesture recognizer | Could work | Once the board sends the standard point format (guiMonitor 1), the most-forked Python mmWave SDK 3 reader reads every point of two recorded walks with its parser unedited; the reference ROS 1 driver publishes every point when frames come every 50 ms, nothing at the board's 250 ms; the most-starred gesture recognizer reads no frame, because it expects an AWR1642 SDK 2 layout and near-field gestures at 10 frames a second. | `docs/experiments/2026-11-03-iwrl6432boost-sdk3-python-reader.md` |
<!-- ports:end -->

## Where the evidence lives

`iwrl6432boost/README.md`, `iwrl6432boost/captures/`, `iwrl6432boost/eval/`, and `data/results/iwrl6432boost.csv`. Where the README and the CSV differ, the CSV rows are newer.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-11-03. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- TI's people-tracking config at 100 ms frames (frameCfg 2 0 200 64 100 0) on the shipped 05.04 Presence_Demo image: frames stop after about 31 and the CLI stops answering (`iwrl6432boost/live/README.md`, 2026-11-03)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| xds110reset -s <probe serial>, then resend the config (also clears a CLI hung by a failed sensorStart; the CLI answers 'version' about 6 s after the reset) | remote | proven | `iwrl6432boost/live/README.md`, 2026-11-03 |

<!-- recovery:end -->
