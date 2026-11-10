# TI IWRL6432BOOST 60 GHz radar on a Mac: the shipped demo's USB stream, parsed in Python

> Disclosure: Fox Red Labs is a lab run by a TI employee on personal time. Contributors' projects are their own. Not affiliated with, sponsored by, or endorsed by Texas Instruments or any other manufacturer.

**It streams presence and point-cloud frames over its USB serial port with no setup: about 4 frames a second, 384 bytes each on the 15 s file, and a few lines of Python parse them.** This folder reads that stream on a Mac. Safety: keep 20 cm between the board and a person while it transmits.

Terms: [vocab](../docs/vocab.md). Problems and fixes: [FAQ](FAQ.md). 1 kB = 1,024 bytes.

## What is in this folder

<!-- folder-contents -->
**In this folder:** this README; `AGENTS.md` (notes for an AI assistant, every line from a run); `FAQ.md` (every error hit, with its exact text and the fix). Folders: `captures/` captured radar frames, `eval/` evaluation scripts and configs, `img/` pictures, `tools/` helper scripts for this board, `zones/` the room-zone experiment. Results are rows in `data/results/iwrl6432boost.csv` at the top of the repository, not in this folder.

**Not in this folder:** TI's SDK, CCStudio, SysConfig and the compilers (you install them; the versions are under "Where these facts come from"); the radar's firmware images (it streams out of the box, so nothing here builds one).

**Taking this folder alone:** the commands in this README run from the top of a clone and call `tools/` and `data/`; the benchmarks also borrow sources from folders that several boards share. A sparse clone fetches only those, about 10 to 60 MB, instead of the whole repository (about 500 MB):

```bash
git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git
cd fox-red-labs
git sparse-checkout add iwrl6432boost tools data
```

`docs/` is left out on purpose (236 MB, mostly pictures). Read a page a link points to there with `git show HEAD:docs/<file>`, for example `git show HEAD:docs/vocab.md`.


## The board

Top silkscreen "PROC117 REV. B"; stickers "IWRL6432BOOST", "ASSY REV A1", "Silicon REV ES2.0". Pictures: `img/top.jpg`, `img/back.jpg`. Switches: S1 (six, SOP0 and SOP1 are positions 1 and 2), S4 (four) and a third one, S5 (two, `XDS_I2C_SW`). As shipped (read 2026-10-09): S1 = 1 ON, 2 OFF, 3 ON, 4 OFF, 5 ON, 6 ON; S4 = 1 OFF, 2 OFF, 3 OFF, 4 ON. SOP0 ON and SOP1 OFF is the application mode, so the demo runs from flash at power-up. Probe: after a DSLite run updated its firmware to 3.0.0.43 it showed `NOserial`; `xdsdfu -n RI32` wrote it back as `RI32RI32` (xdsdfu writes 4 characters onto a 4-character prefix), so the ports are `/dev/cu.usbmodemRI32RI321` and `4`; use those names in the ten-minute path.

Chip ID over JTAG (2026-10-09, DSLite, first unit, probe RI32RI32): CFGAP IDCODE 0x2BB8902F (version 2, part 0xBB89, manufacturer TI), DPIDR 0x5BA00477, core Cortex-M4 r0p1 (CPUID 0x410FC241). A dedicated device ID register address is unknown (not in the TRM text). The reset and halt step after the read fails with -2063 and -2062 in the shipped mode.

Not done: flashing TI's other demos.

LEDs as photographed (2026-10-09, `img/top.jpg`): D6 (5V_LED, green), D5 (RESET_LED, green) and D3 (PGOOD, amber) are lit. D7 (USER_LED) and the other LEDs are dark. The photo does not say what the demo prints on its CLI; read-only commands are in the plan.

## Ten-minute path

1. Plug the board's micro-USB port (J5) into the Mac. Leave the switches as shipped. Lights D6 (5 V) and D3 (power good) come on.
2. Find the ports: `ls /dev/cu.usbmodem*`. The board adds two, ending in `1` and `4`. The one ending in `1` carries the data.
3. Make the venv: `python3 -m venv <scratch>/venvs/iwrl6432 && <scratch>/venvs/iwrl6432/bin/pip install pyserial`.
4. Capture 15 seconds at 115,200 baud from the port ending in `1` into a file with any serial tool.
5. Parse it: `python3 tools/tlv_parse.py capture.bin`. One line prints per frame: frame number, bytes, detected objects, the TLV types and sizes.
6. Self test with no board: `python3 tools/tlv_parse.py --selftest`.

## What the shipped image does (read 2026-10-09)

It sends frames with no configuration needed. Each starts with the 8-byte magic word `02 01 04 03 06 05 08 07`, then a 40-byte header, then TLVs. Platform field `0x000A6432`. Details and numbers: `captures/` and the table below.

## One-command build

There is no build command for this board on a Mac. The shipped image needs no build, and TI's build route has no Mac host (table below). The nearest single command is the parser's self test with no board: `python3 tools/tlv_parse.py --selftest`.

## What was measured

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| Frame rate, shipped Motion and Presence demo | 4.00 frames a second | 115,200 baud, 60 s capture; no frame number missing inside it | measured | `radar_frame_rate` in `data/results/iwrl6432boost.csv`, `iwrl6432boost/captures/2026-10-10-shipped-uart-60s.bin` |
| Link load of the shipped demo | 1.51 kB/s (about 13 percent of the link) | derived from the 60 s capture; 11.5 kB/s is what 115,200 baud carries | measured | `radar_uart_bandwidth` |
| Frame size | 384 bytes | shipped demo, 15 s capture, frames 1340 to 1398 back to back | measured | `iwrl6432boost/captures/2026-10-09-shipped-uart-15s.bin` |
| Chip ID over JTAG | CFGAP IDCODE 0x2BB8902F, DPIDR 0x5BA00477, core Cortex-M4 r0p1 (CPUID 0x410FC241) | DSLite, first unit, probe RI32RI32, 2026-10-09 | measured | `iwrl6432boost/IWRL6432.ccxml` |
| Switch settings as shipped | S1 = 1 ON, 2 OFF, 3 ON, 4 OFF, 5 ON, 6 ON; S4 = 1 OFF, 2 OFF, 3 OFF, 4 ON | read at the bench, 2026-10-09; SOP0 ON and SOP1 OFF is application mode | measured | `img/top.jpg` |
| Walking-in detection range, 2x3 antenna setting | 6.53 m | Motion and Presence 2D demo, 250 ms frame period, paced walk-in at the first two flagged frames | measured | `radar_detect_range` (presence-walking-2x3) |
| Seated detection range, 2x3 | 4.0 m | farthest tested distance held in 4 of 5 frames over 40 s; not the 20 of 20 rule | measured | `radar_detect_range` (presence-seated-2x3) |
| False alarms, 2x3, overnight | 0 presence frames in 6.08 h of an empty room, then a flag at 05:54; 95 percent upper bound 0.49 an hour | PresenceDetect-hold12.cfg; whether that flag was a person is not verified | measured | `radar_false_alarm` (presence-2x3-hold12-overnight) |
| Expected frame rate before the 60 s capture | about 4 frames a second | from the 15 s file | computed | `iwrl6432boost/captures/2026-10-09-shipped-uart-15s.bin` |

Not measured: the 1.25 Mbaud rows, Home Assistant latency (`radar_ha_latency` mac-rest), and TI's other demos. The full list is `data/results/iwrl6432boost.csv`.

## Which toolchains and tools reach this chip (read 2026-10-09)

Plain answer: only TI's own route, and its build host is Windows or Linux, not macOS. Every line is a row in `data/results/iwrl6432boost.csv`.

| Route | Status | What was read |
|---|---|---|
| mmWave L SDK 05.05.00 (TI's build route) | not supported on a Mac | Host PC is Windows 10 64-bit or Ubuntu 18.04 64-bit or higher, with TI Arm Clang 3.2.2.LTS, CCS 12.7.1, SysConfig 1.20.0. A Mac needs a Linux build host |
| Zephyr | not supported | Zephyr 4.4.2 on this Mac has no xWRL6432 board or SoC; GitHub code search finds none |
| Arduino, PlatformIO, MicroPython | not supported | Searches found no core, platform or port. This is the absence of a result, not a vendor statement |
| Rust | not run | A community crate, `iwrl64xx-pac` (svd2rust, built from TI's `iwrl6432.xml`), exists; its authors blinked the LED from Rust in May 2025. Register access only, no HAL. It was not built here |
| Radar Toolbox 4.00.00.05 | not run | Prebuilt images, configs and docs are plain files; the visualizer is a Windows `.exe` with Python source |
| Applications and Industrial Visualizer | not run | Lists the xWRL6432 Out of Box Demo with Motion and Presence. Mac use untested |
| MATLAB Radar Toolbox support package | not run | Lists "TI IWRL6432BOOST" (ES2), one port for configuration and data. No MATLAB on this Mac |

The cross-platform portability matrix (`portability/matrix.csv`) is built per software layer between two chips (TI Drivers, DriverLib), so it has no row shape for this board: the radar has no driver example twin to port.

## Frame rate and byte rate on the shipped demo

Expected before any new capture, from `captures/2026-10-09-shipped-uart-15s.bin`: frame numbers 1340 to 1398 arrive back to back (the header's frame number is the radar's frame counter, so no gap means no loss), each 384 bytes, about 4 frames a second, about 1.5 kB/s of the 11.5 kB/s a 115,200 baud link carries.

Measured later, 2026-10-10: 4.00 frames a second and 1.51 kB/s on the shipped demo (table above). On the evening of 2026-10-09, at 23:14 on 2026-10-09 the board's two ports were silent (60 s capture at 0 bytes, then 8 s on each port, a `version` and `help` on the CLI port, 10 s, 5 s: nothing). The JTAG chip-ID read earlier that evening tried a reset and halt of the M4 that failed with -2063 and -2062; the demo may have stopped as a result (suspected, not checked). One press of NRST (button S2, `RESET_SW`) brings it back; `tools/capture_ts.py` takes the 60 s capture.

The 1.25 Mbaud rows are not run. In the toolbox source the 1.25 Mbaud code sits behind `ENABLE_UART_HIGH_BAUD_RATE_DYNAMIC_CFG`; whether the shipped build has it is unknown, and a failed switch could leave the link at a rate the Mac cannot follow until a reset.

## Home Assistant from the Mac

No walkthrough with screenshots exists; this section is the recipe.

1. Keep the radar plugged in and streaming. Nothing is written to it.
2. `python3 tools/ha_bridge.py selftest` (no hardware), or `python3 tools/ha_bridge.py replay captures/2026-10-09-shipped-uart-15s.bin` to see the presence changes in the 15 s file (frame 1274 off, 1275 on, 1340 off).
3. `python3 tools/ha_bridge.py run --samples 20 --log ha-latency.csv` posts `binary_sensor.iwrl6432_presence` on every change and every 30 s, reading the token from the `home-assistant` entry in `~/.claude.json` through `tools/ha.py` (never printed, never stored here).
4. Walk in and out of the beam. Each change prints the frame number, the new state, ms from the frame's last byte to the REST reply, and ms to a confirming read.

Presence is on when any zone state in TLV 315 is nonzero (byte 0 is the zone count, then 2 bits per zone). Home Assistant forgets a state posted this way when it restarts until the next post, which the 30 s heartbeat covers.

Status: the bridge parses split frames (selftest ok) and the 15 s file (replay ok), and Home Assistant answers a ping. `radar_ha_latency` mac-rest has no number: the stream is silent until NRST, and the 20 samples need movement in front of the radar.

## Try TI's other demos (not run on this board)

These steps were written from TI's documents and have not been run. TI's Radar Toolbox 4.00.00.05 holds prebuilt images for this board (Motion and Presence 2D and 3D, Out Of Box, mmWave Demo). It does not hold a flasher: `arprog_cmdline.py` comes with the mmWave L SDK. Only one switch moves.

1. Unplug nothing. Slide S1 position 1 (the bottom slider of the six-switch block, next to SOP0) to OFF, then press S2 (RESET_SW). The data port goes silent.
2. Run `python3 arprog_cmdline.py -p /dev/cu.usbmodem<serial>1 -f <image>.appimage -s SFLASH -t META_IMAGE1` (needs pyserial). Start it only after step 1: it reads any 0xCC byte as the board's answer.
3. Slide S1 position 1 back to ON, press S2.
4. Send the demo's cfg line by line: `python3 tools/send_cfg.py <port> <file>.cfg`, then `python3 tools/tlv_parse.py` on a capture.

## If you see this

- If you see `-2063` and `-2062` after a reset and halt of the M4 over JTAG, the shipped mode does not allow it; both ports may then go silent until NRST (button S2) is pressed ([FAQ](FAQ.md#dslite-fails-with--2063-and--2062-on-reset-and-halt-of-the-m4-what-now)).
- If you see both ports silent (a capture of 0 bytes), press NRST (button S2, `RESET_SW`) once ([FAQ](FAQ.md#both-usb-serial-ports-are-silent-and-a-60-s-capture-is-0-bytes-what-now)).
- If you see a probe serial of `NOserial` after DSLite updated the probe firmware to 3.0.0.43, write the name back with `xdsdfu -n RI32` ([FAQ](FAQ.md#the-probe-shows-noserial-and-the-ports-have-no-name-how-do-i-restore-it)).

The rest: [FAQ](FAQ.md).

## Limits

TI's other demos are not flashed. The Home Assistant latency rows need someone to walk through the beam.

## Where these facts come from

Read 2026-10-09 and 2026-10-10 on a Mac with the board's shipped Motion and Presence demo (RFS 07.01.00.04, mmWave SDK 05.03.03.00), DSLite for the JTAG read, `xdsdfu` for the probe name, and Python with pyserial. TI's build route is mmWave L SDK 05.05.00 (TI Arm Clang 3.2.2.LTS, CCS 12.7.1, SysConfig 1.20.0); Radar Toolbox 4.00.00.05. Files: `data/results/iwrl6432boost.csv`, `iwrl6432boost/AGENTS.md`, `iwrl6432boost/FAQ.md`, `iwrl6432boost/captures/` and the sources behind the toolchain table.
