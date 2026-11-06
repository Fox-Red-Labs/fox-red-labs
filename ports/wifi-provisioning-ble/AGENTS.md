# Giving an LP-EM-CC35X1 its Wi-Fi network over Bluetooth: notes for an AI coding assistant

The image reuses TI's own provisioning service, the Mac's Bluetooth is the usual first failure, and only the boot, the advertising and the join timeout have run on the board; the Bluetooth write, the join and the first MQTT message have not. Each item below comes from builds and boots on one board.

Terms: [docs/vocab.md](../../docs/vocab.md) and the [README](README.md) in the Fox Red Labs repo. In short: **MQTT** is a small publish and subscribe message protocol; a **broker** is the server that passes MQTT messages on; **bleak** is the Python Bluetooth LE client library; the **Toolbox** is TI's SimpleLink Wi-Fi Toolbox, which signs and programs the image; a **GATT service** is a group of values a client can read, write or be notified of. 1 kB = 1,024 bytes. Checked 2026-10-25 on an Intel Mac, macOS 15.7.9, with SimpleLink Wi-Fi SDK 10.20.00.39, Toolbox 4.3.23, SysConfig 1.28.0 and TI Arm Clang 5.1.1.LTS, on one LP-EM-CC35X1 Rev A with an LP-XDS110ET.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths below in backticks starting `ports/` or `cc35x1/` are in the Fox Red Labs repo, where the evidence for each line lives; they are not in your copy, and links in this file only work inside that repo. The files to copy are listed in "Take it with you" in `ports/wifi-provisioning-ble/README.md`.

## The port

| Part | Fact |
|---|---|
| What it is | `provision.py` on a computer writes the network over TI's `ble_wifi_provisioning` GATT service (`0xCC00`: name `0xCC01`, password `0xCC02`, connection `0xCC03`, security `0xCC04`) to the chained demo image (`ports/wifi-provisioning-ble/README.md`) |
| Image | TI's `mqtt_client`, `ble_wifi_provisioning` and `ota_example` pieces in one build: 1,337 kB of flash, 22 kB of RAM free, signed with the SDK example key (`cc35x1/chained-demo/README.md`) |
| Ran on the board | Boot with no network stored, advertising as `TI CC3xxx` 0.90 s after the banner; a network it cannot find gives up after 16.1 to 16.2 s and advertises 0.2 s later, 5 of 5 |
| Limits | The Bluetooth write, the join, the IP address, the first MQTT message, the rejoin after reset, the wrong-password notification, phone apps (`ports/wifi-provisioning-ble/README.md`, "Limits") |
| Notifications | `1` join started, `2` joined, `3` failed (`ports/wifi-provisioning-ble/provision.py`) |
| Security values | `0` open, `2` WPA2, `12` WPA3 (`ports/wifi-provisioning-ble/provision.py`) |

## Start here

| Step | This port |
|---|---|
| Tools | SimpleLink Wi-Fi SDK 10.20.00.39, Toolbox 4.3.23, SysConfig 1.28.0, TI Arm Clang 5.1.1.LTS at the paths at the top of `cc35x1/chained-demo/build.sh`; Python 3 with `bleak`, `paho-mqtt`, `pyserial` |
| Settings | Copy `chained_config.example.h` outside the repo; set `CHAINED_BROKER_HOST`, `CHAINED_BROKER_PORT`, `CHAINED_CMD_TOKEN` (`openssl rand -hex 16`); leave `CHAINED_DEFAULT_SSID` empty |
| Build | `OUT_ROOT=<build folder> CONFIG=<settings file> cc35x1/chained-demo/build.sh` (about 4 minutes) |
| Probe serial | `xdsdfu -e` from CCStudio's `ccs_base/common/uscif/xds110/` folder |
| Program | `./simplelink-wifi-toolbox programmer -i XDS110 -param1 <probe serial> programming --tool_settings <build folder>/v0.0.1.0/toolbox/tool_settings.json` from the Toolbox folder (56 s), then `xds110reset -s <probe serial>` |
| Check | Console at 115200 on `/dev/cu.usbmodem<probe serial>1` shows `[APP] stored credentials: none` and `[PROV] advertising` |
| Provision | `WIFI_PASSWORD='<password>' python3 provision.py --ssid '<network name>'`; exit 0 joined, 3 failed, 4 nothing came back |
| Forget | `mosquitto_pub -h <broker> -p <port> -t frl/cc35x1/cmd -m 'token=<token> forget-wifi'`, or hold SW2 for 3 s |

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| ``Timeout expired connecting to Network`` with only ``[WLAN EVENT] Unexpected event [0x19]`` before it | The board never found the network name; on macOS 15 `ipconfig getsummary en0` prints `<redacted>` instead of the name | Type the network name yourself | `[WIFI] connected, addr=` (`cc35x1/FAQ.md`) |
| `provision.py` or any bleak scan never returns on a Mac | The Mac's Bluetooth controller is down: `State: Off` with `Address: NULL` | Turn Bluetooth off and on in Control Center | `system_profiler SPBluetoothDataType` shows `State: On` (`cc35x1/FAQ.md`) |
| `Bluetooth is not authorized` over SSH | macOS gives Bluetooth only to apps allowed in Privacy & Security | Run the client from a Terminal window on that Mac | The scan returns |
| About 100 lines of `ERROR txCtrl_TxCompleteCb:not valid uDescId` during the radio firmware load | Not traced | Ignore them | The image advertises after them (`ports/wifi-provisioning-ble/README.md`) |
| Build stops with only a killed linker or `[PYI-36117:ERROR] Failed to execute script 'simplelink-wifi-toolbox'` | The disk is down to a few hundred MB | Free disk, rerun | Signing ends with a `.sign.bin` (`cc35x1/FAQ.md`) |

## Rules for the assistant

1. **Never write a real network name, password, broker address or token into a tracked file.** Settings go in a file outside the project; the password is read from `WIFI_PASSWORD` or a file and is never printed (`ports/wifi-provisioning-ble/provision.py`).
2. **Say the Bluetooth write, join and publish have not run.** Only the boot, the advertising and the fallback timing are measured (`ports/wifi-provisioning-ble/README.md`).
3. **Reuse TI's provisioning service.** The image compiles it unmodified from the SDK (`ports/wifi-provisioning-ble/README.md`, "What runs on the board").
4. **Take the probe lock when the board is shared, and know there is no backup.** The program sits in external flash that the Toolbox cannot read back, so keep the output of its `get_fuse_data` command instead (`cc35x1/AGENTS.md`).
