<!-- experiment
id: E243
board: LP-EM-CC2340R5
date: 2026-10-12
status: waiting
reached: run
line: The LP-EM-CC2340R5 advertises Nordic's LED Button Service and Nordic UART Service and a Mac drives both (LED write answered in 24.3 ms median, 60 of 60 UART lines echoed identical); the LED, the button and the phone apps have not been seen.
stop: none
-->
# Can an LP-EM-CC2340R5 answer Nordic's phone apps, nRF Blinky and any Nordic UART terminal?

**Short answer.** From a Mac, yes: the board shows both Nordic services, the LED characteristic takes writes (24.3 ms median round trip, 60 of 60 answered) and the UART service echoed 60 of 60 lines back identical (24.2 ms median). Nordic's two phone apps, the physical button and the LED lighting did not run.

Terms: [vocab](../vocab.md). 1 kB = 1,024 bytes. Full port, numbers and FAQ: [ports/nordic-apps/README.md](../../ports/nordic-apps/README.md).

**Expectation, written before the first build.** The F3 SDK's Bluetooth stack uses the same GATT calls as the F2 stack that the GATT code generator (`gatt_gen`) already generated for, so two 8-line descriptions should generate working C with include changes only; one 128-bit UUID fits in the advertisement beside the flags (nRF Blinky filters on it and on a name); the Mac should see the board within a second or two of flashing and round trips should sit near the connection interval, 30 to 50 ms.

**Question.** Can TI's Bluetooth stack serve the two services Nordic's phone apps look for, with the same UUIDs, with no Nordic code?

**Vendor position.** Nordic: the UUIDs are in `lbs.h` and `nus.h` of the nRF Connect SDK, and nRF Blinky (BSD-3-Clause source) lists devices that have a name and advertise the LED Button UUID (read 2026-10-12; sources in the port README). TI: neither TI Bluetooth SDK ships either service; the F3 SDK's `simple_gatt` profile uses 16-bit UUIDs `0xFFF0` to `0xFFF5`.

**What was tried.** TI's `basic_ble` for the LP-EM-CC2340R5 (F3 SDK 9.21.00.36, SysConfig 1.26.3, TI Arm Clang 5.1.1, FreeRTOS), changed by one patch (`basic_ble.patch`, 3 files) and four generated files plus `app_nordic.c`. `gatt_gen` got a target for the F3 SDK (`--target ti-f3`) and a `variable` length field. Commands: `./build.sh`, DSLite flash, `host/nordic_test.py full` with `bleak` on a Mac.

**How far it got.** Run, checked from the Mac. Built in 38 s on the first try; flash 187.3 kB (`basic_ble` as shipped: 187.4 kB), 4.0 kB of 36 kB RAM left. First advertisement 0.17 to 0.69 s after the flash finished (three flashes). The Mac found it by name and by each service UUID, listed both services with the right properties, wrote the LED characteristic 60 times with every write answered and read back, and got 60 of 60 UART lines echoed identical, plus a 101-byte line back as 62 plus 39 bytes (ATT MTU 65). Not reached: the board's LED seen lit, the button notifying, either phone app. The LED was not seen lit, so no light is claimed.

| Measured | Value |
|---|---|
| Flash, RAM used, RAM left | 187.3 kB, 32.0 kB, 4.0 kB |
| First advertisement after DSLite exits | 0.69, 0.17, 0.17 s |
| LED write with response, median of 20, three runs | 24.2, 24.3, 24.5 ms (min 22.6, max 44.7) |
| UART echo of a 14-byte line, median of 20, three runs | 24.3, 23.9, 24.2 ms (min 22.7, max 39.8), 60 of 60 identical |

The latency is a round trip on the Mac clock with a connection interval macOS chose; the board's own time is less.

**The line.** No limit reached. Things that cost time: SysConfig writes a 128-bit advertising UUID in the order given, not low byte first (the Mac showed it reversed until it was given the reversed bytes); two default-on advertisement fields (16-bit UUID list, power level) pushed the scan response to 33 bytes of 31 and the error does not name them; `ATT_GetMTU` is an assert in this SDK so the MTU comes from an event. The example's serial menu opens buttons named `CONFIG_BUTTON_0` and `_1`, so the button instance has to be renamed.

First proof, no vendor example: TI has no example of a custom 128-bit service on the CC2340R5 and nothing for Nordic's UUIDs. The nearest working code was `gatt_gen` (F2 target) and the thermometer port. Effort: about 1 hour, one board and one flash cycle per test, nothing by hand except the phone check still open. Added to the generator: the `ti-f3` target and the `variable` field in `gatt_gen` (its 15 tests pass, and the old target's output is byte-identical). Verdict: good for a developer, with two SysConfig traps that cost a few minutes each; the stack itself behaved.

**Ask.** Run nRF Blinky (tap FRL Nordic, switch the LED, press BTN-1) and nRF Toolbox UART (send hello, press BTN-1) and report what the screens and the board show.
