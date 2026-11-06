<!-- experiment
id: E43
board: ESP32-C6-DevKitC-1
date: 2026-10-11
status: line found
reached: boot
line: One ESPHome image can hold the Bluetooth proxy and a Zigbee end device on the ESP32-C6. The end device never joined with the proxy scanning all the time or with Wi-Fi alone, while the Zigbee-only image joined in 6 s. Merely carrying the idle Zigbee stack cost the proxy 10 percent of its advertisements: 727 against 804 per 10 s.
stop: unknown
-->
# Can one ESP32-C6 be a Bluetooth proxy and a Zigbee end device at once?

**Short answer.** Not in ESPHome 2026.9.1 on this board. The combined image builds and boots, and the proxy half works, but the Zigbee end device never joined ZHA. Removing Bluetooth and keeping Wi-Fi did not help either. A version with the Bluetooth scan cut to a tenth of the time was tried only with an invalid permit, so its join is untested. The same Zigbee component with no Wi-Fi and no Bluetooth joined in 6 s. While the Zigbee half is only searching, the proxy hears about 10 percent fewer advertisements.

Terms: [vocab](../vocab.md). 1 kB = 1,024 bytes. The ESP32-C6 has one 2.4 GHz radio front end that Wi-Fi, Bluetooth and 802.15.4 (Zigbee, Thread) share by taking turns.

**Question.** Does a Bluetooth proxy board also work as a Zigbee end device, and what does each cost the other?

**Vendor position.** ESP-IDF's coexistence table marks Bluetooth scanning with an 802.15.4 end device as supported with conditions, and with a router as not supported. ESPHome's Zigbee page says Wi-Fi in station mode with an end device works fine.

**What was tried.** ESPHome 2026.9.1, ESP-IDF 5.5.5, one board. All images use the house proxy's name and keys. The advertisement counter is `on_ble_advertise`, printed every 10 s with a phase label that alternates every 60 s. Zigbee "fast" reports a counter each second, and "idle" reports nothing.

| Image | Flash | Static RAM | Result |
|---|---|---|---|
| Proxy plus counter, no Zigbee (baseline) | 1.40 MB | 179 kB | 804 advertisements per 10 s over 16 windows, standard deviation 17 |
| Proxy plus Zigbee end device, scanning all the time | 1.59 MB | 194 kB | never joined; 727 per 10 s over 18 windows, sd 38, idle 726 and fast 727 |
| The same with a 30 ms scan window in 320 ms | 1.59 MB | 194 kB | join not tested, permit call refused, see below; 120 per 10 s; board reset about every 55 s |
| Wi-Fi plus Zigbee end device, no Bluetooth | 1.01 MB | 154 kB | never joined; board reset about every 12 s |
| Zigbee end device alone, the first Zigbee experiment | 492 kB | 83 kB | joined 5.8 s after boot |

Joining: ZHA was opened with `zha.permit` (250 s) at the start of each try. The combined image was flashed, without erasing, onto a board that had joined with the Zigbee-only image. It kept its network data and printed `The BDB Device Reboot failed with status(0x03), please retry` every 1.6 s for 60 s. The 30 ms window try and the first Wi-Fi-only try used a 300 s permit. Home Assistant rejects that, because the limit is 254 s, so they did not count. The Wi-Fi-only image was repeated with 250 s. The 30 ms image was not repeated, and the board is no longer reachable to repeat it. The device was removed from ZHA afterwards by IEEE address; the device list matched the list from before, and its entities were gone.

**How far it got.** Boot, Wi-Fi, API connection to Home Assistant and the proxy ran in the first image. Zigbee reached the network search and stopped there. No traffic could be compared with the Zigbee side joined. So "with and without Zigbee traffic" became "without the Zigbee stack, and with it searching". That gave 804 against 727 advertisements per 10 s, a drop of 9.6 percent. The fast and idle phases did not differ because nothing was joined to report.

**The line.** The end device's search for a network returns status 0x03, no network found, at every try with the proxy scanning all the time. The Bluetooth scan was ESPHome's default for a proxy: the window is raised to the interval, so the receiver is on all the time. With a 30 ms window, 9 percent, the join was not tested. In the full-scan case the only evidence of what the 802.15.4 receiver heard is the status code. The two images that reset, the 30 ms window and the Wi-Fi-only one, restart through a software reset a moment after a Wi-Fi disconnect. There is no panic text in the log, and the cause was not isolated. The Zigbee-only image had neither problem.

**Ask.** Has anyone run ESPHome's Zigbee end device together with Wi-Fi on an ESP32-C6 that joined? What did you change?
