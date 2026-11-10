<!-- experiment
id: E193
board: LP-EM-CC35X1
date: 2026-10-13
status: waiting
reached: run
line: ESPHome builds, flashes and runs on the LP-EM-CC35X1 through an external platform `cc35xx`. `esphome compile` takes 56 s cold and 23 s warm, and `esphome upload` takes 60 s with a full erase. The board joins Wi-Fi 5.3 s after a reset and serves ESPHome's native API. A client on the board got 3 entities and 20 of 20 switch commands at a median of 120 ms. Home Assistant adoption is not run: the test network isolates its clients.
stop: network
-->
# Can ESPHome's own commands build and flash the LP-EM-CC35X1 as a Wi-Fi device?

**Short answer.** Yes, up to the last step. An ESPHome platform `cc35xx` (in `ports/esphome-ti/components/cc35xx/`) builds ESPHome's generated code with TI's compiler on top of TI's Wi-Fi SDK, signs it with TI's Toolbox, and flashes it with `esphome upload`. The board boots in 1.9 s, joins Wi-Fi 5.3 s after a reset, writes ESPHome's log to the board's serial port, announces `frl-cc35x1.local` with the `_esphomelib._tcp` service, and serves ESPHome's native API. A native-API client running on the board itself, over the loopback interface, got the hello, the device info, three entities and 20 of 20 switch commands confirmed, with the LED's output latch matching every one. What is not proven: Home Assistant adopting it over Wi-Fi. The test network isolates its clients from the home LAN, so the Home Assistant leg needs a network that does not.

Terms: [vocab](../vocab.md). The native API is the protocol every ESPHome device speaks with Home Assistant (TCP port 6053). mDNS is how Home Assistant finds ESPHome devices without an address. Loopback is the network interface inside a device that talks to itself (127.0.0.1). The Toolbox is TI's SimpleLink Wi-Fi Toolbox, which signs images and programs this chip. Related: the [ESPHome CC27xx platform page](2026-10-13-esphome-cc27xx-platform.md) (the same platform idea for TI's Zigbee chips). Code and maker path: [ports/esphome-ti/README.md](../../ports/esphome-ti/README.md).

Contents: question, vendor position, expectation, what was tried, how far it got, the line, ideas, ask, build and soak findings.

**Question.** ESPHome's own documentation lists the chips it builds for, and no TI chip is among them. Most of the 151,326 Home Assistant installations that use ESPHome run Wi-Fi devices (Home Assistant's public analytics, read 2026-10-13). The CC3551E is TI's Wi-Fi 6 and Bluetooth LE microcontroller. Can it be one of those devices, built and flashed with ESPHome's own commands?

**Vendor position.** TI ships no ESPHome support for any chip. TI's Wi-Fi SDK 10.20.00.39 ships FreeRTOS, lwIP with sockets, select, a loopback interface and the mDNS responder, set by `LWIP_SOCKET`, `LWIP_HAVE_LOOPIF 1` and `LWIP_MDNS_RESPONDER LWIP_UDP` in `lwipopts.h`, and an `mqtt_client` example that joins a network from a compiled-in name and password. ESPHome's documentation for adding a platform is its source: external components may be target platforms with their own compile and upload steps (`IS_TARGET_PLATFORM`, `run_compile`, `upload_program`, used by the CC27xx platform).

**Expectation.** RAM is the risk: the Matter thermostat image on this chip used 579 kB static of about 669 kB. ESPHome's core plus Wi-Fi, sockets and the API on TI's Wi-Fi stack was expected to fit with room, because TI's mqtt_client example is smaller than the Matter image.

**What was tried.**

- A platform `cc35xx` modeled on the `cc27xx` platform: ESPHome's code generation as usual (the platform answers to ESPHome's Zephyr code paths for pins and preferences), then `ti35_build.py` copies TI's `mqtt_client` example as the board support, runs SysConfig, compiles TI's C and ESPHome's C++ with TI Arm Clang 5.1.1, links with TI's libraries and linker file, and signs with the Toolbox's makefile. `esphome upload` erases the whole flash and programs the signed bundle.
- A Wi-Fi backend for ESPHome (`shim/wifi_component_cc35xx.cpp`, 261 lines, the same functions as ESPHome's Pico W backend) on TI's driver: start, station role, connect, disconnect, scan, RSSI, addresses from lwIP's interface. TI's `wifi_if.c` is reused unchanged except for two lines added to its event handler (the channel at connect and the end of a scan) and a block of entry points appended to it.
- ESPHome's logger, mDNS and the socket wake hooks for this platform (`shim/logger_cc35xx.cpp`, `shim/mdns_cc35xx.cpp`, `shim/esphome/core/wake/wake_zephyr.h`), and ESPHome's compiler flags passed through to TI's compiler.
- Four ESPHome files take their lwIP branch instead of their Zephyr one in the build copy (the socket and IP-address headers, `socket.cpp`, the logger). ESPHome's installed files are not edited.
- An on-board API client (`shim/api_selftest.cpp`, built with `api_self_test: true`) because no client on the house network can reach the board on the guest network.

**How far it got.** Run, on an isolated test network (guest network). It took fourteen build rounds in all, the first five of them failures listed in the findings below.

| Step | Result |
|---|---|
| Code generation | Yes, for `wifi`, `api`, `socket`, `mdns`, `logger`, a GPIO switch and two sensors; three keys (`power_save_mode`, `max_connections`, `max_send_queue`) need values in the YAML, because ESPHome's defaults are per platform |
| Compile and link | Yes: 64 files, 1,274 kB of flash used of 2,687 kB, 74 kB of main RAM and 86.7 kB of TCM free (rows below) |
| Sign and flash | Yes: `esphome upload` erased and programmed in 60 s |
| Boot and ESPHome setup | Yes: 1.9 s from a reset to ESPHome's setup; ESPHome logs "setup() finished successfully" |
| Join Wi-Fi | Yes: 5.3 and 5.7 s from a reset to an address on two clean joins; on a third the access point dropped the first association, ESPHome scanned (3 networks) and joined another access point, address at 16.9 s |
| mDNS | Announces `frl-cc35x1.local` with 1 service (the board's log); not seen from another device (isolated network) |
| Native API, on the board | Yes over loopback, every 5 minutes: hello in 30 ms, device info (103 bytes), 3 entities, 20 of 20 switch commands confirmed, LED latch matched 20 of 20, median 120 ms (114 to 120) |
| Native API from Home Assistant | Not run: port 6053 unreachable from the Mac and the Home Assistant host on the house network |

| Measure | Value | Row |
|---|---|---|
| Flash used | 1,274 kB | `radio_flash_used wifi-esphome-api` |
| Main RAM free after link, TCM free | 74.0 kB, 86.7 kB | `radio_ram_left wifi-esphome-api` |
| Free heap, running, minimum seen | 139.7 kB | the board's alive lines |
| `esphome compile`, cold and warm | 55.7 s, 23.3 s | `blink_build esphome-cc35xx-compile-cold`, `-warm` |
| Reset to address | 5.3 s median of two clean joins | `radio_join wifi-esphome-guest` |
| Switch command to confirmed state, on the board | 120 ms median | `radio_cmd_latency esphome-api-loopback` |

**The line.** The network, not the chip. Everything up to a Home Assistant client works on the board; the test network isolates its clients, so port 6053 is unreachable from other devices. The next run is Home Assistant's ESPHome integration on the home network: add the device (or let mDNS offer it), switch the LED 20 times, read both sensors, leave it a day.

What remains to make it a stranger's ten-minute path, each a known piece of work: encryption for the API (needs a real random source, TI's TRNG driver), preferences that survive a reset (TI's NV storage), Wi-Fi setup without the password in the YAML (Improv over serial or a setup access point), platform defaults for the three keys, and over-the-air updates.

**Ask.** If you have built an out-of-tree ESPHome platform: is there a supported way to give it platform defaults for `wifi: power_save_mode`, `api: max_connections` and `max_send_queue` (ESPHome's `SplitDefault` lists only its own platforms), and to declare its UARTs to the logger without patching `uart_selection`?

## Build and soak findings (what to expect if you rebuild)

- **Five build rounds, each failure naming one thing.** ESPHome's logger has no UART list for an unknown platform (logger left out first). TI's `lwipopts.h` sets `LWIP_COMPAT_SOCKETS 1`, which turns `connect()` and `close()` into macros and breaks ESPHome's socket class (a one-line override for ESPHome's files). Renaming every Zephyr branch in the socket files also hid a helper ESPHome needs (narrowed to the files that pull Zephyr's headers). TI's `osi_kernel.h` declares `Report()` without C linkage guards, so C++ files cannot call it (a C wrapper). The station's channel query is answered only for the access-point role (`AP role is not up!`; the channel now comes from the connect event).
- **First image on the board.** Boot, ESPHome setup, join, an address from the access point, then silence after the channel query. Not explained; the next image without that call ran.
- **Logger.** The loop ran but the Wi-Fi component's loop printed nothing, so the logger was made to work: the logger's Zephyr branch takes its generic path in the build copy, a 20-line backend writes to TI's console, and ESPHome's compiler flags (log level among them) reach TI's compiler. ESPHome's own log then showed a normal ESP32-like start: fast connect, connected, API waiting for a client.
- **Isolated network.** Port 6053 was unreachable from the Mac and the Home Assistant host: the test network isolates its clients. If you see this, test from a network that lets clients reach each other.
- **On-board API client.** mDNS runs on lwIP's responder. The first run gave 20 of 20, median 296 ms, LED matched 10 of 20. Both were artifacts of the test client: it wrote each frame header and body in two writes (Nagle's algorithm with a delayed ACK) and read the pin's pad value, which reads 0 while it drives. With one write per frame and the output latch read back: 120 ms, 20 of 20.
- **Reset without a reflash.** This showed a real gap: when the access point dropped the first association, ESPHome fell back to scanning, and the backend had no scan, so the board stayed without an address. Scanning was added (TI's `Wlan_Scan`, results from the scan event); on the next three resets it joined in 5.7 s and 5.3 s, and on the third it scanned and joined another access point.
- **Soak, first rounds.** On-board API test every 5 minutes, console captured. Rounds 1 to 4: each 20 of 20 confirmed, LED latch 20 of 20, median 120 ms (minimum 111 to 114, maximum 121 to 122), free heap 140.8 kB every round, minimum 136.0 kB.
- **Random source.** The placeholder random source (a linear congruential generator) is weak. It now reports failure, so anything that needs real randomness stops instead of using it, and the platform refuses `api: encryption` at config time with a plain message until TI's TRNG driver is wired in. The same change went into the Zigbee platforms' shim. The image still compiles; the soak image is plain-text API, unaffected.
- **24-hour soak.** 281 rounds, 5,616 of 5,620 switch commands confirmed (99.9%), all four misses in rounds 19 and 20 on the first day; every round's median was 120 ms. The access point dropped the board 22 times and it reconnected on its own each time; the board never restarted. Free heap stayed at 140.7 kB with a lowest point of 135.7 kB.
