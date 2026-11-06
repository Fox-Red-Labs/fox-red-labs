# Give an LP-EM-CC35X1 its Wi-Fi network over Bluetooth, then publish to MQTT

The image builds (1,337 kB of flash, 22 kB of RAM free), boots with no network stored and advertises as `TI CC3xxx` 0.90 s after its banner. The design: the board takes the network name and password from a computer or phone over Bluetooth LE, joins Wi-Fi, saves the network in flash, connects to an MQTT broker and publishes. After a reset it joins again without Bluetooth. The Bluetooth write, the join and the first MQTT message have not run (see Limits).

Terms: [vocab](../../docs/vocab.md). Board setup, activation and the programmer: [cc35x1/README.md](../../cc35x1/README.md). Problems and fixes: [cc35x1/FAQ.md](../../cc35x1/FAQ.md). 1 kB = 1,024 bytes.

| Step | Status | Result |
|---|---|---|
| Build and sign | Works | 1,337 kB of flash, 168 kB static RAM plus a 319 kB heap, 22 kB free; signed with the SDK example key |
| First boot, no network stored | Works | `[APP] stored credentials: none`, then advertising as `TI CC3xxx`, 0.90 s after the banner |
| A compiled-in network the board cannot find | Works as designed | The join gives up 16.1 to 16.2 s after the station starts, then advertising starts 0.2 s later, 5 of 5 runs |

## What runs on the board

The image is the [chained demo](../../cc35x1/chained-demo/README.md). It is built from TI's own pieces, compiled from the SimpleLink Wi-Fi SDK folder:

- TI's `ble_wifi_provisioning` GATT service (`nimble_host_provisioning.c`, unmodified): service `0xCC00`, network name `0xCC01`, password `0xCC02`, connection `0xCC03` (a read starts the join; notifications `1` started, `2` joined, `3` failed), security type `0xCC04` (`0` open, `2` WPA2, `12` WPA3).
- TI's `mqtt_client` interface for the broker, plain TCP with no login.
- TI's NVOCMP flash storage for the saved network, with a CRC.

No other vendor's code is used.

## Steps from a Mac

Needs: the board activated (`cc35x1/README.md`), the Wi-Fi SDK, Toolbox, SysConfig and compiler from that README, Python 3 with `pip install bleak paho-mqtt`, and an MQTT broker on the same network that accepts clients with no login.

1. Copy `cc35x1/chained-demo/chained_config.example.h` to a file outside the repo. Set `CHAINED_BROKER_HOST` and `CHAINED_BROKER_PORT`, and `CHAINED_CMD_TOKEN` from `openssl rand -hex 16`. Leave `CHAINED_DEFAULT_SSID` empty, so the board asks over Bluetooth.
2. Build and sign (about 4 minutes on an Intel Mac):

   ```bash
   OUT_ROOT=<build folder> CONFIG=<your settings file> cc35x1/chained-demo/build.sh
   ```

3. Program it from the Toolbox folder, then reset the board:

   ```bash
   ./simplelink-wifi-toolbox programmer -i XDS110 -param1 <probe serial> programming --tool_settings <build folder>/v0.0.1.0/toolbox/tool_settings.json
   xds110reset -s <probe serial>
   ```

   Programming took 56 s. Over the OTA example's two-slot layout no erase is needed; both use the same layout.
4. Read the console at 115200 on `/dev/cu.usbmodem<probe serial>1`. Expect `[APP] stored credentials: none` and `[PROV] advertising`.
5. Watch the broker: `mosquitto_sub -h <broker> -p <port> -t 'frl/cc35x1/#' -v`.
6. Give the board your network:

   ```bash
   WIFI_PASSWORD='<password>' python3 provision.py --ssid '<network name>'
   ```

   It prints `joined` and the seconds since the connect request, or `join failed`. The console shows `[PROV] joined; credentials saved` and `[WIFI] connected, addr=...`, and the broker shows `frl/cc35x1/status online 0.0.1.0`, then telemetry every `CHAINED_PUBLISH_PERIOD_S` seconds.
7. Reset the board. It joins with the saved network and prints no `[PROV]` lines.
8. To clear the network: `mosquitto_pub -h <broker> -p <port> -t frl/cc35x1/cmd -m 'token=<token> forget-wifi'`, or hold SW2 for 3 s. The board restarts and advertises again.

To time all of it, run `bench_run.py --mode provision --runs 5`, then `--mode reset` and `--mode wrongpw` (settings are environment variables, listed at the top of the script). It takes the Bluetooth, console and broker times on one clock.

**From a phone.** TI's README for `ble_wifi_provisioning` names its SimpleLink Connect app: connect to `TI CC3xxx`, open "Wi-Fi Provisioning Over BLE", enter the network and press Connect. A generic Bluetooth LE app that writes characteristics, such as nRF Connect, can write `0xCC01`, `0xCC02` and `0xCC04`, then read `0xCC03`. Neither app has been used with this image.

## Take it with you

Two parts: a computer-side script that needs only Python, and the board image, which is built from files in this repo plus TI's SDK. Copy them keeping the folder names (`cc35x1/chained-demo/` and `portability/iot-chain/` as sibling paths under one top folder), because the build script finds the second from the first.

| File | What it is |
|---|---|
| `ports/wifi-provisioning-ble/provision.py` | Writes the network name, password and security type over Bluetooth LE and reports `joined` or `join failed`; runs on Mac, Linux or Windows with `pip install bleak`; no other file is needed |
| `ports/wifi-provisioning-ble/bench_run.py` | Optional: times provisioning, rejoin and wrong-password runs; needs `pyserial`, `paho-mqtt`, a broker and a probe, and imports `provision.py` from the same folder |
| `cc35x1/chained-demo/build.sh`, `makefile` | Builds and signs the image; the makefile is TI's `mqtt_client` makefile plus the new objects |
| `cc35x1/chained-demo/chained_app.c`, `prov_glue.c`, `prov_glue.h`, `ota_glue.c`, `ota_glue.h`, `cred_nv.c`, `cred_nv.h` | Application: Bluetooth provisioning, saved network, MQTT publishing, over-the-air update |
| `cc35x1/chained-demo/wifi_if.c`, `wifi_if_creds.h` | Changed copy of TI's `wifi_if.c` (four marked changes) and its header |
| `cc35x1/chained-demo/chained_config.example.h` | Settings: broker host and port, command token, topic prefix, publish period; copy it elsewhere and edit |
| `portability/iot-chain/core/` | `cred_record` and `cmd_parse`: the credential record with a CRC and the command parser, board-independent C |
| `portability/iot-chain/test/` | Host test that `build.sh` runs before it builds (`run.sh`, `host_test.c`) |

TI's own `nimble_host_provisioning.c`, the `mqtt_client` interface and the NVOCMP storage are compiled from the SDK folder and are not copied.

**What it needs.**

- SimpleLink Wi-Fi SDK 10.20.00.39, SimpleLink Wi-Fi Toolbox 4.3.23 (it signs the image), SysConfig 1.28.0 and TI Arm Clang 5.1.1.LTS. `build.sh` has these four install folders at the top; change them if yours differ. It needs 600 MB free on `/`.
- Settings: `CONFIG=<your copy of chained_config.example.h>`. With none, the example's placeholders are used and the board asks for a network over Bluetooth.
- No defines and no force-include flag. The makefile sets `-DTHREADSTACKSIZE=10752` and the include paths itself.
- Python 3 with `bleak` for `provision.py`; on a Mac the terminal app must be allowed Bluetooth in Privacy & Security.

**A different board in the same family.** Only the LP-EM-CC35X1 was built. The board-specific parts are TI's SDK paths in the makefile (`DEMOS`, `MQTT`, `PROV`, `OTA` point at `examples/rtos/LP_EM_CC35X1/demos/...`) and the syscfg it reads (`mqtt_client.syscfg`, unchanged); a sibling board needs those paths and its own `mqtt_client` demo.

**Build from a fresh folder** (`<top>` holds `cc35x1/chained-demo/` and `portability/iot-chain/`):

```bash
cd <top>
OUT_ROOT=<build folder> CONFIG=<settings file> cc35x1/chained-demo/build.sh      # about 4 minutes; signed images in <build folder>/v0.0.1.0/toolbox/
```

Built from a copy of only these files on 2026-10-25 (copy outside the repo, placeholder settings, zero errors): the host test passed 109 of 109, the image linked in 56 s at 1,337 kB of flash with 22 kB of RAM free, and the Toolbox wrote 20 signed `.bin` files. The image was not programmed from that build.

## If you see this

| What you see | Do this |
|---|---|
| The join prints `Timeout expired connecting to Network`, and the log shows only `[WLAN EVENT] Unexpected event [0x19]` before it | The board never found the network name. Event `0x19` is `WLAN_EVENT_CONNECT_PERIODIC_SCAN_COMPLETE`. Check the name; on macOS 15, `ipconfig getsummary en0` prints the placeholder `<redacted>` instead of the network name, so a build that reads the name from it joins nothing. Type the name yourself |
| `provision.py` or any `bleak` scan never returns on a Mac | Check `system_profiler SPBluetoothDataType`: `State: Off` with `Address: NULL` means the Mac's Bluetooth controller is down. Turn Bluetooth off and on in Control Center |
| `bleak` says `Bluetooth is not authorized` when run over SSH on a Mac | macOS gives Bluetooth only to apps allowed in Privacy & Security. Run the client from a Terminal window on that Mac |
| About 100 lines of `ERROR txCtrl_TxCompleteCb:not valid uDescId` while the radio firmware loads | Ignore them; the image starts and advertises after them |

## Numbers and how they were taken

Console lines were stamped on the Mac as they arrived (`bench_run.py`, 115200 baud). The fallback times are from 5 resets of an image with a network name the board could not find. Sizes are from the linker map. Wi-Fi SDK 10.20.00.39, TI Arm Clang 5.1.1.LTS, Toolbox 4.3.23, SysConfig 1.28.0, one LP-EM-CC35X1 Rev A on an LP-XDS110ET, Intel Mac with macOS 15.7.9. Rows are in `data/results/cc35x1.csv`, variants starting `wifi-ble-prov`, `wifi-stored`, `wifi-unknown-network` and `wifi-credentials`.

For comparison, TI's unmodified `ble_wifi_provisioning` example joined 7.13 s after the connect request (two runs, 2026-10-09), and TI's `mqtt_client` answered 720 of 720 requests (`cc35x1/README.md`).

## Limits

Not checked: the Bluetooth write, join, IP address and first MQTT message; the reset rejoin; the wrong-password notification (TI's service notifies `3` on a failed join); the phone apps; Wi-Fi and Bluetooth at the same time (TI's release notes list coexistence issue OSPREY_MX-1518).

## License

TI's files (`nimble_host_provisioning.c`, the `mqtt_client` interface, NVOCMP) are BSD-3-Clause, Copyright Texas Instruments, compiled from the SDK; `cc35x1/chained-demo/prov_glue.c` and `wifi_if.c` are TI files with marked changes and keep TI's notice. `provision.py` and `bench_run.py` here are BSD-3-Clause.
