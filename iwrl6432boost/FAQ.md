# What goes wrong with the TI IWRL6432BOOST radar kit on a Mac, and how do you fix it?

Problems hit with an IWRL6432BOOST (IWRL6432 chip: Arm Cortex-M4 radar) from a Mac on 2026-10-09 and 2026-10-10. Terms: [docs/vocab.md](../docs/vocab.md). **NRST** is the reset button (S2); **SOP** is the switch pair that sets the boot mode.

**What caused it**, one tag per answer:

| Tag | Meaning |
|---|---|
| Our mistake | A mistake in this repo's runs. Listed so you do not repeat it |
| Setup or handling | How the hardware was plugged in or set up |
| Tool or docs | A TI tool bug, a missing tool, or a missing or wrong instruction |
| Third-party behavior | How someone else's code behaves with this board |
| Unknown | Not found |

## Connecting and reading the stream

### Both USB serial ports are silent and a 60 s capture is 0 bytes. What now?

Press NRST (button S2, `RESET_SW`) once, then capture again. Suspected: Our mistake. The ports were silent after a JTAG chip-ID read that tried a reset and halt of the M4; the demo may have stopped as a result, which is suspected, not checked. A 60 s capture, then 8 s on each port, a `version` and `help` on the CLI port, 10 s and 5 s gave nothing. Source: `iwrl6432boost/README.md`.

### DSLite fails with -2063 and -2062 on reset and halt of the M4. What now?

No fix recorded. What is known: the ID read works and the reset-and-halt step after it is what fails, so reading the ID alone is the untested workaround; a run that proves it has not been made. The ID read itself works (CFGAP IDCODE 0x2BB8902F, DPIDR 0x5BA00477); the reset and halt after it fails with -2063 and -2062 in the shipped mode. Press NRST afterwards if the ports go silent. Source: `iwrl6432boost/README.md`.

### The probe shows `NOserial` and the ports have no name. How do I restore it?

Write the name back with `xdsdfu -n RI32`. After a DSLite run updated the probe firmware to 3.0.0.43 it showed `NOserial`. `xdsdfu` writes 4 characters onto a 4-character prefix, so the serial became `RI32RI32` and the ports are `/dev/cu.usbmodemRI32RI321` and `4`; use those names in the ten-minute path. A second unit reads `RI32` (firmware 3.0.0.13) from the start, so its ports are `/dev/cu.usbmodemRI321` and `4`; read the name with `ls /dev/cu.usbmodem*` before using either. Source: `iwrl6432boost/README.md`.

## Building

### How do I build my own firmware for the IWRL6432BOOST on a Mac?

Run `bash portability/radar-family/build/build.sh <work dir>`. TI lists Windows 10 or Ubuntu as the MMWAVE-L-SDK host, but its makefiles build on a Mac when the Unix helpers are passed in (`"MKDIR=mkdir -p"`, `RM`, `COPY`, `CAT`, `TOUCH`). The boot image needs TI's `out2rprc.exe` under mono in an arm64 Debian container (mono crashes under amd64 emulation) and its x86-64 image tools in an amd64 one. All four variants of the 05.05.04.02 presence and tracking demo built this way on 2026-11-03, within 192 B of TI's prebuilt images; none has been flashed yet. Source: `portability/radar-family/README.md`.

### Where is the flasher for TI's other demos? Radar Toolbox has only images

Radar Toolbox 4.00.00.05 holds prebuilt images but no flasher; `arprog_cmdline.py` comes with the mmWave L SDK. Start the script only after S1 position 1 is OFF and NRST is pressed, because it reads any 0xCC byte as the board's answer. Source: `iwrl6432boost/README.md`.

## Configs and other people's code

### My 05.04 cfg runs on a 05.05 image, but tracking is off. Why?

Cut `trackingCfg` to its first 4 values. On MMWAVE-L-SDK 05.05.04.02 the fifth value is `boresightFilteringEnable` and the firmware works out velocity limits from the chirp; the 05.04 line `trackingCfg 1 2 100 3 61.4 191.8 250` turns the filter on and the rest is ignored, with no error. `portability/radar-family/port_cfg.py` rewrites a cfg for each chip of the family and `cli_check.py` checks it offline. Source: `portability/radar-family/README.md`.

### A Python radar reader reads nothing from the board on macOS. What now?

Read before you count. On this Mac the XDS110 data port reports 0 bytes waiting until a read is pending, so code that calls `read(in_waiting)` waits forever. `ports/radar-host/sdk3-python-reader/run.py --mac-glue` wraps the port; with it the reader decoded 120 frame headers in 30 s. Readers written for mmWave SDK 3 also need `guiMonitor 1 ...`, which sends float points in TLV 1. Source: `docs/experiments/2026-11-03-iwrl6432boost-sdk3-python-reader.md`.

### ti_mmwave_rospkg starts but publishes no points. What now?

Set its parameters by hand, and send frames less than 100 ms apart. It waits for parameters its config node computes from `profileCfg` and `frameCfg`, which a low-power SDK cfg lacks. Its serial reader then adds a stale byte on each 100 ms read timeout, so at our 250 ms frames every frame fails its length check. At 50 ms it published every point of a walk recording. Source: `docs/experiments/2026-11-03-iwrl6432boost-ti-mmwave-rospkg.md`.

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
