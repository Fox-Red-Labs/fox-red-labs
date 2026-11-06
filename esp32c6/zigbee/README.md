# Espressif ESP32-C6-DevKitC-1-N8 as a Zigbee light in Home Assistant

Yes. With a four-line change to Espressif's own light example, the board joined the house ZHA network as a Zigbee router 32.5 s after flashing, and 20 of 20 on and off commands from Home Assistant landed, with the state settling in 98.5 ms (median). Measured 2026-10-11 on macOS Intel 15.7 with ESP-IDF v6.1 and esp-zigbee-lib 2.0.4.

Terms: [docs/vocab.md](../../docs/vocab.md). Problems with fixes: [FAQ](../FAQ.md). 1 kB = 1,024 bytes. The same test on the TI LP-EM-CC2340R5: [cc2340r5/zigbee](../../cc2340r5/zigbee/README.md).

## What you need

- ESP32-C6-DevKitC-1-N8 on the port marked USB
- ESP-IDF v6.1 with `CONFIG_LIBC_NEWLIB=y` (see the FAQ: the default C library faults on this Mac's toolchain)
- A clone of `esp-zigbee-sdk` (github.com/espressif/esp-zigbee-sdk, 210 MB, commit `7b9a2e0` of 2026-09-28)
- Home Assistant with the ZHA integration and a Zigbee coordinator (here a Home Assistant Connect ZBT-2)

## Steps

1. **Terminal.** Copy `examples/home_automation_devices/on_off_light` to a new folder and apply [router-light.patch](router-light.patch). The stock example is a coordinator that forms its own network; the patch makes it a router, makes it look for a network on every channel (11 to 26), and stops it opening the network for others.
2. **Terminal.** Build for the C6: `idf.py set-target esp32c6`, set newlib, `idf.py build`. The first build downloads `esp-zigbee-lib` from Espressif's component registry.
3. **Terminal.** Erase the chip first (`esptool --port <port> erase-flash`), so the light starts factory-new and looks for a network, then `idf.py -p <port> flash`.
4. **Home Assistant.** Settings, Devices and services, Zigbee, Add device (or call the service `zha.permit`). Within about half a minute the light appears as `light.espressif_esp32c6`. On this bench the first search failed with `Failed to steering network with status(0x03)` and the automatic retry joined; nothing needed doing.
5. **Home Assistant.** Switch the light. The serial log prints `ON_OFF_LIGHT: Set On/Off: 1` and `0`.

## What you get

| Measured | Value | How checked |
|---|---|---|
| Flash to the light listed in Home Assistant | 32.5 s (one run) | host time stamps on the ZHA join and the serial log |
| 20 toggles from Home Assistant | 20 of 20 landed | Home Assistant state and the board's serial, both |
| Command to state settled | 98.5 ms median, 74 to 167 ms | 20 timed toggles |
| Signal as ZHA reports it | LQI 136, RSSI -66 dBm | ZHA device list |
| Image size | 513 kB of flash, 81 kB of static RAM used of 442 kB | build size table |

The TI LP-EM-CC2340R5 router on the same coordinator measured 95 ms median and 37 s to join (2026-10-05); the two numbers come from different days and positions, so read them as the same order of size, not a ranking.

## What this does not establish

- One join. Join times vary with the channel scan and the coordinator's state; three joins would give a median.
- The board was removed from ZHA after the test. It does not run as a light in the house.
- ESPHome's Zigbee page reports some ESP32-C6 boards working only next to the coordinator; this board was across the house from it and worked, which says nothing about other boards.
