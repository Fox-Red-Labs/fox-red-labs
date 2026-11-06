# Bluetooth light (LP-EM-CC2755P10 and LP-EM-CC2340R5)

Terms: [docs/vocab.md](../../docs/vocab.md). Errors and fixes: [FAQ](../FAQ.md). 1 kB = 1,024 bytes.

Status: Should work. A phone page controls the kit's LEDs over Bluetooth Low Energy. The firmware builds for both boards and has not run on either.

The firmware is TI's `basic_ble` example from the SimpleLink Low Power F3 SDK 9.21.00.36 with one added file, `frl_light.c`, a GATT service modelled on TI's `simple_gatt_profile.c`. TI's license header stays in the file. `apply.py` makes the four edits to the example's copy (call `FrlLight_start()` in `app_main.c`, device name and advertising data in the SysConfig file, one rule in the makefile); the SDK itself is never changed.

## What it does

It advertises as a connectable peripheral named `FRL Light`. The advertising data holds the flags and the 128-bit service UUID (21 bytes); the name is in the scan response (11 bytes), so `navigator.bluetooth.requestDevice({filters: [{services: [UUID]}]})` finds it. No pairing or bonding is asked for or needed.

Service UUID `8a570001-4724-4979-ab46-406d217495e5`

| Characteristic | UUID | Use | Format |
|---|---|---|---|
| color | `8a570002-4724-4979-ab46-406d217495e5` | read, write | 3 bytes R,G,B. A channel is on at 128 or more. The kit has red and green LEDs only, so blue is ignored. Writing it ends any pattern. |
| pattern | `8a570003-4724-4979-ab46-406d217495e5` | write | Up to 16 steps of 5 bytes: R, G, B, then the step time in ms as a little-endian uint16 (minimum 20). Plays in a loop. An empty write goes back to the solid color. A long write (more than the MTU) is accepted in chunks. |
| status | `8a570004-4724-4979-ab46-406d217495e5` | read, notify | ASCII, notified once a second: `up=12s solid` or `up=12s step=2/4`. |

At power-up the LEDs show solid green (color 0,255,0).

## LED pins

From TI's board files in the SDK (`source/ti/boards/.meta/LP_EM_CC2755P10.syscfg.json` and `LP_EM_CC2340R5.syscfg.json`, through the `basic_ble` SysConfig file's `CONFIG_GPIO_LED_RED` and `CONFIG_GPIO_LED_GREEN`). Both LEDs are active high.

| Kit | Red | Green |
|---|---|---|
| LP-EM-CC2755P10 | DIO16 | DIO12 |
| LP-EM-CC2340R5 | DIO14 | DIO15 |

## Build

    cc2755p10/ble-light/build.sh               # LP-EM-CC2755P10
    cc2755p10/ble-light/build.sh cc2340r5      # LP-EM-CC2340R5 (also cc2340r5/ble-light/build.sh)

About 30 seconds. It copies the SDK example to a temporary folder, applies this repo's files, builds with the SDK's makefile (SDK 9.21.00.36, SysConfig 1.26.3, TI Arm Clang 5.1.1.LTS; override with `FRL_F3_SDK`, `FRL_SYSCONFIG_DIR`, `FRL_TICLANG`) and writes `prebuilt/<board>/ble-light/`: `ble-light.out`, `ble-light.hex`, `manifest.json` (`release_cleared: false`) and `NOTICE.txt`.

## The CC2755P10 image and its configuration areas

A CC27xx keeps its debug port closed unless both CCFG (0x4E020000) and SCFG (0x4E040000) are valid ([FAQ](../FAQ.md)). The `.hex` is made by `cc2755p10/thread/make_flash_hex.py`, which takes main flash from the build and the CCFG and SCFG from `known-good/` (read from the board on 2026-10-15), and refuses the build unless its own CCFG and SCFG match those in every byte except the CRC fields and the CCFG user record (0x750 to 0x7CF). This build matched: the SCFG is identical, and the CCFG differs only in the user record. The image never reaches the HSM area (0xE8000 and up).

Bytes the CC2755P10 `.hex` writes (212.3 kB of data in all):

| Region | From | To (exclusive) | Size |
|---|---|---|---|
| main flash, vector table | 0x00000000 | 0x000000CC | 204 B |
| main flash, code and constants | 0x000000E0 | 0x0003452C | 209.1 kB |
| CCFG | 0x4E020000 | 0x4E020800 | 2 kB |
| SCFG | 0x4E040000 | 0x4E040400 | 1 kB |

The CC2340R5 image writes main flash 0x0 to 0x2FE9C (in four pieces, listed in its manifest) and the CCFG the SDK built (0x4E020000, 2 kB); that CCFG matches the one read from the CC2340R5 on the bench on 2026-10-11 except the user record (0x750 to 0x7CF). The `.hex` carries no SCFG (the CC2340R5 has none).

Bond storage (NVS) sits at 0xE5000 on the CC2755P10 and 0x7D000 on the CC2340R5; the `.hex` leaves it alone.

## Not run

Nothing ran. These are unchecked: that Chrome on Android and Bluefy find and connect, that an 80-byte pattern write works at the MTU the phone negotiates (the code accepts chunked long writes), and that the 1 s notification arrives.
