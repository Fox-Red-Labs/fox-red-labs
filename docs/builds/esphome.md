# Configure a TI radio chip with an ESPHome YAML

Add a TI platform to an ESPHome YAML, run esphome compile and esphome upload, and a development kit joins Home Assistant: Zigbee on the LP-EM-CC2755P10 and LP-EM-CC2340R5. The LP-EM-CC35X1 builds and joins Wi-Fi; Home Assistant adoption has no recorded run on it.

The page: [site/project-esphome.html](../../site/project-esphome.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## ESPHome Zigbee router on the LP-EM-CC2755P10

ESPHome's own compile and upload put a Zigbee router on the board; it joins Home Assistant in about 6 s.

Status: Works. Boards: cc2755p10.

Folders this version uses (in a sparse clone, fetch them first, then read `ports/esphome-ti/AGENTS.md`):

```bash
git sparse-checkout add ports/esphome-ti
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-EM-CC2755P10 LaunchPad development kit | 1 | the board |
| LP-XDS110ET debugger | 1 | the debugger that programs the board |
| Home Assistant with the Zigbee Home Automation (ZHA) integration | 1 | the hub the board joins |
| A Mac to build and flash from | 1 | runs ESPHome and the flasher |

### Before you start

- A Mac. Only macOS was tried, on an Intel Mac. ESPHome 2026.9.1 and the TI tools of steps 1 and 2 at exactly these versions: other versions were not run.
- Home Assistant with the Zigbee Home Automation (ZHA) integration and a working Zigbee coordinator.
- Close any CCStudio debug connection before step 6; it holds the debug probe.
- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`.

### Steps

1. **Mac, terminal.** Make a Python 3.12 virtual environment and install ESPHome 2026.9.1 into it. The `cbor2<6` pin is needed on an Intel Mac, where cbor2 6 has no wheel and wants a Rust compiler.

   ```text
   python3.12 -m venv venv && ./venv/bin/pip install "cbor2<6" esphome==2026.9.1
   ```

   You should see: pip finishes in about 26 s and the environment holds 222 MB.

   If pip stops asking for a Rust compiler, the `cbor2<6` pin is missing.

2. **Mac, installers.** Install TI's tools once, about 8 GB in all: the SimpleLink Low Power F3 SDK 9.21.00.36, TI Arm Clang 5.1.1.LTS (it comes with CCStudio 21.0.1), SysConfig 1.26.3 and UniFlash 9.6.0. The downloads need a free myTI login. The platform looks under `/Applications/ti`, `~/ti` and `/opt/ti`.

   You should see: The four folders sit under `/Applications/ti`.

   If a build later stops with `TI F3 SDK not found` (or Arm Clang, SysConfig, DSLite), install that tool, or point to it with `ESPHOME_TI_F3_SDK`, `ESPHOME_TI_CGT`, `ESPHOME_TI_SYSCONFIG` or `ESPHOME_TI_DSLITE`.

3. **Mac, terminal.** Copy the `components/` folder and `frl-cc27xx.yaml` from `ports/esphome-ti` into one empty folder, and run the remaining steps there. Set `probe_serial:` in the YAML to your debug probe's serial number, or leave it out and pass `--device <your-probe-serial>` to `upload`. `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e` prints the serial.

   ```text
   cp -R <repository folder>/ports/esphome-ti/components <repository folder>/ports/esphome-ti/frl-cc27xx.yaml .
   ```

   You should see: The folder holds `components/` and `frl-cc27xx.yaml`.

   If ESPHome later says `Cannot upload Over the Air as the ota configuration is not present`, run `upload` from the folder that holds `components/`, with ESPHome 2026.9.1.

4. **Mac, terminal.** Build the firmware. The YAML adds the `cc27xx` platform with `external_components`.

   ```text
   ./venv/bin/esphome compile frl-cc27xx.yaml
   ```

   You should see: The build ends without errors in 35 s cold, 27 s warm. The image is 262.7 kB of flash and 37.6 kB of RAM (124.4 kB free).

   If it stops with `TI F3 SDK not found` (or Arm Clang, SysConfig, DSLite), a tool is missing: see step 2.

5. **Home Assistant, browser.** Open the join window before you flash: Settings, Devices, Zigbee Home Automation, Add device. It stays open for 4 minutes.

   You should see: Home Assistant shows that it is searching for devices.

   If the window closes before the board is flashed, open it again and reset the board.

6. **Mac, terminal.** Flash the board. Plug in the debug probe first; leave `--device` out only when `probe_serial:` is set in the YAML.

   ```text
   ./venv/bin/esphome upload frl-cc27xx.yaml --device <your-probe-serial>
   ```

   You should see: The upload takes about 15 s. The device appears in Home Assistant 5 to 6 s after the flash, with an LED switch and a chip temperature sensor.

   If a second flash never rejoins while the old device is still listed in Home Assistant, remove the device from ZHA first and flash again (cause unknown). If DSLite reports `Error -2001` or `Debug is not allowed`, read `cc2755p10/FAQ.md` before any chip erase.

7. **Home Assistant, browser.** Switch the LED entity on and off from its card.

   You should see: The red LED (DIO16) switches on and off; the state settles in a median of 85 to 92.5 ms (32 of 32 commands).

   If the device does not appear, the 4-minute join window may have closed: open it again and reset the board.

### Expected result

A Zigbee router built by `esphome compile` and flashed by `esphome upload` appears in Home Assistant's ZHA 5 to 6 s after the flash with an LED switch and a chip temperature sensor, and the red LED follows the switch. From a repeat compile (27 s) to a joined device took about a minute.

### What this shows

- ESPHome 2026.9.1's own `compile` and `upload` commands build and flash a TI chip through the `cc27xx` platform in this repository; no ESPHome file was changed.
- The device joins Home Assistant and its LED switch worked 32 of 32 times, with a median of 85 to 92.5 ms from command to settled state.
- The image is 262.7 kB of flash and 37.6 kB of RAM, with 124.4 kB of RAM free.

### Limits

- That it works on Windows or Linux, or with another ESPHome, SDK or compiler version: only the versions in step 1 and 2 were built, on macOS.
- That the device is stable over days: there was no long soak test.
- That every ESPHome component works: only the Zigbee switch and a chip temperature sensor were exercised.
- A second flash with the old device still listed in Home Assistant never rejoined; removing the device first fixed it (cause unknown).
- `esphome logs` has nothing to read: the platform has no logging.
- Pins are written as DIO numbers; only the LEDs are mapped (red DIO16, green DIO12).

### Numbers and sources

| | Value | Source |
|---|---|---|
| ESPHome | 2026.9.1 in a Python 3.12 virtual environment, `cbor2<6` on an Intel Mac | `ports/esphome-ti/README.md` |
| TI tools | SimpleLink Low Power F3 SDK 9.21.00.36, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, UniFlash 9.6.0 | `ports/esphome-ti/README.md` |
| Image | 262.7 kB of flash, 37.6 kB of RAM, 124.4 kB RAM free (router) | `docs/experiments/2026-10-13-esphome-cc27xx-platform.md` |
| Build time | 35 s cold, 27 s warm (median of three, Mac busy) | `docs/experiments/2026-10-13-esphome-cc27xx-platform.md` |
| Flash time | 15 s (14.6 to 16.1 s) | `docs/experiments/2026-10-13-esphome-cc27xx-platform.md` |
| Join | 5.7 s after the first flash, 6.3 s after the third | `docs/experiments/2026-10-13-esphome-cc27xx-platform.md` |
| LED switch | 32 of 32 commands, median 85.0 to 92.5 ms | `docs/experiments/2026-10-13-esphome-cc27xx-platform.md` |

## ESPHome Zigbee end device on the LP-EM-CC2340R5

The same compile and upload on the 36 kB Bluetooth chip, as a Zigbee end device; it joins Home Assistant in 7 to 19 s.

Status: Works. Boards: cc2340r5.

Folders this version uses (in a sparse clone, fetch them first, then read `ports/esphome-ti/AGENTS.md`):

```bash
git sparse-checkout add ports/esphome-ti
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-EM-CC2340R5 LaunchPad development kit | 1 | the board |
| LP-XDS110ET debugger | 1 | the debugger that programs the board |
| Home Assistant with the Zigbee Home Automation (ZHA) integration | 1 | the hub the board joins |
| A Mac to build and flash from | 1 | runs ESPHome and the flasher |

### Before you start

- A Mac. Only macOS was tried, on an Intel Mac. ESPHome 2026.9.1 and the TI tools of steps 1 and 2 at exactly these versions: other versions were not run.
- Home Assistant with the Zigbee Home Automation (ZHA) integration and a working Zigbee coordinator.
- Close any CCStudio debug connection before step 6; it holds the debug probe.
- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`.

### Steps

1. **Mac, terminal.** Make a Python 3.12 virtual environment and install ESPHome 2026.9.1 into it. The `cbor2<6` pin is needed on an Intel Mac, where cbor2 6 has no wheel and wants a Rust compiler.

   ```text
   python3.12 -m venv venv && ./venv/bin/pip install "cbor2<6" esphome==2026.9.1
   ```

   You should see: pip finishes in about 26 s and the environment holds 222 MB.

   If pip stops asking for a Rust compiler, the `cbor2<6` pin is missing.

2. **Mac, installers.** Install TI's tools once, about 8 GB in all: the SimpleLink Low Power F3 SDK 9.21.00.36, TI Arm Clang 5.1.1.LTS (it comes with CCStudio 21.0.1), SysConfig 1.26.3 and UniFlash 9.6.0. The downloads need a free myTI login. The platform looks under `/Applications/ti`, `~/ti` and `/opt/ti`.

   You should see: The four folders sit under `/Applications/ti`.

   If a build later stops with `TI F3 SDK not found` (or Arm Clang, SysConfig, DSLite), install that tool, or point to it with `ESPHOME_TI_F3_SDK`, `ESPHOME_TI_CGT`, `ESPHOME_TI_SYSCONFIG` or `ESPHOME_TI_DSLITE`.

3. **Mac, terminal.** Copy the `components/` folder and `frl-cc23xx.yaml` from `ports/esphome-ti` into one empty folder, and run the remaining steps there. Set `probe_serial:` in the YAML to your debug probe's serial number, or leave it out and pass `--device <your-probe-serial>` to `upload`. `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e` prints the serial.

   ```text
   cp -R <repository folder>/ports/esphome-ti/components <repository folder>/ports/esphome-ti/frl-cc23xx.yaml .
   ```

   You should see: The folder holds `components/` and `frl-cc23xx.yaml`.

   If ESPHome later says `Cannot upload Over the Air as the ota configuration is not present`, run `upload` from the folder that holds `components/`, with ESPHome 2026.9.1.

4. **Mac, terminal.** Build the firmware. The YAML adds the `cc23xx` platform with `external_components`.

   ```text
   ./venv/bin/esphome compile frl-cc23xx.yaml
   ```

   You should see: The build ends without errors in 31 s cold. The image is 219.9 kB of flash and 32.0 kB of RAM (4.0 kB free).

   If the link stops with `program will not fit into SRAM` you built the router (`frl-cc23xx-router.yaml`); use `frl-cc23xx.yaml`, or set `heap_size: 0x23BC` in the `cc23xx:` block and accept that no heap is left over.

5. **Home Assistant, browser.** Open the join window before you flash: Settings, Devices, Zigbee Home Automation, Add device. It stays open for 4 minutes.

   You should see: Home Assistant shows that it is searching for devices.

   If the window closes before the board is flashed, open it again and reset the board.

6. **Mac, terminal.** Flash the board. Plug in the debug probe first; leave `--device` out only when `probe_serial:` is set in the YAML.

   ```text
   ./venv/bin/esphome upload frl-cc23xx.yaml --device <your-probe-serial>
   ```

   You should see: The upload takes about 15 s. The device appears in Home Assistant 7 to 19 s after the flash, with an LED switch and a chip temperature sensor.

   If a second flash never rejoins while the old device is still listed in Home Assistant, remove the device from ZHA first and flash again (cause unknown). If DSLite reports `Error -2001` or `Debug is not allowed`, read `cc2755p10/FAQ.md` before any chip erase.

7. **Home Assistant, browser.** Switch the LED entity on and off from its card.

   You should see: The red LED (DIO14) switches on and off. 93 of 100 commands were answered over five runs, 20 of 20 in the last.

   If a command takes 2 to 3 s or times out, the end device is sleeping between polls (every 3 s). The platform keeps its receiver on unless the YAML has `zigbee: sleepy: true`; rebuild with ESPHome 2026.9.1 and the current `components/`.

### Expected result

An end device built by `esphome compile` and flashed by `esphome upload` appears in Home Assistant's ZHA 7 to 19 s after the flash with an LED switch and a chip temperature sensor, and the red LED follows the switch.

### What this shows

- The `cc23xx` platform builds and flashes a CC2340R5, a chip with 36 kB of RAM, from ESPHome's own commands.
- As an end device with its receiver on it joined 7 to 19 s after the flash and answered 93 of 100 commands over five runs, 20 of 20 in the last.
- The image is 219.9 kB of flash and 32.0 kB of RAM, with 4.0 kB of RAM free.

### Limits

- That the router form is comfortable: it needs `heap_size: 0x23BC` and then has no heap left over (it ran 50 of 50 commands in a 7-minute run).
- That a sleepy end device is usable: with TI's default sleep a command took 2 to 3 s and some failed.
- That it works on Windows or Linux, or with other tool versions.
- 4.0 kB of RAM is free on the end device; a larger YAML may not link.
- The chip temperature sensor reads about 10 C below the room (cause not found).
- Pins are written as DIO numbers; only the LEDs are mapped (red DIO14, green DIO15).

### Numbers and sources

| | Value | Source |
|---|---|---|
| ESPHome | 2026.9.1 in a Python 3.12 virtual environment, `cbor2<6` on an Intel Mac | `ports/esphome-ti/README.md` |
| TI tools | SimpleLink Low Power F3 SDK 9.21.00.36, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, UniFlash 9.6.0 | `ports/esphome-ti/README.md` |
| Image | 219.9 kB of flash, 32.0 kB of RAM, 4.0 kB RAM free (end device) | `docs/experiments/2026-10-13-esphome-cc27xx-platform.md` |
| Build time | 31 s cold | `docs/experiments/2026-10-13-esphome-cc27xx-platform.md` |
| Flash time | 13.7 to 16.7 s | `docs/experiments/2026-10-13-esphome-cc27xx-platform.md` |
| Join | 7 to 19 s after the flash | `docs/experiments/2026-10-13-esphome-cc27xx-platform.md` |
| Commands | 93 of 100 over five runs, 20 of 20 in the last | `docs/experiments/2026-10-13-esphome-cc27xx-platform.md` |

## ESPHome Wi-Fi device on the LP-EM-CC35X1

ESPHome's own compile and upload put a Wi-Fi device with ESPHome's native API on the CC3551E; Home Assistant adoption has not run.

Status: Could work. Boards: cc35x1.

Folders this version uses (in a sparse clone, fetch them first, then read `ports/esphome-ti/AGENTS.md`):

```bash
git sparse-checkout add ports/esphome-ti
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-EM-CC35X1 LaunchPad development kit | 1 | the board |
| LP-XDS110ET debugger | 1 | the debugger that programs the board |
| A Mac to build and flash from | 1 | runs ESPHome and the flasher |
| A Wi-Fi network the board can join | 1 | the board's network |

### Before you start

- A Mac. Only macOS was tried, on an Intel Mac. ESPHome 2026.9.1 from step 1 of the Zigbee versions (same virtual environment).
- TI's SimpleLink Wi-Fi SDK 10.20.00.39, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0 and the SimpleLink Wi-Fi Toolbox 4.3.23, and the LP-EM-CC35X1 activated once with the Toolbox. Override paths with `ESPHOME_TI_WIFI_SDK`, `ESPHOME_TI_CGT`, `ESPHOME_TI_SYSCONFIG_WIFI`, `ESPHOME_TI_TOOLBOX`.
- Close any other program that holds the debug probe before step 4.
- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`.

### Steps

1. **Mac, terminal.** Copy `components/` and `frl-cc35xx.yaml` from `ports/esphome-ti` into one folder, and write `secrets.yaml` beside them with `wifi_ssid:` and `wifi_password:`.

   ```text
   cp -R <repository folder>/ports/esphome-ti/components <repository folder>/ports/esphome-ti/frl-cc35xx.yaml .
   printf 'wifi_ssid: "<your network>"\nwifi_password: "<your password>"\n' > secrets.yaml
   ```

   You should see: The folder holds `components/`, `frl-cc35xx.yaml` and `secrets.yaml`.

   If compile stops with `KeyError: 'power_save_mode'` (or `max_connections`, `max_send_queue`), keep those three keys in the YAML; they have no platform default on `cc35xx`.

2. **Mac, terminal.** Build the firmware.

   ```text
   ./venv/bin/esphome compile frl-cc35xx.yaml
   ```

   You should see: The build ends without errors in 56 s cold, 23 s when repeated.

   If a tool is missing the build stops with its name and version; install it or set its override variable.

3. **Mac, terminal.** Flash the board. This erases the whole flash and writes the signed image.

   ```text
   ./venv/bin/esphome upload frl-cc35xx.yaml --device <your-probe-serial>
   ```

   You should see: The upload takes about 60 s.

   If nothing appears on the serial port afterwards, read the port at 115200 baud; programs from the Toolbox start by themselves after a full erase. An image programmed over TI's Wi-Fi CSI image without the erase stays silent (see the CC35X1 FAQ).

4. **Mac, terminal.** Read the board's application serial port at 115200 baud, and reset the board.

   You should see: ESPHome's log appears and shows `[I][wifi]: Connected` about 5 s after the reset, then the API waiting for a client.

   If the log stops after `Starting scan`, use ESPHome 2026.9.1 with this platform's current Wi-Fi backend.

### Expected result

The board prints ESPHome's log on its serial port, joins your Wi-Fi about 5 s after a reset and serves ESPHome's native API on port 6053. Adding it to Home Assistant's ESPHome integration has not been run.

### What this shows

- ESPHome 2026.9.1's own `compile` and `upload` commands build and flash a CC3551E through the `cc35xx` platform in this repository.
- The board joined Wi-Fi in 5.3 to 5.7 s after a reset and an API client on the board confirmed 20 of 20 switch commands.
- Over a 24-hour soak 5,616 of 5,620 commands were confirmed and the board never restarted.

### Limits

- That Home Assistant adopts the device: the test network isolates its clients, so port 6053 was not reachable from Home Assistant.
- That the API is secure: it runs without encryption until TI's random-number driver is wired in.
- That it survives a power cycle with its settings: preferences do not persist.
- The API runs without encryption, settings do not survive a reset, the network name and password are built into the image, and there are no over-the-air updates.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Image | 1.2 MB of flash used; 74.0 kB main RAM and 86.7 kB TCM free after the link | `docs/experiments/2026-10-13-esphome-cc35xx-platform.md` |
| Build time | 55.7 s cold, 23.3 s warm | `docs/experiments/2026-10-13-esphome-cc35xx-platform.md` |
| Reset to address | 5.3 s median of two clean joins | `docs/experiments/2026-10-13-esphome-cc35xx-platform.md` |
| Switch command, on the board | 120 ms median, 20 of 20 confirmed | `docs/experiments/2026-10-13-esphome-cc35xx-platform.md` |
| Free heap | 139.7 kB running, 135.7 kB lowest in the 24-hour soak |  |
