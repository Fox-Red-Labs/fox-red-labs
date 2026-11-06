<!-- experiment
id: E198
board: LP-EM-CC2340R5
date: 2026-10-17
status: works
reached: verified
line: A CC2340R5 tag fed by a machine MCU's UART advertised its counters on a legacy set and its full record on an extended set at the same time, and a CC35X1 hub with its own access point and the house Wi-Fi up logged 283 of 283 events (median 1.4 s) and 269 of 274 run summaries, with totals identical to the equipment's; the on-tag run-event classifier was right 95.9% of the time on live run events.
stop: none; the equipment was simulated over USB, and the hub's web server timed out on 17 of about 2,400 requests with two clients polling
-->
# Can a CC2340R5 Bluetooth tag carry a machine's telemetry to a CC35X1 hub, and a phone, with no internet?

**Short answer.** Yes. Over 10 minutes the hub logged 283 of 283 events from the equipment, a median 1.4 s after the equipment sent each one, with totals identical to the equipment's. 269 of 274 per-run summaries arrived, and the tag's own model classified the run events 95.9% right. The project page: [tool-telemetry/README.md](../../tool-telemetry/README.md).

Terms: [vocab](../vocab.md), [CC2340R5 FAQ](../../cc2340r5/FAQ.md), [CC35X1 FAQ](../../cc35x1/FAQ.md). Wire formats: [PROTOCOL.md](../../tool-telemetry/PROTOCOL.md).

![The phone app served by the hub: one machine's trend, live values, load mix and recent run events](../../tool-telemetry/img/app-tool.png)

**Question.** Can a machine's motor MCU hand its counters to a cheap Bluetooth tag over a UART, can the tag advertise them for any phone (legacy) and in full for a Bluetooth 5 hub (extended) at once, and can a Wi-Fi hub collect every machine nearby and serve a phone with no internet?

**Vendor position.** TI's `basic_ble` example for the LP-EM-CC2340R5 ships one legacy advertising set; its SysConfig offers extended advertising as an option (`extAdv`, which adds `-DUSE_AE`). TI's network_terminal for the CC35X1 has extended scanning built in: "Set Ble Scan Configuration. Default Parameters: extended 1M PHY, scan interval of 100ms and scan window of 50ms." (`str.c`, SDK 10.20.00.39, read 2026-10-17), and brings up an access point with its own DHCP server. Neither SDK has an example that joins the two.

**What was tried.** LP-EM-CC2340R5 (probe <probe serial>), F3 SDK 9.21.00.36, `basic_ble` patched by `tool-telemetry/tag/apply.py`: a UART task parsing the equipment's lines, the peripheral's legacy set reloaded with the 25-byte frame, a second set created in code as extended, non-connectable, every 1 s, and a 35-node decision tree classifying each run event. LP-EM-CC35X1 (probe <probe serial>), Wi-Fi SDK 10.20.00.39, `network_terminal` patched by `tool-telemetry/gateway/apply.py`: at boot it runs the example's own commands (`wlan_start`, `ble_start`, `wlan_ap_role_up`, `wlan_sta_role_up`, `wlan_connect`), then scans passively with NimBLE's `ble_gap_ext_disc` (100 ms interval, 70 ms window), keeps a 2,048-record log and serves the app and a JSON API on port 80 from two worker tasks. The equipment was `sim/toolsim.py` writing into the tag's USB serial port at 3 times real speed; `sim/e2e.py` timed every event from the equipment's line to the hub's log, both through the hub's UART and through its HTTP API, with the phone app open in a browser.

**How far it got.** Verified, on the bench. The codecs agree byte for byte in C and Python (9,014 checks). Seven 5 and 10 minute runs; the last:

| What | Result |
|---|---|
| Equipment lines | 1,264 sent, 1,264 good, 0 bad |
| Events at the hub | 283 of 283; median 1.4 s, 95% within 5.0 s, max 9.2 s (hub UART) |
| Totals | identical: 275 run events, 9 trips, 661 s run |
| Run summaries | 269 of 274; the hub counted the 5 it missed |
| On-tag classifier | 95.9% right on 269 live run events; 9 of 9 jams |
| Hub web server | 17 of about 2,400 requests timed out with the app and a 4-per-second poller at once; one client alone 60 of 60, mean 0.11 s |

**The line.** Nothing stopped the chain. What it took, with the cause of each wrong turn:

| Problem | Cause tag | Cost | Fix |
|---|---|---|---|
| `ble_start` refused, then the access point refused a 12-character name as "max 32 characters" | Tool or docs | 20 min | Wi-Fi first, quoted SSID |
| Hub records lost behind `Received VLAN packet - drop it` with no line end | Tool or docs | 15 min | parse the marker anywhere in a line |
| Hub froze the Bluetooth side when a client stalled | Our mistake: lock held across `send()` | one run | build under the lock, send after |
| Two 44 kB response buffers took the hub off the network | Our mistake: FreeRTOS heap has 48 kB to spare | one flash | two 24 kB buffers, 100-record pages |
| Tag died 10 to 20 s after boot | Our mistake: `snprintf` on TI's 1 kB BLE thread | 40 min | format on a task of its own |

No TI example joins a CC23xx advertiser with a CC35xx scanner and access point, so the method was worked out here. Effort: about five hours, no hands at the bench (both boards were already plugged in). Verdict: good. TI's examples did the hard parts (the stack, the scanner, the access point, DHCP). The code is about 900 lines of C and the sharp edges came from it or were small.

**Ask.** Anyone who has run a CC35xx access point and a NimBLE scan side by side for days: did the scan duty or the heap hold up?
