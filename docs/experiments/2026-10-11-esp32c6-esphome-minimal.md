<!-- experiment
id: E37
board: ESP32-C6-DevKitC-1
date: 2026-10-11
status: line found
reached: boot
line: The smallest useful ESPHome device on the ESP32-C6 is 816 kB of flash and 138 kB of static RAM, with 305 kB of heap free at boot. The Wi-Fi-only image found no network in four boots, and the update over the air stalled at 0.1 percent twice.
stop: unknown
-->
# What does a minimal ESPHome device cost on the ESP32-C6-DevKitC-1?

**Short answer.** The numbers are small: the image with Wi-Fi, the Home Assistant API, over-the-air (OTA) updates and the logger is 816 kB of flash and 138 kB of static RAM, and it boots with 305 kB of heap free. The two things that should have worked did not here. That image never found the home Wi-Fi network in four boots, and the version that did (with an idle Bluetooth scanner added) stalled in an OTA update after 0.1 percent, twice.

Terms: [vocab](../vocab.md). 1 kB = 1,024 bytes. ESPHome is the tool that turns a YAML file into firmware for a Home Assistant sensor. The numbers below are the yardstick for an ESPHome port to a TI chip.

**Question.** How much flash, RAM and heap does the smallest useful ESPHome device take on the chip ESPHome was built for, and how long does a YAML edit take to reach the board?

**Vendor position.** ESPHome 2026.9.1 builds on ESP-IDF 5.5.5 for the ESP32-C6 and lists Wi-Fi, the API, OTA and logger as the base of every device. Source: esphome.io, read 2026-10-11. The estimate written before the run was about 1 MB of flash and over 200 kB of free heap.

**What was tried.** `esp32c6/proxy/frl-esphome-minimal.yaml` has Wi-Fi, API with encryption, OTA, logger and the debug heap sensor. It ran on ESPHome 2026.9.1 and ESP-IDF 5.5.5, on one ESP32-C6-DevKitC-1-N8 with 8 MB flash, erased and flashed over USB with esptool.

1. First compile took 287 s, including the one-off download of ESP-IDF at 2.6 GB on disk. A recompile after a one-line edit took 137 s and 147 s on the two images below, while other builds ran on the Mac.
2. Flash and boot: the write took 6 s. The boot log prints `Free Heap Size: 311944 bytes`.
3. Wi-Fi: the log said `No networks found` after every scan for 100 s, in four boots, two of them after a full erase. The same board and place joined the home network in under 10 s, at -52 dB, with the Bluetooth proxy image.
4. A second image, the same YAML plus an `esp32_ble_tracker:` line, an idle Bluetooth scanner, joined at once. Its file is `esp32c6/proxy/frl-esphome-minimal-tracker.yaml`. Its sizes: 1.34 MB of flash, 178 kB of static RAM, 210 kB of free heap at boot.
5. OTA: from that image `esphome upload` connected, finished the encrypted handshake, started the transfer, and the board logged `Progress: 0.1%` after 2.6 s and nothing after. Three attempts each time, 234 s and 299 s, ended in `Operation timed out` or `Connection reset by peer`. The board kept answering ping, with 0 to 33 percent loss and 5 to 500 ms round trips, but printed nothing until reset. The same happened with the Bluetooth proxy image.

**How far it got.** Compile, link, flash, boot and run for both images. Wi-Fi joined only on the image with the Bluetooth scanner. OTA never completed, so the OTA update time and the "YAML edit to updated device" time were not measured; the edit-to-compiled time of 137 s and the USB write of 6 s were.

**The line.** Sizes come from the `idf.py size` totals. The minimal image is 816 kB of flash, 21 percent of the 3.75 MB app partition, with static RAM at 138 of 441.5 kB and 305 kB of heap free at boot. The two failures are not explained. For the Wi-Fi failure, one configuration difference separates the image that failed from the images that joined: the Wi-Fi and Bluetooth coexistence code, which the minimal image leaves out. Switching it on through the YAML had no effect on the build. For the OTA stall, ESPHome logs `Stopping scan for OTA` and the board then goes quiet.

**Ask.** Has anyone seen an ESPHome Wi-Fi-only image on an ESP32-C6 that finds no networks until a Bluetooth or 802.15.4 component is added? What does your OTA stall look like?
