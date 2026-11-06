<!-- experiment
id: E41
board: ESP32-C6-DevKitC-1
date: 2026-10-11
status: line found
reached: run
line: An ESPHome device on the ESP32-C6 joined the house Thread network within 1.2 s of reset and answered 20 of 20 pings from the MacBook. The Mac that runs Home Assistant has no route to the Thread network, and adding one needs an administrator password.
stop: tools
-->
# Can an ESPHome device on Thread reach Home Assistant through the Apple TV?

**Short answer.** The Thread half works; the Home Assistant half needs one command that was not run. An ESPHome image for the ESP32-C6 with the `openthread:` component attached to the house Thread network 1.2 s after reset and the MacBook pinged it 20 times out of 20. The Mac that runs Home Assistant finds its name through the border router but sends nothing to it, because macOS does not install the route to the Thread network by itself. Adding the route needs `sudo` (administrator rights) on that Mac.

Terms: [vocab](../vocab.md). 1 kB = 1,024 bytes. Thread is the low-power mesh network used by Matter devices; a border router (here an Apple TV) joins it to the home network.

**Question.** Does ESPHome's Thread support give an ESP32-C6 a path to Home Assistant on a Mac in Docker?

**Vendor position.** ESPHome's `openthread` component takes a dataset, which is a Thread network's keys and channel, and attaches the device as a full Thread device. It cannot run with Wi-Fi on the same device, and its source lists a conflict with `wifi`.

**What was tried.** `esp32c6/proxy/frl-esphome-thread.yaml`: `openthread:` with the dataset read from Home Assistant and kept in the git-ignored secrets file, `network: enable_ipv6`, API with encryption, OTA, logger, a heap and a temperature sensor. ESPHome 2026.9.1, ESP-IDF 5.5.5, one ESP32-C6-DevKitC-1-N8, flashed over USB after a full erase. The house network is on channel 25 with an Apple border router as leader.

**How far it got.** Ran. From the reset to `SRP client has started`, which needs an attached device, took 1.2 s, the same on the first boot after the erase and on a later reset. The device registers itself with the border router, which publishes its name `frl-esphome-thread.local` on the home network. From the MacBook `ping6 frl-esphome-thread.local` returned 20 of 20, round trip 21.7 ms at best, 35.2 ms on average, 126.5 ms worst, through a route that was already on the Mac. It was `fd00:db8::/64` via the Apple TV's link-local address on Wi-Fi, added by hand earlier. From the Home Assistant Mac the name resolved to the right Thread address but 3 of 3 pings were lost; that Mac has no route to that prefix. Adoption in the Home Assistant ESPHome integration was not attempted for that reason.

**The line.** Tools, not the chip: `sudo route add -inet6 fd00:db8::/64 <Apple TV link-local address>%en0` (`fd00:db8::/64` is an example prefix; use the Thread prefix your network advertises) on the Home Assistant Mac, which needs a password, and which the Docker container would need as well (not tested). The numbers for the yardstick: 748 kB of flash image, 106.5 kB of static RAM, 368 kB of heap free at boot, against 305 kB for the Wi-Fi image of the first experiment. Only one device was joined; Home Assistant never saw this one.

**Ask.** What is the cleanest way to give a Dockerized Home Assistant a route to a Thread network that an Apple border router advertises?
