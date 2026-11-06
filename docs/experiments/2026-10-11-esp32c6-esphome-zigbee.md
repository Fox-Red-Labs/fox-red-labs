<!-- experiment
id: E38
board: ESP32-C6-DevKitC-1
date: 2026-10-11
status: works
reached: verified
line: ESPHome's Zigbee end device on the ESP32-C6 joined ZHA 5.8 s after boot, and 31 of 31 counter reports reached Home Assistant in order. The image is 492 kB of flash and 83 kB of static RAM. It offers no switch to toggle on this chip.
stop: none
-->
# Does ESPHome's Zigbee end device work on the ESP32-C6 and what does it cost?

**Short answer.** Yes for sensors, no for switches. A YAML file with a `zigbee:` block built a 492 kB image that joined the house Zigbee network through Home Assistant's ZHA 5.8 s after boot and sent a counter every 2 s; all 31 values reached Home Assistant in order. The plan's 20 toggles could not be run, because ESPHome's Zigbee component supports only sensors and binary sensors on the ESP32 (switch and number are nRF52 only, its documentation says).

Terms: [vocab](../vocab.md). 1 kB = 1,024 bytes. ZHA is Home Assistant's Zigbee integration. This is the nearest yardstick for the same plan on a TI chip with a 36 kB RAM budget.

**Question.** Does ESPHome's Zigbee component work as an end device on the ESP32-C6 into ZHA, and how big is it?

**Vendor position.** ESPHome's Zigbee page, read 2026-10-11, lists the ESP32-C5, C6 and H2, says the H2 is more reliable than the C6, warns that some C6 boards work only next to the coordinator, and says Wi-Fi in station mode with an end device works fine.

**What was tried.** `esp32c6/proxy/frl-esphome-zigbee.yaml`: Zigbee end device, one template sensor that counts up every 2 s with `report: force`, the chip temperature, the logger. No Wi-Fi, no API, no OTA, as a CC2340R5 build would be. ESPHome 2026.9.1, esp-zigbee-lib 2.0.4, ESP-IDF 5.5.5. The board was erased, flashed, ZHA opened with `zha.permit` (250 s), and the script `esp32c6/zigbee/report_bench.py` read the counter entity every 20 ms for 60 s while the serial log ran beside it.

**How far it got.** Verified. The device log said `Joined network successfully ... Channel(20)` 5.8 s after boot; the entities were in Home Assistant 6.4 s after boot: counter, temperature, signal strength, link quality and an identify button. The counter reached Home Assistant 31 times out of 31 printed, none missing, none out of order. The latency was not measured to better than about 0.2 s, because the serial log is read in chunks. The gaps between values in Home Assistant matched the 2 s send period. The chip temperature showed 95.7 in Home Assistant while the same chip reads about 31 to 36 C in ESP-IDF; the unit was not checked.

The board was removed from ZHA afterwards with `zha.remove` and its IEEE address; the device list was identical to the list before the test and its entities were gone.

**The line.** No limit reached on the join. The cost: 492 kB of flash image, 82.7 of 441.5 kB of static RAM (18.7%), 385 kB of heap free at boot. The CC2340R5 has 36 kB of RAM in total. This image needs 83 kB of static RAM with ESP-IDF, FreeRTOS and ESPHome's runtime included, so a like-for-like port needs a larger part such as the CC2755. The switch limit is in ESPHome's component, not the chip: its ESP32 code builds the Analog Input and Binary Input clusters only.

**Ask.** Has anyone driven a light or switch from ESPHome's Zigbee component on an ESP32 by declaring the On/Off cluster?
