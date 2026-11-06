<!-- experiment
id: E277
board: TMDS62LEVM (AM62L evaluation module)
date: 2026-10-24
status: works
reached: verified
line: Mosquitto, Node-RED, Pi-hole and Home Assistant Container ran on the AM62L from TI's own SDK Linux image (Docker 29.3.0 is already in it) and each did one real thing: a message crossed the broker, a Node-RED flow answered an HTTP call, Pi-hole answered 0.0.0.0 for three ad hosts, and an MQTT sensor reading 21.5 appeared in Home Assistant 2026.10.4. Home Assistant needs 557 MiB and 67.7 s to answer its first API call. gpiozero and Blinka toggle the user LED after three workarounds; the button press was not recorded.
stop: Nobody pressed SW5 and the LED was checked by register, not by eye.
-->
# Can the AM62L run the Raspberry Pi software people run most, and what does each cost?

**Short answer.** Yes, with no new operating system. TI's SDK 12.01 image (Arago 2026.04, kernel 6.18.38) already runs Docker 29.3.0 with overlayfs and cgroup v2; the four containers pulled from Docker Hub and ghcr.io and ran as published for arm64. Terms: [vocab](../vocab.md). FAQ: [AM62L](../../am62l-evm/FAQ.md). Commands, versions and every number: [am62l-evm/linux-apps/README.md](../../am62l-evm/linux-apps/README.md), rows in `data/results/am62l-evm.csv` (tests `linux_app_*`).

**Expectation (before the runs).** None was written down before these runs. The board's measured idle (561 mW, 4,256 CoreMark per core) was known, and the plan assumed Debian or Armbian would be needed for apt and containers. That assumption was wrong: the image's package manifest lists `docker-moby` and `containerd`, and the running system had them.

| App | Real action | Result | Costs |
|---|---|---|---|
| Mosquitto 2 | publish and subscribe, in the container and from a Mac | received both ways | 2.6 MiB, 2.4 s to the port |
| Node-RED 5.0.9 | deploy an http-in flow over the admin API, call it | JSON back 2.5 s after the deploy | 110 MiB, 17.7 s to the editor |
| Pi-hole 2026.10.0 | `dig` four names | `doubleclick.net`, `googleadservices.com`, `ads.google.com` answered 0.0.0.0; `example.com` resolved | 24.8 MiB, 4.6 s to the first answer |
| Home Assistant 2026.10.4 | onboarding, MQTT integration, one discovery sensor | `sensor.frl_bench_frl_test_sensor` = 21.5 | 557 MiB, 67.7 s to the onboarding API |

Idle power across all of them stayed within 5 mW of 567.6 mW (8 INA228 rails, 30 s means). CPU idle fell from 93.0% to 88.9% at worst (Node-RED), apart from Home Assistant's first minute after the MQTT integration (82.5%, not steady).

**Python GPIO.** `gpioinfo` showed LD8 (GPIO0_123) and SW5 (GPIO0_90) claimed by the kernel's heartbeat LED and `gpio-keys` drivers; unbinding them frees the lines. gpiozero 2.0.1 on the lgpio factory and Adafruit Blinka 9.2.0 (with a new `AM62L_EVM` board file, starting from the BeaglePlay one) both toggled GPIO0 bank 3 OUT_DATA bit 27, read from `/dev/mem`. Reading the LED pin back gives 0 whatever was written, so a library's `toggle()` does nothing; the demos keep their own state.

**Three workarounds.** (1) `lgpio` has no wheel for Python 3.14 and needs swig below 4.4 (`PyInt_*` errors with 4.5.1), `--no-build-isolation`, and the C library built from source. (2) gpiozero 2.0 raises `PinUnknownPi` on a board that is not a Raspberry Pi until it is given a board description. (3) Blinka does not know the board until its import tables and Adafruit-PlatformDetect get an AM62L entry. All three are in the README.

**What did not work.** `opkg` cannot fetch: its feed host does not resolve outside TI's network, and there is no apt. A first Home Assistant MQTT call returned 400 until the form's `other_settings` keys were sent. The Pi-hole blocklist loads minutes after the first DNS answer.

**Not run.** SW5 press and release: it needs a person at the board and nobody pressed it. LED seen by a person or camera: not checked, the camera link was down. Three runs per app: Home Assistant's pull alone took 655 s. Zigbee2MQTT and Frigate: they need a radio and a detector.

Rows: `data/results/am62l-evm.csv`, tests `linux_app_ram_mb`, `linux_app_idle_pct`, `linux_app_ready_s`, `linux_app_pull_s`, `linux_app_image_mb`, `rail_power_mw` variants `app-*`.
