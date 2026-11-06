# Board to board over Bluetooth LE: CC2340R5 central, CC1354P10 peripheral

Two TI chips from two different SDKs talking with no phone or computer in the loop. The LP-EM-CC2340R5 (F3 SDK, Cortex-M0+) scans for the name **"Fox Red 1354"**, connects to the LP-CC1354P10-1 (F2 SDK, Cortex-M33, running TI's prebuilt Project Zero, renamed in `../../cc1354p10/`), finds Project Zero's LED0 characteristic by its 128-bit UUID, and writes it 1, 0, 1, 0 twenty times, timing every write.

## Result (2026-10-06)

| Step | Result |
|---|---|
| Find "Fox Red 1354" by name | Found at RSSI -33 dBm, boards about 30 cm apart on the desk |
| Connect | Connection interval 100 ms (80 x 1.25 ms) |
| Discover LED0 by UUID F0001111-0451-4000-B000-000000000000 | Value handle 0x0022 |
| 20 acknowledged writes | **20 of 20**, round trip min 194.0, average 195.4, max 199.8 ms |

The round trip is two connection events: the write goes out in one 100 ms event and its acknowledgement comes back in the next. A shorter connection interval would shorten it; the radio link added nothing measurable on top.

## What was built

TI's `basic_ble` example from the F3 SDK 9.21.00.36, with:

- `app/app_foxred_link.c` (new, about 190 lines): scan, match the name in advertising data, connect, `GATT_DiscCharsByUUID`, then `GATT_WriteCharValue` in a loop, each write started on the previous write's response.
- `app/app_central.c`: one call to `FoxRed_start()` at the end of `Central_start()`.
- `freertos/basic_ble.syscfg`: role set to `CENTRAL_CFG`, advertising lines commented out (SysConfig's command line fails with "Cannot read properties of undefined (reading 'advData1')" if a central keeps them), device name "Fox Red CC2340 central".
- `freertos/ticlang/makefile`: one object and one rule for the new file.

Build: `cd freertos/ticlang && make SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR=... SYSCONFIG_TOOL=... TICLANG_ARMCOMPILER=...` (200 kB flash; 1 kB = 1,024 bytes). Run: `run_b2b.py <hex> <backup dir> <log> [seconds]` backs up the CC2340R5 (it may hold another image), flashes the central, captures the UART, and writes the backup back.

## If you see this

- If the central connects but never writes: the 128-bit UUID bytes are in the wrong order (GATT wants them least significant byte first).
- If the first write is refused: discovery had not finished. Start the write from the discovery-complete callback.

Terms: **central** is the side that scans and connects; **RSSI** is received signal strength; **GATT** is Bluetooth LE's data model; a **characteristic** is one readable or writable value in it. See also: [Thread](../thread-two-radios-2026-10-06/README.md) and [Zigbee](../zigbee-cc1354-cc2340/README.md).
