# Bluetooth proxy for Home Assistant on an ESP32-C6-DevKitC-1-N8 with ESPHome

Flash ESPHome's Bluetooth proxy onto the board, join it to Wi-Fi, and add it to Home Assistant by its IP address. On this bench the first compile took 324 s, the upload command 6 s (esptool's write 4.0 s), and the board joined Wi-Fi 8.8 s after the bootloader's first log line; Home Assistant added it by the three REST calls below (observed 2026-10-11; no log of the calls or their timing was saved). Measured 2026-10-11 on macOS Intel 15.7 with ESPHome 2026.9.1, which builds on its own copy of ESP-IDF 5.5.5 (the separate ESP-IDF v6.1 in the [FAQ](../FAQ.md) is for the benchmark board, not for this). 1 kB = 1,024 bytes.

Terms: [docs/vocab.md](../../docs/vocab.md). Problems with fixes: [FAQ](../FAQ.md). Why the proxy exists: Home Assistant runs in Docker on a Mac and cannot use the host's own Bluetooth, so nothing in the house could hear the BTHome beacon from the LP-EM-CC2340R5 (ports/bthome). A Bluetooth proxy is a small board that listens for Bluetooth advertisements and forwards them to Home Assistant over Wi-Fi.

## What you need

- ESP32-C6-DevKitC-1-N8. Other ESP32-C6 boards should work with `flash_size` changed in the YAML; only this one was run.
- A USB-C data cable into the port marked **USB** on the board (the chip's own USB, not the one marked UART)
- Python 3.12 to 3.14 on the Mac (ESPHome 2026.9.1's requirement on PyPI; 3.12 was used here)
- Home Assistant reachable on the same network
- About 3 GB of free disk for ESPHome and the ESP-IDF copy it downloads

## Steps

1. **Terminal.** Install ESPHome in a virtual environment (a private Python folder). The `--only-binary` flag matters on an Intel Mac: without it pip tries to build the `cbor2` package from source and stops with `error: can't find Rust compiler`.

```bash
python3.12 -m venv ~/esphome-venv
~/esphome-venv/bin/pip install --only-binary cbor2 esphome==2026.9.1
```

2. **Terminal.** Copy `secrets.example.yaml` to `<scratch>/secrets/esp32c6/secrets.yaml` (outside the repository) and fill in the Wi-Fi name and password, an API key (`openssl rand -base64 32`) and any OTA password. Never put the real file in the repository: `esphome.sh` copies it into a build folder outside the tree for each run. Never commit it.

3. **Terminal.** Compile. The first run downloads ESP-IDF 5.5.5 and its tools into `~/Library/Caches/esphome` (about 2.7 GB on disk, the measurement in `mac-setup/README.md`).

```bash
./esphome.sh compile frl-ble-proxy.yaml
```

What you should see at the end: `RAM: 40.6% (used 183728 bytes from 452112 bytes)` and `Flash: 37.2% (used 1461906 bytes from 3932160 bytes)`, then `Successfully compiled program`.

4. **Bench and terminal.** Plug the board in and flash it. Find the port with `ls /dev/cu.usbmodem*`.

```bash
./esphome.sh upload frl-ble-proxy.yaml --device /dev/cu.usbmodemXXXX
```

What you should see: `Wrote 1527808 bytes ... Hash of data verified.` and `Successfully uploaded program.` The board resets itself. No button is pressed at any point.

5. **Terminal.** Watch it join Wi-Fi: `./esphome.sh logs frl-ble-proxy.yaml --device /dev/cu.usbmodemXXXX`. The line to wait for is `IP Address: 192.168.x.x`. In the one boot saved here, a `Disconnected ssid='...' bssid=... reason='Authentication Failed'` line appeared right before the join and the second attempt joined; seen once, cause not found.

6. **Terminal or browser.** Add the board to Home Assistant by IP address. What worked here, through the REST API (`../../tools/ha.py` wraps the calls): three POSTs to `/api/config/config_entries/flow`, the first with handler `esphome`, the second with the board's IP and port 6053, the third with the API key as `noise_psk`. In the browser the same flow is Settings, Devices and services, Add integration, ESPHome, then the IP, port and key. Home Assistant may also list the board as discovered; that path was not seen on this bench. If you type the `.local` name and get `connection_error`, use the IP: the name did not resolve from Home Assistant here (cause not isolated; suspected: the mDNS name does not cross the Docker network on the host).

7. **Bench.** Move the board to a USB charger near the devices it should hear. ESPHome's wireless update is built into the image (`./esphome.sh run frl-ble-proxy.yaml --device <ip>`); not run on this bench.

## What you get

| Measured | Value | Source |
|---|---|---|
| First compile, toolchain download included | 324 s | the wrapper's timer around `esphome compile` (`compile exit 0 in 324 s`), first run |
| Upload over USB | 6 s for the whole `esphome upload` command, of which esptool's write took 4.0 s (1.46 MB at 3.09 Mbit/s) | upload log |
| Bootloader's first log line to `IP Address:` | 8.8 s | serial log stamps |
| Image size (the app only) | 1.39 MB (37.2% of the 3.75 MB app slot) | build size table |
| Static RAM used | 179 kB of 442 kB | build size table |
| Free heap | 207 kB at boot; 200 kB 20 minutes later with Home Assistant connected | serial boot log; a saved reading of `sensor.frl_bluetooth_proxy_heap_free` at 12:47 |
| Wi-Fi signal on the bench (about 2 m from the router, estimated) | -39 dBm | serial boot log (`Signal strength: -39 dB`); the saved Home Assistant reading 17 minutes later was -40 dBm |
| Bluetooth proxy | active scanning, 3 connection slots, scan window 320 ms | boot log |

The upload writes more than the image: the bootloader and partition table go with it. The debug entities (heap, largest block, reset reason) come from the `debug` component in the YAML. They are optional; remove them for a plain proxy.

## What this does not establish

- Nothing has been heard through the proxy. Next: the BTHome beacon test (the CC2340R5 seen by Home Assistant through this board), not run.
- One board, one router, one afternoon. Range, loss and the proxy's behavior with many devices are not measured here.
- How long a second compile takes was not timed. The charger move and the first wireless update have not happened.

## Files

- `frl-ble-proxy.yaml`: the proxy, built from esphome.io's recommended Wi-Fi proxy configuration with this board's variant and flash size and the debug sensors above.
- `frl-esphome-minimal.yaml`: Wi-Fi, API, OTA and logger only, the yardstick for a minimal ESPHome device ([experiments](../../docs/experiments/)).
- `secrets.example.yaml`: the shape of the secrets file.
- `esphome.sh`: runs esphome from a build folder outside the repository with the real secrets file copied in (`SECRETS`, `BUILD`, `ESPHOME` override the paths).
