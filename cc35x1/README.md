# How do you get a TI LP-EM-CC35X1 LaunchPad development kit from the box to a changed blink on a Mac?

> Disclosure: Fox Red Labs is an independent lab run by a TI employee on personal time. Contributors' projects are their own. Not affiliated with, sponsored by, or endorsed by Texas Instruments or any other manufacturer.

**It takes about a minute from an edited line to a blinking LED once the chip is activated, using only terminal commands.** The one step you cannot undo is the activation: TI's Wi-Fi Toolbox burns a security key into fuses inside the chip, once, before it accepts any program. The board used here arrived not activated and was activated with TI's example key (5 seconds).

Terms: [docs/vocab.md](../docs/vocab.md). Problems and fixes: [FAQ](FAQ.md). Every number, with how it was measured: [RESULTS-cc35x1.md](RESULTS-cc35x1.md). What was not run: [RESULTS-cc35x1.md](RESULTS-cc35x1.md). 1 kB = 1,024 bytes; 1 MB = 1,024 kB.

## What is in this folder, and what is not

<!-- folder-contents -->
**In this folder:** this README; `AGENTS.md` (notes for an AI assistant, every line from a run); `FAQ.md` (every error hit, with its exact text and the fix); `RESULTS-cc35x1.md` (the measured results); `prebuild.sh` (builds every target that needs no board; it writes outside the repository). Folders: `adc/` ADC tests, `chained-demo/` the chained radio demo, `coremark/` CoreMark build, `csi/` Wi-Fi channel-state-information runs, `dhrystone/` Dhrystone build, `img/` pictures, `matter/` Matter demos, `mqtt-ha/` MQTT to Home Assistant, `mqtt-test/` MQTT tests, `occ/` occupancy-model runs, `ota/` over-the-air update, `uartloop/` the UART loop test. Results are rows in `data/results/cc35x1.csv` at the top of the repository, not in this folder.

**Not in this folder:** TI's SDK, CCStudio, SysConfig and the compilers (you install them; the versions are under "Where these facts come from"); built images (`.out`, `.hex`, `.bin`, `.elf`), which are not cleared for redistribution, so you build them with the command below.

**Taking this folder alone:** the commands in this README run from the top of a clone and call `tools/` and `data/`; the benchmarks also borrow sources from folders that several boards share. A sparse clone fetches only those, about 10 to 60 MB, instead of the whole repository (about 500 MB):

```bash
git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git
cd fox-red-labs
git sparse-checkout add cc35x1 tools data cc2340r5 third-party/coremark portability/fom
```

`docs/` is left out on purpose (236 MB, mostly pictures). Read a page a link points to there with `git show HEAD:docs/<file>`, for example `git show HEAD:docs/vocab.md`.

## The board

The CC3551E chip: an Arm Cortex-M33 at 160 MHz that runs your own program, 1.1 MB of RAM, Wi-Fi 6 on 2.4 and 5 GHz, and Bluetooth LE 5.4 (TI data sheet SWRS337B, June 2026). The program sits in an 8 MB flash chip next to it (IS25WJ064F, found by `detect_flash_type`). The LaunchPad adds an RGB LED, two buttons, a temperature sensor, an accelerometer, a chip antenna and 40 header pins (TI user guide SWRU629C). It has **no debugger**: it plugs onto an LP-XDS110ET or LP-XDS110, sold separately, and it also needs its own USB-C cable for power, both to the same computer.

## Ten-minute path

What you need: the board, an LP-XDS110ET debugger, two USB-C cables, and the tools above.

1. Debugger's TGT VDD jumper on **EXT**, the board's 20-pin edge into the debugger, both USB-C cables into the Mac ([picture](img/cc35x1-mount.png)). Leave the board's jumpers as shipped, with J4 and J6 on the lower two pins.
2. Check the activation state (read-only). The Toolbox in `/Applications/ti/simplelink_wifi_toolbox_macos_4_3_23`:

```bash
./simplelink-wifi-toolbox programmer -i XDS110 -param1 <probe serial> get_fuse_data --flash_type IS25WJ064F --activation_type sdk_example_key --verbose
```

   On a board that is not activated it stops with `Process is stopped. Life cycle OPERATIONAL is not valid.` That is the answer "not activated". `<probe serial>` is the eight characters `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e` prints.
3. Activate, once, if step 2 said OPERATIONAL. This burns fuses for good:

```bash
./simplelink-wifi-toolbox programmer -i XDS110 -param1 <probe serial> custom_activation --flash_type IS25WJ064F --activation_type sdk_example_key --authentication_enabled yes
```

   Run step 2 again: it now prints the fuse table.
4. Build TI's blink (`examples/rtos/LP_EM_CC35X1/drivers/empty` in the SDK), sign it with the Toolbox's image builder, and program it. `prebuild.sh` has the build and image commands; program with `programmer ... programming --tool_settings <path to the tool_settings.json the image builder wrote>`. Use absolute paths. The red LED toggles once a second; change the period, rebuild, program again (20 seconds). It blinked five times a second.
5. Serial output is on `/dev/cu.usbmodem<probe serial>1` at 115200 for TI's SDK examples and at 921600 for the Wi-Fi CSI program; read at the rate of the image that is on the board, or the port looks silent. Set the baud in the same program that reads the port (a Python `termios` script does it); `stty` followed by `cat` reads at the wrong rate on macOS. `xds110reset -s <probe serial>` resets the board.

## One-command build

```bash
cc35x1/prebuild.sh
```

Builds the blink and signs its images, CoreMark at two optimization levels, the four room-occupancy neural nets for this chip's core, every SDK example for this board (31 of 32 build; the bootloader example fails on a missing linker file), TI's MQTT example with a placeholder network name, and the Thread border-router host libraries, into `./build-cc35x1` in the folder you run it from (set `FRL_OUT` to change it). It touches no board, never runs the activation, installs nothing, and refuses to start while the Mac is busy (add `--wait` to wait instead). `cc35x1/occ/build.sh` builds the occupancy test images.

## What was measured

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| Edit to blinking LED | 60 s, then 20 s for a later program-only change | Toolbox, command line, board mounted and activated | measured | `time_to_blink` (cli) in `data/results/cc35x1.csv` |
| CoreMark, TI Arm Clang -O3 | 718.91 per second (4.49 per MHz) | 160 MHz; above the 500 to 670 expected, cause unexplained; cross-checked with the host clock | measured | `coremark` (ticlang-freertos-o3) |
| CoreMark, -Oz | 354.11 per second | 0.49 of -O3 | measured | `coremark` (ticlang-freertos-oz) |
| Occupancy models, plain C, -O3 | 0.602, 1.538, 6.25 and 24.4 ms for the 500, 2k, 8k and 30k models | all four fit; 20 of 20 test vectors match | measured | `ai_occ_latency`, `ai_occ_fit` in `data/results/cc35x1.csv`, `cc35x1/RESULTS-cc35x1.md` |
| Wi-Fi join, home WPA2 network | 5.74 s to an IPv4 address (ten runs) | 5 GHz; board to Mac TCP 12.1 and UDP 16.0 Mbit/s, ping mean 17 ms | measured | `radio_join` (wifi-wpa2-5g) |
| MQTT request and reply through a throwaway broker | 720 of 720 answered over an hour, median 167 ms | [how to repeat it](mqtt-test/README.md) | measured | `cc35x1/RESULTS-cc35x1.md` |
| Bluetooth LE connect | 0.52 s | advertises, the Mac sees it and connects | measured | `ble_connect` |

Also measured, with the numbers in [RESULTS-cc35x1.md](RESULTS-cc35x1.md): the UART loop (4 kB correct at every speed from 115200 to 5 Mbaud), an over-the-air update, and a Matter light and thermostat in Apple Home ([Matter](matter/README.md)). Not measured: MQTT discovery in Home Assistant (it has no MQTT integration) and power. The reasons are in the same file.

Wi-Fi presence from channel state information (TI's CSI example, plugin 1.20, released 2026-10-07): the image builds and signs from a terminal (`csi/build.sh`), the Wi-Fi login is typed to the board at run time (`csi/csi_run.py`), the board joins, and TI's model reports one result every 2 s, the first 7.4 s after the host asks. In the test room on 2026-10-09 the shipped model said "presence" in 97% of windows over 21 minutes, and 135 of 136 times in the one phase with a known label (empty, door closed), so it does not detect out of the box here; TI's data-capture image and training on that room would be needed. Full table and conditions: [Wi-Fi presence experiment](../docs/experiments/2026-10-09-cc35x1-wifi-csi-presence.md).

## If you see this

- If you see `Process is stopped. Life cycle OPERATIONAL is not valid. Get fuse data failed`, the tool is not broken: the chip is not activated. Run the activation in step 3 of the ten-minute path ([FAQ](FAQ.md#get_fuse_data-prints-process-is-stopped-life-cycle-operational-is-not-valid-get-fuse-data-failed-is-the-tool-broken)).
- If you see `Path to programming_instructions after path correction couldn't be found`, rebuild the images with an absolute build folder, or replace the relative paths in `tool_settings.json` with absolute ones ([FAQ](FAQ.md#programming-stops-with-path-to-programming_instructions-after-path-correction-couldnt-be-found)).
- If you see a silent serial port or one or two odd characters, read at the baud rate of the image on the board: 115200 for TI's SDK examples, 921600 for the Wi-Fi CSI program ([FAQ](FAQ.md#the-boards-serial-port-is-silent-or-prints-one-or-two-odd-characters)).

The rest: [FAQ](FAQ.md).

## Where these facts come from

Checked 2026-10-08 on an Intel Mac, macOS 15.7.9: SimpleLink Wi-Fi SDK 10.20.00.39 (TI's SDK for the CC35xx family only), SimpleLink Wi-Fi Toolbox 4.3.23, SysConfig 1.28.0, TI Arm Clang 5.1.1.LTS (TI tested the SDK with 4.0.4.LTS). Board marked "LP-EM-CC35X1 Rev A", debugger firmware 3.0.0.22 (it worked as shipped). Measurements and experiments run 2026-10-08 to 2026-10-13 (dates per row in the data file). Files: `data/results/cc35x1.csv`, `cc35x1/RESULTS-cc35x1.md`, `cc35x1/AGENTS.md`, `cc35x1/FAQ.md`.
