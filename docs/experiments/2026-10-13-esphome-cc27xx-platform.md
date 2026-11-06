<!-- experiment
id: E192
board: LP-EM-CC2755P10
date: 2026-10-13
status: works
reached: verified
line: Add `cc27xx` to an ESPHome YAML, run `esphome compile` in 35 s cold and 27 s warm, and `esphome upload` in 15 s. The LP-EM-CC2755P10 then joins Home Assistant's Zigbee network 5.7 s (first flash) and 6.3 s (third flash) later. It switches its LED 32 of 32 times at a median of 85 to 92.5 ms. The image is 262.7 kB of flash and 37.6 kB of RAM, with 124.4 kB free. The LP-EM-CC2340R5 runs the same way as an end device: 93 of 100 commands over five runs. With the heap cut by 2 kB it ran as a router, 50 of 50 commands.
stop: none
-->
# Can ESPHome's own commands build and flash the LP-EM-CC2755P10 and LP-EM-CC2340R5?

**Short answer.** Yes, for the LP-EM-CC2755P10 as a Zigbee router, and for the LP-EM-CC2340R5 as an end device, with a router on that chip possible only with a 2 kB smaller heap. Two ESPHome platforms (`cc27xx`, `cc23xx`) live in `ports/esphome-ti/components/`; a YAML points at them with `external_components`. In a clean virtual environment with only ESPHome 2026.9.1 and TI's four tools installed, `esphome compile` built the CC2755P10 image in 35 s from nothing (median of three, range 34 to 40) and 27 s the next time (25 to 32). `esphome upload` flashed it in 15 s (three flashes: 14.6 to 16.1 s). The board appeared in ZHA, Home Assistant's Zigbee integration, 5.7 s after the first flash and 6.3 s after the third, switched the red LED 32 of 32 times from Home Assistant (median 92.5 ms in the first 20, 85 ms in the next 12) and reported the chip temperature every 30 s. The flash and RAM are the same as the [script build](2026-10-13-cc2755p10-esphome-fit.md) to within 340 bytes. One thing did not work and is not explained: after a second flash, with the old device still listed in ZHA, the board did not rejoin in 200 s. If you see this, remove the device in ZHA first; it then joined in 6 s.

Terms: [vocab](../vocab.md). 1 kB = 1,024 bytes. A platform is the part of ESPHome that knows a chip family (`esp32:`, `nrf52:`); the TI ones are `cc27xx:` and `cc23xx:`. A router relays Zigbee traffic and stays awake; an end device sleeps and talks to a router. ZHA is Home Assistant's Zigbee integration. ZBOSS is the Zigbee stack in TI's SDK for the CC2340 and CC2755 chips ([vocab](../vocab.md)). DSLite is the command-line flasher in UniFlash. The shim is 388 lines that answer the Nordic and Zephyr calls ESPHome's Zigbee code makes ([script-build page](2026-10-13-cc2755p10-esphome-fit.md)). Cold means no earlier build in the folder; warm means the same folder built again after a change to one file.

**Question.** Can someone who has installed ESPHome in a virtual environment and TI's tools type `esphome compile` and `esphome upload` and get a TI chip into Home Assistant, with no build script from this repo in the way?

**Vendor position.** ESPHome has no TI platform and no documented way to add one, as the sources installed with ESPHome 2026.9.1 show, read 2026-10-13. Its own `platform_hooks.py` describes "a target platform supplied via external_components", so the loader allows it, and its nrf52 platform shows the shape: a config schema that records the target platform and framework, a pin schema, `to_code`, and two optional hooks, `run_compile` and `upload_program`. TI's side: the Zigbee example is built by a makefile and flashed with UniFlash or CCStudio, as the F3 SDK 9.21.00.36 [CC2755P10 README](../../cc2755p10/README.md) says; there is no ESPHome or PlatformIO route from TI.

**What was expected, written down before the first build** (2026-10-13, after reading ESPHome 2026.9.1's nrf52 platform, `platform_hooks.py` and `__main__.py`): an external component with `IS_TARGET_PLATFORM = True` can supply its own `run_compile` and `upload_program` hooks, because ESPHome's own platform_hooks.py comments describe a "target platform supplied via external_components". The firmware should be the same bytes as the script build (same generated C++, same flags): 263.1 kB flash and 37.6 kB RAM for the CC2755P10 router, 211.1 kB flash and 32.2 kB RAM as an end device on that chip, and 219.8 kB and 32.0 kB on the CC2340R5 end device. The first `esphome compile` should take about 75 s cold (the 62 s script run plus ESPHome's code generation) and 15 to 20 s warm if objects are reused and only the link repeats, since the link of a link-time-optimized build is the long step. The likely trouble is in ESPHome's validation, not the build: Zigbee's `switch` schema is switched on by the `nrf52` component being loaded, and the pin schema is per platform. The flash and the join should repeat the script build: joined within 10 s, 20 of 20 toggles, median near 90 ms. A stranger needs about 8 steps.

**What was tried.**

1. Read ESPHome's nrf52 platform and its hook registry. Wrote `components/cc27xx/platform_factory.py` (schema, pin, `to_code`, `run_compile`, `upload_program`) and `ti_build.py` (the steps of the `build.sh` script build in Python: SysConfig, compile in parallel with unchanged files skipped, link, CRC patch, Intel hex, DSLite). `cc23xx` is the same code under another name. The shim and the SysConfig files moved into `components/cc27xx/`; `build.sh` still works from them.
2. Added `die_temperature`, a small sensor component, because ESPHome's `internal_temperature` refuses any platform not on its list.
3. A clean test: a new virtual environment, only ESPHome 2026.9.1 installed, a copy of `components/` and the two YAML files in an empty folder, three cold and three warm compiles of each board, wall time from `/usr/bin/time -p` (the Mac was loaded: load average 5 to 8 on 12 cores).
4. On the CC2755P10 (probe LB07021W, holding TI's SDK blink): read main flash (0 to 0xE8000), CCFG and SCFG twice, identical both times and identical to the earlier backup; the image's CCFG and SCFG checked against TI's example (SAFE); `esphome upload --device LB07021W`; ZHA join; 20 and 12 switch commands from Home Assistant, the LED pin (DIO16) read from the chip after every third or fourth command; restore from the backup.

**How far it got.** Verified on the board.

| Step | Result | Cause tag | What fixed it |
|---|---|---|---|
| `esphome config` | `This feature is only available on [esp32, rp2, bk72xx, nrf52, ln882x]` for `internal_temperature` | Third-party behavior | The port's own `die_temperature` sensor component |
| Compile | `no member named 'ZephyrGPIOPin' in namespace 'esphome::zephyr'` | Our mistake | The generated `main.cpp` needs the two shim headers included; the platform adds them |
| Compile, second platform | The CC2340R5 LED is DIO14 and DIO15, not DIO7 (guessed) | Our mistake | Read from the generated `ti_drivers_config.c` |
| `esphome upload` | `External platform cc27xx is not importable` then `Cannot upload Over the Air` | Third-party behavior | `upload` reuses a cached validated config and never loads external components; the platform deletes that cache file at the end of its compile and upload hooks |
| Clean venv | `pip install esphome==2026.9.1` fails: `cbor2` 6 needs a Rust compiler on an Intel Mac | Third-party behavior | `pip install "cbor2<6" esphome==2026.9.1` |
| Second flash | No rejoin in 200 s while the old device was listed in ZHA | unknown | Remove the device in ZHA first; joined in 6 s |

Two ESPHome behaviors the platform works around, both in the code with their reasons: ESPHome's Zigbee component turns on its switch support only when a component named `nrf52` is loaded, so the platform adds that name to the loaded integrations (nothing from nrf52 is built); and the Zigbee sources are guarded by `USE_NRF52` and `USE_ZEPHYR`, so the platform passes both defines.

**On the board.** Probe LB07021W, LP-EM-CC2755P10, 2026-10-13.

| Check | Result | How checked |
|---|---|---|
| Flash | `esphome upload` 16.1 s, 14.6 s, 15.0 s (three flashes, one of them the failed-rejoin one) | `/usr/bin/time -p`, includes ESPHome's start-up and a second of config validation |
| Join | Entities in Home Assistant 5.7 s after the first flash ended and 6.3 s after the third (join window open before the flash); the second flash did not join | Entity timestamps; the flash-end stamp has 1 s resolution |
| 20 switch commands | 20 of 20, state settled median 92.5 ms (69 to 125), service call median 78 ms | Home Assistant entity state, polled every 5 ms |
| 12 more, with the pin | 12 of 12, median 85.0 ms (67 to 113); DIO16 read from the chip 4 of 4 (on, off, on, off); in the first run 5 of 5 off-state reads matched | DSLite memory read of the GPIO output register |
| Temperature | 20.69, 21.06, 20.88, 21.06, 21.25 C at 30 s spacing | Home Assistant sensor; not compared with a second thermometer |
| Restore | Flash 0 to 0xE8000, CCFG and SCFG read back identical to the backups, 18 s | `cmp` against the backups |

**On the CC2340R5 board.** Probe <probe serial>, LP-EM-CC2340R5, 2026-10-13. The board held a Find My beacon image, so a fresh backup came first: main flash 0 to 0x7FFFF (512 kB, the whole flash) and the 2 kB CCFG, each read twice, identical both times (an earlier backup of this board was 256 B short and had no CCFG). The restore script rebuilt a hex from the backup, flashed it and read flash and CCFG back identical, in 33 s; the ESPHome end device was then flashed again and left running.

| Check | Result | How checked |
|---|---|---|
| Flash | `esphome upload --device LS470GTU`: 16.7 s, 13.7 s, 15.1 s for the end device, 17.2 s for the router | `/usr/bin/time -p` |
| Join, end device that sleeps between polls (TI's default) | 45 s after the flash; Home Assistant commands then took 2 to 3 s each (the device is asked every 3 s) and some failed | Entity timestamps; toggle timer |
| Join, end device with its receiver on (ESPHome's default `sleepy: false`, which the platform now honours) | 19 s and 7 s after the flash | Entity timestamps |
| Commands, receiver-on end device, 5 runs of 20 | 93 of 100 landed; 20 of 20 in the last run (settled median 136 ms, min 102, max 717); the 7 misses were the first command after a quiet spell, which timed out after about 29 s; the LED pin (DIO14) read from the chip matched the commanded state 6 of 6, including 2 of the misses | Home Assistant state; DSLite memory read of the GPIO output register |
| Router with a 2 kB smaller heap | Joined 6 s after the flash; 20 of 20, then 30 of 30 more over 7 minutes (settled medians 97, 91, 124 and 189 ms); minimum free heap recorded by FreeRTOS: 0 bytes | Home Assistant state; DSLite read of `xMinimumEverFreeBytesRemaining` |
| End device heap, default 10.9 kB | Peak use 8.9 kB; minimum free 2 kB after 20 commands | same read |
| Chip temperature | 11 C at every report; the sensor's raw reading was 17.25 C and the driver's own correction gives 11; the CC2755P10 beside it read 21 C. Earlier BTHome work on this board read 13 to 14 C. Cause not found | Home Assistant; DSLite read of the sensor register |

The end device's receiver is now on by default because TI's example is a sleepy end device that polls its parent every 3 s, which makes a Home Assistant command wait for the next poll (2 to 3 s, with some timing out). The platform passes `-DED_RX_ALWAYS_ON=1` unless the YAML says `sleepy: true`. The die temperature now comes from TI's Temperature driver (it must be started by hand; adding its SysConfig module generated nothing). A router needs about 1.4 kB more RAM than this chip has at the default heap, so the platform takes `heap_size:`; 0x23BC links with 683 B spare, and 0x25BC fails to link because the RAM vector table needs an aligned hole.

**The line.** Nothing stopped it. Build time and image size, from the linker's memory table (the chip has 162 kB of RAM and a 920 kB application flash region; the CC2340R5 has 36.0 kB of RAM and 508 kB):

| Build | Flash | RAM used | RAM free | `esphome compile` cold / warm |
|---|---|---|---|---|
| CC2755P10, router, `cc27xx` | 262.7 kB | 37.6 kB | 124.4 kB | 35.1 s (33.8 to 40.1) / 27.0 s (25.5 to 31.9) |
| CC2340R5, end device, `cc23xx` | 219.9 kB | 32.0 kB | 4.0 kB | 31.5 s (31.2 to 31.5) / 22.4 s (22.2 to 23.3) |
| Script build, CC2755P10 router (for comparison) | 263.1 kB | 37.6 kB | 124.4 kB | 71.9 s, one run, serial |

The platform's image is 356 B smaller than the script's because `die_temperature` replaced `internal_temperature`. Parallel compiling and skipping unchanged files cut the cold build from 72 s to 35 s. Of the 27 s warm build, the link is 23 s: ESPHome's Zigbee code writes the build time into `main.cpp` on every run, so one object and the link are always redone. The CC2340R5 router does not fit (1.3 kB over, [the CC2340R5 fit page](2026-10-13-cc2340r5-esphome-fit.md)); the platform stops with a message that says so.

The stranger's steps: install ESPHome (26 s, 222 MB), install four TI tools (8 GB, once), copy the folder, optionally set the probe serial, `esphome compile`, open the join window, `esphome upload`. That is 7 steps, 10 if each TI tool counts, and 6 once the TI tools are there; the six take about 2 minutes of machine time and 5.7 to 6.3 s to join.

Safety: `upload` always names the probe by serial number in the connection file it writes (from `probe_serial:`, `--device` or the XDS110 port ESPHome offers). A wrong serial stops DSLite with "no XDS110 is connected" (tried with a made-up serial); it does not fall back to another probe, so a bench with many probes is safe.

**What this means.** The ESPHome side was less work than expected: the platform hooks exist and an external component can use them. The extra work was on the ESPHome side: component names hard-coded in the Zigbee options, a sensor that lists platforms, and an upload step that cannot find an external platform when the config cache is warm. TI's side needed nothing new; the six TI-side build errors of the script build already had their fixes in the shim.

**What this does not establish.** Two boards flashed, one day, two joins and 32 toggles on the CC2755P10, four joins and 190 commands on the CC2340R5. No long soak (the longest is 7 minutes), and the second CC2755P10 flash did not rejoin for a reason not found (no air trace, no coordinator log). On the CC2340R5 the first command after a quiet spell sometimes timed out (7 of 100) and the chip temperature reads about 10 C below the room; neither is explained. Only macOS, only ESPHome 2026.9.1, only Zigbee switch and sensor; no logger, so `esphome logs` shows nothing. Random bytes and ESPHome's saved settings are still stubs in the shim. Timings were taken on a busy Mac; a quiet one would be faster by an unmeasured amount.

**Ask.** can an external target platform be listed in the validated-config cache sidecar, so `esphome upload` finds its hooks? On the CC2340R5 the Temperature driver returns 11 C where the room is about 21 C (raw sensor 17.25 C, then the driver's correction); is a calibration step missing?

**Effort.** CC2755P10 run, hands-on plus machine time: about 35 minutes from the first read of ESPHome's nrf52 platform to the first successful `esphome compile`, 10 more to the CC2340R5, 6 for the upload hook, and 12 on the board including the backups and the restore. The join window and the flash were scripted. The CC2340R5 run took 1 hour 10 minutes from the backup to the restore proof and the final flash, in the same way.

**Verdict.** Good. `esphome compile` and `esphome upload` behave like any other platform's after one `external_components` entry, in half the time of the script, and the cost was ESPHome's three small assumptions about which chips exist, each worked around in under 20 lines.
