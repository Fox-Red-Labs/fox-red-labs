# Can one LP-EM-CC35X1 image do Bluetooth setup, Wi-Fi, MQTT and over-the-air update in a row?

**It builds: one image links at 1,337 kB of flash and leaves 22 kB of RAM free, and the Toolbox signs it. On a board it has booted and advertised, but the Bluetooth write, the join and MQTT have not run.** On first boot it takes the Wi-Fi name and password from a phone over Bluetooth LE, saves them to flash, joins Wi-Fi, publishes to an MQTT broker, takes commands, and installs a new image when an MQTT message carrying a shared token tells it to, from one configured server only, then waits for an "accept" before keeping it. First boot on a board (2026-10-25): with no network stored it advertises 0.90 s after the banner, and a network it cannot find times out in 16.2 s and falls back to advertising, 5 of 5 runs ([run record](../../ports/wifi-provisioning-ble/README.md)).

Terms: [docs/vocab.md](../../docs/vocab.md). Board problems and fixes: [cc35x1/FAQ.md](../FAQ.md). Board setup and activation: [cc35x1/README.md](../README.md). 1 kB = 1,024 bytes.

Built 2026-10-08 on an Intel Mac, macOS 15.7.9: SimpleLink Wi-Fi SDK 10.20.00.39, SimpleLink Wi-Fi Toolbox 4.3.23, SysConfig 1.28.0, TI Arm Clang 5.1.1.LTS (TI tested this SDK with 4.0.4.LTS; 5.1.1 is what is installed).

Contents: what each TI demo does alone, what was merged, flash and RAM, Wi-Fi and Bluetooth at once, security of this demo, build it, test plan, what is left.

## What does each TI demo do on its own?

TI ships these as three separate examples in `examples/rtos/LP_EM_CC35X1/demos/`:

| TI demo | What it does | What it does not do |
|---|---|---|
| `ble_wifi_provisioning` | Advertises over Bluetooth LE as "TI CC3xxx". TI's SimpleLink Connect phone app writes the network name, security type and password; the board joins | Keeps the credentials in RAM only, so every reboot needs the phone. No MQTT |
| `mqtt_client` | Joins the network named in `wifi_settings.h` at compile time, connects to a broker, subscribes to four topics, publishes on a button press | Its header says `WIFI_IF_PROVISIONING_SUPPORT (0)`, "PROVISIONING IS NOT SUPPORTED". TLS and broker login: "not implemented" |
| `ota_example` | A serial menu: join Wi-Fi, query TI's Python HTTPS server, download, install, reboot, then accept or reject the trial image | Every step needs a person at the serial terminal |

## What was merged, and what of TI's code changed?

The image starts from `mqtt_client` and adds the other two. TI's files are compiled in place from the SDK folder, so they cannot drift. One TI file was copied and changed.

| Source | Files | Changed? |
|---|---|---|
| TI `mqtt_client` | `button_if.c`, `dns_if.c`, `led_if.c`, `mqtt_if.c`, `tcpip_if.c`, `crc.c`, `nvocmp_cc35xx.c`, `adaptation/*.c`, `freertos/main_freertos.c`, `mqtt_client.syscfg`, `linker.cmd`, all headers | No |
| TI `mqtt_client` | `wifi_if.c` (copy in this folder) | **Yes, four marked changes**: credentials come from a function call at run time instead of `AP_SSID` and `AP_PASSWORD`; with no credentials it brings the station role up without joining; a new `WIFI_IF_connectCredentials()` joins on an existing handle (TI's `WIFI_IF_connect()` needs a scan-list index); the timeout message prints the SSID in use |
| TI `ble_wifi_provisioning` | `nimble_host_provisioning.c` (the GATT service), its headers | No |
| TI `ble_wifi_provisioning` | `provSaveSSID`, `provSavePassword`, `provSaveSecurity`, `provClearCredentials`, `advConfigure`, `advEnable`, `initProvVariables`, and `cmdBleStartCallback` from `ble_cmd.c` | Copied unchanged into `prov_glue.c`, except that the BLE start function lost the network-terminal check and was renamed |
| TI `ota_example` | `ota_server.c`, `ota_fwu.c`, `https_client.c`, `network_mbedtls.c`, `ca_certificate.h` | No |
| TI `ota_example` | The call sequence of `ota_example.c` and the Wi-Fi firmware slot choice in `ota_wifi.c` | Re-used in `ota_glue.c`, with MQTT commands in place of the menu |
| This repo | `chained_app.c` (replaces `mqtt_client_app.c`), `prov_glue.c`, `ota_glue.c`, `cred_nv.c`, `makefile`, `build.sh` | New |
| This repo, any board | [`portability/iot-chain/core/`](https://github.com/Fox-Red-Labs/fox-red-labs/tree/main/portability/iot-chain/core): credential record with a CRC, MQTT command parser with the token and OTA host checks, telemetry text | New, with a host test (`portability/iot-chain/test/run.sh`, 109 of 109 checks pass) |

The makefile is TI's `mqtt_client` makefile plus the new objects, the FWU include paths, and `-DTHREADSTACKSIZE=10752`, both taken from TI's `ota_example` makefile (TLS runs on the main thread). The libraries are the same in all three TI makefiles, so none were added.

The credentials go in an NVOCMP item (TI's flash storage driver, the one TI's code uses for Bluetooth bonds). TI's syscfg turns on its SPS option, and the map file shows its key derivation (`NVOCMP_keyDerivationFromTkdk`) is linked, so the stored record should be encrypted with a key derived from the chip. Not checked on a board.

The telemetry is a counter, the uptime and the radio's own temperature reading (`Wlan_Get(WLAN_GET_CALIBRATOR_TEMPERATURE)`). The I2C temperature sensor on the LaunchPad development kit is left out: TI's `mqtt_client` README says the LED is "assembled incorrectly" and the I2C bus "is missing pull-ups". **Unknown (suspected: board assembly, as TI's README states).** Whether the calibrator temperature call answers outside TI's calibrator tool is **Unknown**; if it fails the field reads `null`, and `CHAINED_READ_CHIP_TEMP 0` turns it off.

## How much flash and RAM does it use, against each TI demo alone?

From the map files, same basis as the CC35X1 fragment: flash is the vector table plus code and constants; static RAM is DRAM used minus the FreeRTOS heap.

| Image | Flash (kB) | Static RAM without heap (kB) | FreeRTOS heap (kB) | DRAM free (kB) |
|---|---|---|---|---|
| `mqtt_client` | 1,235 | 111 | 319 | 79 |
| `ble_wifi_provisioning` | 1,287 | 168 | 328.5 | 12 |
| `ota_example` | 1,263 | 114 | 320 | 74 |
| **Chained image** | **1,337** | **168** | **319** | **22** |

Exact bytes: flash 1,369,170, static RAM 171,809, heap 326,640, DRAM free 22,334. The image uses half of its 2.63 MB flash slot, so two-slot updates have room. The thin part is RAM: the heap is `mqtt_client`'s 319 kB, while TI gave `ota_example` 320 kB "for TLS + OTA buffers" and `ble_wifi_provisioning` 328.5 kB. If the TLS download runs out of heap, the first change to try is raising it to `ble_wifi_provisioning`'s 328.5 kB in SysConfig (about 9.5 kB more, leaving about 12 kB free). **Unknown** until it runs.

## Will Wi-Fi and Bluetooth get in each other's way?

TI lists three known issues in the release notes for this SDK's Wi-Fi and Bluetooth firmware (`docs/wifi_ble_uppermac/release_notes_wifi_ble_uppermac_10_20_00_xx.html`):

- OSPREY_MX-1518: "[COEX] some degraded traffic and scan performance observed in Coexistence mode (Wi-Fi + BLE)"
- OSPREY_MX-1447: "[BLE] BLE Connection timeout occurred sometimes during heavy BLE traffic"
- OSPREY_MX-1519: "[BLE] instability observed in some multiple-connection BLE peripheral scenarios"

This image uses one Bluetooth connection, and only while it is being set up: advertising stops after a good join. The join itself happens while the phone is still connected, which is the case MX-1518 describes. **Unknown** until measured; step 6 of the test plan measures it.

## Security of this demo

Two guards stop other devices on the broker from driving the board:

- **A shared command token.** Every payload on the command topic must start with `token=<CHAINED_CMD_TOKEN>`, compared in constant time. A missing or wrong token is dropped with no reply on the broker. A configured token shorter than 16 characters refuses every command, so the placeholder in the example file cannot be used. Make one with `openssl rand -hex 16`. `forget-wifi` and every `ota` command need the token like the rest.
- **One update server.** `ota https://<host>[:port]` is refused unless the host equals `CHAINED_OTA_HOST` exactly, with no user info and no path. `evil.com/ota.host`, `ota.host.evil.com` and `user@ota.host` are all refused (host test). An empty `CHAINED_OTA_HOST` refuses every update. TI's OTA client also checks the server's TLS certificate against the CA built into the image.

What they do not do:

- **The demo's MQTT is plain TCP on port 1883.** Anyone who can see the network traffic can read the token and replay it. TI's MQTT code has no TLS or broker login. **TI example default.** For anything beyond the bench, use MQTT over TLS on port 8883, broker client authentication, and topic access rules so only your controller can publish to the command topic.
- **The images are signed with TI's SDK example key**, which ships with the SDK, so anyone can sign an image this board accepts. A product signs with its own vendor key, chosen when the chip is activated. That choice burns fuses and cannot be undone.
- Holding SW2 for 3 seconds still erases the stored network. That needs someone holding the board.

## How do I build it?

```bash
cc35x1/chained-demo/build.sh
```

It runs the host test, builds with the same tool paths as `cc35x1/prebuild.sh`, signs with the SDK example key (on the Mac only), prints the sizes, and deletes the object files. Output goes to `$HOME/tmp/_agent/runs/arrivals/cc35x1/chained-demo/v0.0.1.0/` unless you set `OUT_ROOT=<folder>` (the signed images are in `toolbox/`). It stops if fewer than 600 MB are free. A clean build took 35 to 37 seconds on this Mac (wall clock, with other jobs running).

For the update test, build a second image with a higher version:

```text
VERSION=0.0.2.0 cc35x1/chained-demo/build.sh
```

TI's syscfg leaves the Toolbox's image version at its default, 0.0.1.0, so `build.sh` writes `VERSION` into the generated `action_request_extra.txt` before signing. The header of each signed image reads back as its version (checked with the same byte offset TI's OTA server uses).

Settings: copy `chained_config.example.h` to `chained_config.h` in this folder and set the broker, `CHAINED_CMD_TOKEN` and `CHAINED_OTA_HOST`. `chained_config.h` is git-ignored; never commit a real network name, password or broker address. The example file builds with placeholders, and with it the board refuses every command.

## What is the test plan?

Needs: the board activated and mounted as in the [CC35X1 README](../README.md); SimpleLink Connect (TI's phone app named in TI's provisioning README) on a phone; a serial terminal at 115200 on the probe's port; an MQTT broker that accepts this client with no login (TI's MQTT code has none) on the same network. Either the broker on a second machine, or mosquitto on the Mac with a test listener (`listener 1883` and `allow_anonymous true`).

Watch everything the board sends:

```text
mosquitto_sub -h <broker> -t 'frl/cc35x1/#' -v
```

Send a command (replace `<token>` with your `CHAINED_CMD_TOKEN` and `<command>` with one from the table):

```text
mosquitto_pub -h <broker> -t frl/cc35x1/cmd -m 'token=<token> <command>'
```

| Step | Do | What proves it |
|---|---|---|
| 1. First boot | Program the 0.0.1.0 image, reset | Serial shows the banner with `0.0.1.0` and `[APP] stored credentials: none`, then `[PROV] advertising` |
| 2. Bluetooth setup | In SimpleLink Connect: connect to "TI CC3xxx", open "Wi-Fi Provisioning Over BLE", enter the network, press Connect. Start a stopwatch at power-up | Serial `[PROV] joined; credentials saved` and `[WIFI] connected, addr=...`; the app shows the connected status. Record power-up to IP address in seconds |
| 3. Reboot without the phone | Phone Bluetooth off, reset the board | Serial shows the stored network name and an IP address, with no `[PROV]` lines |
| 4. MQTT | Watch the broker; send `publish`, `led green`, `hello`, then `publish` with a wrong token and with none | `status` says `online 0.0.1.0`; `telemetry` arrives every 30 s with the count rising; `publish` adds one at once; `led green` toggles the LED; `hello` gets `unknown command`; the two bad-token messages get nothing back and the serial log shows `[CMD] unauthorized` |
| 5. Update | Copy TI's `tools/ota_https_server/` and the 0.0.2.0 build's `toolbox/primary_vendor_image.sign.bin` to one folder, run `python3 ota_server.py`, set `CHAINED_OTA_HOST` to that machine's address, send `ota https://<server-ip>:8443`. Also send one with another host | `status` shows querying, downloading, staged, rebooting; then `online, trial image 0.0.2.0`. The other host gets `unknown command`. Send `ota accept`: it reboots and reports `online 0.0.2.0`. Repeat with 0.0.3.0 and `ota reject`: it comes back as 0.0.2.0 |
| 6. Rollback with no broker | Update again, stop the broker before the reboot | After 300 s the trial image rejects itself and the old one boots |
| 7. Forget the network | Send `forget-wifi`, or hold SW2 for 3 s | It reboots into step 1's advertising |
| 8. Wi-Fi beside Bluetooth | Repeat step 2 five times with the phone connected and step 3 five times without | Join times in both cases. A large gap is the MX-1518 issue |

Which signed file the OTA server should serve is read from TI's server script: it classifies files with "vendor_image" in the name as the vendor image and reads the version from the image header. TI's README does not name the file. **Tool or docs.** If the server rejects it, try `primary_vendor_image.unsign.bin` next.

## What is left

- Everything in the test plan past first boot: nothing past step 1 has run.
- Timings for the run, recorded as rows in `data/results/cc35x1.csv` when it happens.
- If RAM runs short, the heap size in SysConfig (a change to TI's syscfg, which is unmodified today).
- Broker login and TLS for MQTT. TI's MQTT code has neither, and its publish call sends QoS 0 whatever QoS it is given (`mqtt_if.c`). **TI example default.**
