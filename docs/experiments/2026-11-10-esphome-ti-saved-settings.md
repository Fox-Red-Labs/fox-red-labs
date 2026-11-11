<!-- experiment
id: E322
board: LP-EM-CC2755P10
date: 2026-11-10
status: works
reached: verified
line: ESPHome's saved settings (`preferences:`, `restore_mode`, `globals: restore_value`) are now kept in a flash region through TI's NVS driver on the cc27xx and cc23xx platforms. A counter kept over three software resets on both the LP-EM-CC2755P10 and the LP-EM-CC2340R5 (1, 2, 3, 4). The region costs 8 kB of the application flash.
stop: none
-->
# Can ESPHome's saved settings survive a reset on the TI platforms?

**Short answer.** Yes. Before, the platform stubbed ESPHome's preferences, so a `restore_mode: RESTORE_DEFAULT_OFF` switch or a `globals` value with `restore_value: yes` started from its default every boot. Now the settings are written to an 8 kB region just under the Zigbee stack's own flash area, through TI's NVS driver. A `globals` counter incremented at every boot read 1, 2, 3 and 4 over three software resets on the CC2755P10 and on the CC2340R5, and the log line `Loaded 1 settings` (or 2) appeared at each restart.

Terms: [vocab](../vocab.md). NVS is TI's non-volatile storage driver. A software reset is `App.safe_reboot()`.

**Question.** Can an ESPHome device on these chips keep a value across a reset?

**What was expected.** ESPHome's preferences need a backend that stores keyed byte strings. TI's NVS driver erases by sector and writes in aligned blocks, so a small log-structured store with two halves (so a power cut during a rewrite never loses the data) should do. The region must come out of the application flash, so the linker file's flash length has to shrink by the same amount.

**What was tried.** A new shim (`preferences_f3.cpp`, ESPHome's class names, an append log with a checksum per record and a compaction into the other half when full), a SysConfig NVS instance at 0xE4000 (CC2755P10) and 0x7D000 (CC2340R5), and the linker file patched in the build copy. The test YAML counts boots in a persisted global and restarts itself until the count is a multiple of 4.

**How far it got.** Verified on both boards.

| Step | Result | Cause tag | What fixed it |
|---|---|---|---|
| Compile | `redefinition of 'ZephyrPreferences'` | Our mistake | The shim headers were included twice by two paths; include them from the build folder |
| CC2755P10 | `boot_count` 1, 2, 3, 4; "Loaded 2 settings" at each restart | | |
| CC2340R5 | `boot_count` 1, 2, 3, 4; "Loaded 1 settings" at each restart | | |

**What this does not establish.** Only software resets. No power cut during a write, no wear test, and the compaction path (a full half) was not exercised. `esphome upload` erases the whole application flash, so a new image starts with empty settings, and the Zigbee network state is cleared with them.

**Ask.** None.

**Effort.** 1 hour 20 minutes.

**Verdict.** Good. 330 lines of shim and an 8 kB region.
