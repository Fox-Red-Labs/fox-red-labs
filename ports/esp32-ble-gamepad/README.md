# Can ESP32-BLE-Gamepad's report code run on the LP-MSPM0G3519, LP-EM-CC2340R5, LAUNCHXL-F2800137 and NUCLEO-U083RC with the same bytes?

Yes for the report code: on five boards the descriptor and the four test reports match the library's output on an ESP32-C6 byte for byte. The Bluetooth half runs on the LP-EM-CC2340R5: it advertises and serves Device Information. A second build (`LIVE=1`) paired from macOS Bluetooth settings, and the Mac received its button presses as HID reports, 15 of 15. The Python connect script waits about 31 s and stops, because the HID service needs a pairing a script cannot accept.

Terms: [docs/vocab.md](../../docs/vocab.md). 1 kB = 1,024 bytes. Measured 2026-10-20 on an Intel Mac, macOS 15.7.9.

## What was ported

[ESP32-BLE-Gamepad](https://github.com/lemmingDev/ESP32-BLE-Gamepad) by lemmingDev (MIT, 1,602 stars on 2026-10-19, commit `65d5178`): a Bluetooth LE HID gamepad library for Arduino-ESP32 on NimBLE-Arduino. Its generic mode builds a HID report descriptor and packs an input report (16 buttons, eight axes and sliders, one hat) and hands it to NimBLE. The port carries the descriptor builder and the report packer (`buildGenericDescriptor`, `sendGenericReport`, `press`, `release`, `pressStart`, `pressSelect`, `setAxes`, `setHat1`) and leaves out SInput, XInput, NuS, output and feature reports, motion and pairing. The upstream license is in `LICENSE-upstream.txt`.

## The check

Every board prints one block over its serial console every 3 s: `MAP` (the 96-byte report descriptor) and `RPT1` to `RPT4` (the 19-byte input report after four fixed call sequences), plus `TIME` (descriptor build and report pack, microseconds). `check/gp_serial.py` compares MAP and the four reports with a golden file, the output of the unmodified library on the ESP32-C6 (`harness/frl_gp_check.ino`; capture it from the serial console and save it as `check/golden.txt`). `check/gp_ble.py` scans and connects from the Mac with `bleak` and reads Device Information and Battery.

| Board | Code | What the board did | Flash | Static RAM | Report pack |
|---|---|---|---|---|---|
| ESP32-C6 (original) | the library, NimBLE-Arduino 2.5.1, Arduino-ESP32 3.3.12 | 5 of 5 lines are the golden output; advertises as "ESP32 BLE Gamepad"; Mac connects | 702.1 kB | 24.0 kB | 14 to 131 us (includes the NimBLE call) |
| ESP32-C6, core only | `boards/mspm0g3519/frl_gamepad.c` unchanged | 5 of 5 lines same | 270.9 kB | 15.5 kB | 1 us |
| LP-MSPM0G3519 | `boards/mspm0g3519/` | 5 of 5 lines same | 9.9 kB | 2.0 kB | 17 us at 32 MHz |
| NUCLEO-U083RC | `boards/nucleo-u083rc/` | 5 of 5 lines same | 3.6 kB | 880 B | 41 us at 16 MHz |
| LAUNCHXL-F2800137 | `boards/f2800137/` | 5 of 5 lines same (a char is 16 bits, every byte masked with 0xFF); run from RAM | 8.1 kB of code | 1.5 kB | 5.64 us at 120 MHz |
| LP-EM-CC2340R5 | `boards/cc2340r5/` | 5 of 5 lines same; advertises; Device Information equals the C6 (image of run 1; three runs built 188.5 to 189.4 kB) | 189.4 kB | 33.8 kB of 36.0 kB | 17 us at 48 MHz on a 10 us tick |

Numbers are rows in `data/results/<board>.csv` (tests `gamepad_*`; exact byte counts are there). Each board's core was written by a separate AI assistant that started from the repository, so the cores differ: the packer times of the U083 and the G3519 rows are not a comparison of the chips (the U083's descriptor time also covers state set-up).

## How it was done

Each port was one AI-assistant run, told the board and the library and to start from `llms.txt` and `PORTING.md`. Runs and list-price cost are in the experiment page. The check reads the board after the assistant finishes; the assistant's own report does not count as a pass.

## Run it

- Core on the Mac, for the CC2340R5 build: `boards/cc2340r5/host/` (`ref_harness.cpp` runs the library's own function bodies against a clone of the library at `65d5178` placed in `boards/cc2340r5/lib/`).
- Target images: `boards/cc2340r5/build.sh` (needs the SimpleLink Low Power F3 SDK 9.21.00.36, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0; it builds, it does not flash).
- Check a board that is running one of these images: `python3 check/gp_serial.py <board id>` (needs pyserial; ports are set at the top of the file) and `python3 check/gp_ble.py "ESP32 BLE Gamepad" 30 4` (needs bleak).

Limits: only the report code was ported. The CC2340R5 and the original ESP32-C6 run Bluetooth; the LP-MSPM0G3519, NUCLEO-U083RC and LAUNCHXL-F2800137 ports are report code only. Write-up: [docs/experiments/2026-10-19-esp32-ble-gamepad-port-matrix.md](../../docs/experiments/2026-10-19-esp32-ble-gamepad-port-matrix.md).
