<!-- experiment
id: E39
board: ESP32-C6-DevKitC-1
date: 2026-10-11
status: line found
reached: compile
line: Zigbee and Thread in one ESP32-C6 image compile, but the link stops with 7 multiple definitions of the 802.15.4 driver callbacks. OpenThread and the Zigbee library each define the same seven esp_ieee802154_* functions.
stop: no-software
-->
# Can one ESP32-C6-DevKitC-1 run Zigbee and Thread at the same time?

**Short answer.** Not from a stock build, and not by merging two stock examples. No ESP-IDF or ESP Zigbee SDK example enables both. When Espressif's Zigbee router light with OpenThread in one image, every source file compiled and the link stopped: both stacks define the same seven radio-driver callbacks, so the linker refuses. Espressif's own answer for both at once is two chips.

Terms: [vocab](../vocab.md). The ESP32-C6 has one 802.15.4 radio. The same silicon is the radio for Zigbee and Thread.

**Question.** The C6 is sold as "Zigbee and Thread". Can one chip hold a Zigbee device and a Thread device together?

**Vendor position.** ESP-IDF's coexistence page treats the 802.15.4 radio as one path shared by time division. The multi-PAN option in `components/ieee802154/Kconfig` says "participate in multiple PANs simultaneously by filtering frames", read 2026-10-11. It is the radio's frame filter. It is not a second stack. Espressif's `zigbee_gateway` example runs Zigbee on a Wi-Fi chip with the 802.15.4 radio on a second chip running the OpenThread RCP firmware. A discussion in `esp-matter`, number 1790 of 2026-07-03, recommends two chips for Zigbee plus Thread.

**What was tried.** ESP-IDF v6.1, esp-zigbee-lib 2.0.4 at SDK commit `7b9a2e0` of 2026-09-28, GCC 15.2.0, newlib, target esp32c6.

1. Search: no file under ESP-IDF's `examples/` or the Zigbee SDK's `examples/` sets both `CONFIG_ZB_ENABLED` and `CONFIG_OPENTHREAD_ENABLED`. The only Zigbee-with-Thread arrangement is the gateway above.
2. Merge, part one: the router light with `CONFIG_OPENTHREAD_ENABLED=y` and `CONFIG_OPENTHREAD_FTD=y` added. Configure and build both pass; nothing calls OpenThread, so the linker leaves it out (518 kB).
3. Merge, part two: `app_main` also starts OpenThread the way ESP-IDF's `ot_cli` does (event loop, netif, eventfd, `esp_openthread_start`). The first attempt failed to compile because the radio-config macros come from `esp_ot_config.h`, which lives in the `ot_cli` example, not in a component. That was a mistake in this repo's merge; copying the header fixed it.
4. The change is `esp32c6/zigbee/zb-thread-merge/main.patch`. The link fails with multiple definitions of `esp_ieee802154_transmit_done`, `esp_ieee802154_receive_done` and `esp_ieee802154_transmit_failed`: both OpenThread's radio port and the Zigbee library define them.

**How far it got.** Compiled; did not link, so it was never flashed.

**The line.** The first error, and the only kind of error the build gave: 7 of `multiple definition of 'esp_ieee802154_...'`. `esp_openthread_radio.c` in ESP-IDF's OpenThread port and `esp_zigbee_plat_radio.c` inside the prebuilt Zigbee library `libesp-zigbee-idf.native.release.a` each define `transmit_done`, `receive_done`, `transmit_failed`, `receive_sfd_done`, `transmit_sfd_done`, `energy_detect_done` and `cca_done`. The 802.15.4 driver calls exactly one set of these when the radio finishes something, so two stacks cannot both own them. The Zigbee side is a binary library, so a reader cannot rename its copy. A size for the combined image cannot be given; the image before the OpenThread start code was added was 518 kB and the full OpenThread device alone is 1.18 MB, so the sum would be about 1.7 MB of an 8 MB flash, which is not the limit here.

**Ask.** Does anyone have Zigbee and OpenThread running in one image on a C6 or H2? A working `sdkconfig` would settle it.
