# Tool telemetry design notes: Bluetooth tag, Wi-Fi hub and phone app

Why the system is shaped the way it is, and what it cannot do. Terms: [docs/vocab.md](../docs/vocab.md). The wire formats are in [PROTOCOL.md](PROTOCOL.md); how to build and run it is in [README.md](README.md).

## The use case

A crew's motor-driven equipment reports how they are used: trigger pulls, motor run time, overcurrent trips, driver faults, battery state. Nothing on the tool needs Wi-Fi or a phone pairing. A hub in the work area (a site box, a van, a storeroom) hears every tool nearby, keeps the history, and hands it to whoever shows up with a phone, internet or not. When the hub has a way out (site Wi-Fi, a cellular modem), it sends the same records home on its own.

## The chain, and why each link is where it is

| Link | Board | Why this part |
|---|---|---|
| Tool MCU | LP-AM13E230 driving the DRV8376EVM and the BLY171S motor (stand-in now: `sim/toolsim.py` on the Mac) | The motor-control MCU already knows the truth: it counts trigger pulls, run time and trips because it runs the motor. Totals live here, so a tag can be swapped without losing history. MSPM0 or MSPM33 fit the same role on a smaller tool. |
| Tag | LP-EM-CC2340R5 (CC2755P10 builds from the same files) | A Bluetooth LE radio with a UART. It does not need to know anything about motors: it parses lines, keeps a small event log in flash, and advertises. CC23xx is the low-cost choice; CC27xx adds 20 dBm and Hubble's satellite path. |
| Hub | LP-EM-CC35X1 | Wi-Fi 6 and Bluetooth LE 5.4 in one chip with an M33 for the application: it scans Bluetooth, runs an access point for phones and a station for the site network at the same time, and serves the app. |
| Phone | Any phone with a browser | The hub serves the app from its own flash, so nothing is installed and nothing needs the internet. The phone keeps everything it pulled and uploads later. |
| Home base | `homebase/server.py` on any computer | One endpoint for every path, one row per record. |

## Decisions

1. **UART between tool MCU and tag, ASCII lines with an XOR checksum.** Readable on a terminal, writable from any MCU in a few lines of C, and a dropped byte costs one line, which is discarded and counted. Binary would save air time that does not matter at one line a second.
2. **Two advertising sets on the tag, one legacy and one extended.** Legacy (31 bytes) is what every phone and old scanner can hear: the counters fit in 25 bytes. Extended (up to 254 bytes in one packet) carries the full record: live values, battery, the tool's name, the last 8 events and the tag's own health. Bluetooth 5 scanners such as the hub get everything; anything else still gets the counters. On the bench the Mac (Intel, macOS 15) hands only the legacy set to apps; the CC35X1 hears both.
3. **Events carry the tag's uptime, not a clock.** The tag has no clock to set. The hub turns "tag uptime when it happened" into its own time, and the phone or modem turns that into wall-clock time. Events from before the hub started still get the right time; events from before the tag's last restart are marked "before restart".
4. **The phone is a data mule.** `GET /api/log?since=N` hands over everything since the phone's last visit, in pages. The phone stores it (IndexedDB), shows it offline, and uploads to the home base when it can. Every hub answers on the same address (10.0.0.3), so the stored data belongs to one web origin and follows the phone from hub to hub.
5. **The hub serves the app over plain HTTP from its access point.** A phone on the hub's Wi-Fi has no internet, so the page cannot come from a website (a page loaded over HTTPS may not call an HTTP hub). The cost: no service worker, so the page reloads only while the phone is on the hub's network; the saved data stays on the phone and the Saved tab can export it as CSV or JSON.
6. **The home base deduplicates.** A record can arrive by phone, by modem and by Wi-Fi uplink; it is stored once, keyed by hub, tag, kind and the record's own identity (snapshot counters, or event number).
7. **One random Wi-Fi password per hub, never a shared default.** `gateway/build.sh` makes it on the first build and keeps it in `<scratch>/secrets/`; the firmware build fails without one, and the hub's log never echoes it.

## Backhaul: how records get home

| Path | Status | Notes |
|---|---|---|
| Phone carried in | Works | The app uploads when the phone can reach the home base. |
| Hub on site Wi-Fi (a local router, a phone hotspot, a crew's own router) | Works: the hub runs its access point and joins a local Wi-Fi network at the same time | The hub's API is reachable from the network; the home base can pull, or a later version pushes. |
| Hub with a cellular modem | Bench stand-in works: `homebase/modem.py` reads the hub's `@L` lines and posts them | Most cellular modules (LTE-M and NB-IoT parts) attach by UART with AT commands. The hub already prints every record as one line on its UART; a real modem driver replaces the Mac script with AT+HTTP or MQTT. |
| Hubble Network (Bluetooth to satellite, and to phones running Hubble's SDK) | Not built | See below. |

## Hubble

The tag can carry a third advertising set in Hubble's format, so a tool is found and its last state reported where there is no hub and no Wi-Fi. What is known:

- The satellite path needs 20 dBm: the **LP-EM-CC2755P10**, not the CC2340R5 (8 dBm). This tag firmware builds for the CC2755P10 (`tag/build.sh cc2755p10`).
- Hubble's packet is encrypted with a per-device key from their dashboard. The terrestrial path (phones running Hubble's SDK) and the satellite path both need Hubble's Live plan.
- To add it: put Hubble's beacon encoder (Apache-2.0, `samples/freertos/ti/`) in as set 3 on the CC2755P10, carrying the tool's id and counters in its payload, interleaved with sets 1 and 2. Check it on the bench with `pyhubblenetwork ble scan --key`, then outdoors on a pass.

## Stage two: control over Bluetooth

**Built and run on the bench (2026-10-17) with the simulated tool; PROTOCOL.md section 7.** The home base signs `lock`, `unlock`, `stop` or `run` for one tag (HMAC with the fleet key, bound to the tag's address, with a counter that only grows); the hub delivers it over a Bluetooth connection; the tag checks it and passes a `$TC` line to the tool MCU. Lock reached the tool in 3.7 s and the hub saw the tool report locked; a replayed command was refused. The simulated tool and the AM13E230 motor firmware both act on lock and unlock; the home base needs the fleet key to sign them, and an unsigned write is refused. Locking a tool that left the site is the obvious use.

## One framework, many apps

Tool telemetry is app 1 of a general field-data framework (`portability/fielddata/`). A device that is not a tool sends `$FD` lines (an app id and numbered integer fields); the tag advertises them in a generic frame (`0x21`), the hub logs them without knowing what they mean, and the phone app, the home base and the exporter decode them from the app's JSON schema. Adding an app is a schema file. Every record from every app is stored in one envelope with two clocks and its path home, and exported in the layouts TI's Tiny ML Tensorlab and Edge AI Studio, Edge Impulse and this repository's `tools/dataset/` read. Room presence (app 2) runs through the same tag and hub on the bench with a simulated radar; the IWRL6432 radar is the real source to wire next.

## Limits today

- No encryption on the air in version 1: anyone nearby can read the counters (not the control path, which does not exist).
- The hub keeps its log in RAM (2,048 records); a power cut loses what no phone or uplink has collected. Its flash file system could hold it. Generic records keep their fields in a smaller ring (256): a phone or uplink must collect within about 4 minutes at 1 sample a second, or the hub marks the older ones lost.
- Advertising carries the latest sample, so the hub logs about one sample a second per device. Faster time series need the tag to batch samples into each frame.
- The hub's `POST /api/time` is open to anyone on its access point; the worst case is wrong times until the next phone sets it again.
- One HTTP request at a time on the hub (lwIP has 5 TCP sockets); fine for a crew, not for a crowd.
- RAM on the CC2340R5 is nearly full (35.4 of 36 kB with TI's basic_ble), so the tag has no room for more features without trimming TI's example.
