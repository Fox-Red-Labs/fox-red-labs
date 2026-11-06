# What an AI coding assistant should know before making a TI LP-EM-CC2340R5 answer Nordic's phone apps

That one TI example plus two 8-line service descriptions is the whole port, that the Nordic UUIDs come from public headers and no Nordic firmware is copied, and that only a Mac has talked to it, never a phone. This file is the short list, each item from runs on one board.

Terms: [docs/vocab.md](../../docs/vocab.md) and the [README](README.md) in the Fox Red Labs repo. In short: a **GATT service** is a group of values a phone can read, write or be notified of; a **UUID** is the 128-bit number that names it; **SysConfig** is TI's tool that writes a chip's pin and radio setup code; **DSLite** is TI's command-line flasher (UniFlash 9.6.0); **bleak** is the Python Bluetooth LE client library. 1 kB = 1,024 bytes. Measured 2026-10-12 and rechecked 2026-10-25 on an Intel Mac, macOS 15.7.9, with SimpleLink Low Power F3 SDK 9.21.00.36 LTS, SysConfig 1.26.3 (1.28.0 gives a byte-identical hex), TI Arm Clang 5.1.1.LTS, UniFlash 9.6.0 and bleak 3.0.2, on one LP-EM-CC2340R5.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths below in backticks starting `ports/` are in the Fox Red Labs repo; they are not in your copy, and links in this file only work inside that repo. The files to copy are listed in "Take it with you" in `ports/nordic-apps/README.md`.

## The port

| Part | Fact |
|---|---|
| What it is | TI's `basic_ble` example with `basic_ble.patch` (3 files, 39 changed lines), `app/app_nordic.c` and four generated service files, built into one image that advertises as "FRL Nordic" (`ports/nordic-apps/README.md`) |
| Services | Nordic LED Button Service `00001523-1212-efde-1523-785feabcd123` and Nordic UART Service `6e400001-b5a3-f393-e0a9-e50e24dcca9e`, UUIDs from Nordic's public headers (`ports/nordic-apps/README.md`) |
| Size | 187.3 kB of flash, 4.0 kB of the 36 kB RAM left (`ports/nordic-apps/README.md`) |
| Proven from a Mac | Both services listed, 60 of 60 UART lines echoed identical, LED writes read back, write-to-answer median 24.3 ms |
| Not seen | nRF Blinky and nRF Toolbox on a phone, the button, the LED lit, the 2026-10-25 connect timings (the Mac's Bluetooth service crashed) (`ports/nordic-apps/README.md`) |
| Pins | Red LED on DIO14, BTN-1 on DIO10 (active low), both on the board, not on a BoosterPack header (`ports/nordic-apps/README.md`) |
| Licensing | Nordic's firmware may only be used with a Nordic chip, so it is read as a behavior spec and none is copied; Nordic's phone apps are BSD-3-Clause (`ports/nordic-apps/README.md`) |

## Start here

| Step | This port |
|---|---|
| Tools | F3 SDK 9.21.00.36 LTS at `<sdk>`, SysConfig at `<sysconfig>/sysconfig_cli.sh`, TI Arm Clang 5.1.1.LTS at `<cgt>` (`ports/nordic-apps/build.sh`) |
| Build | `SDK=<sdk> SYSCFG=<sysconfig>/sysconfig_cli.sh CLANG=<cgt> ./build.sh` (38 s); image `out/frl_nordic_cc2340r5.hex` |
| Probe serial | `xdsdfu -e` from CCStudio's `ccs_base/common/uscif/xds110/` folder |
| Back up | Read main flash and CCFG with DSLite `memory -r` before the first write |
| Flash | `<uniflash>/dslite.sh --mode flash -c <ccxml> -e -f -u out/frl_nordic_cc2340r5.hex` (12 to 25 s) |
| Check | `python3 host/nordic_test.py scan`, then `full`; needs `pip install bleak` and Bluetooth allowed for the terminal app |
| Restore | The backup, written back and read back |

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| The phone or `scan` does not list "FRL Nordic" | The board runs a different image | Flash `out/frl_nordic_cc2340r5.hex` again | Name and both UUIDs appear together (`ports/nordic-apps/README.md`) |
| SysConfig: `The max allowed data length for Legacy adv type is 31 ,the current length is 33` | A power-level field and the 16-bit UUID list are on by default | Set `scanRes1.GAP_ADTYPE_POWER_LEVEL = false` and switch the 16-bit list off | SysConfig passes (`ports/nordic-apps/README.md`) |
| The advertised service UUID is wrong | SysConfig writes a 128-bit UUID in the order given, not low byte first | Give the UUID with its bytes reversed | `scan` prints the Nordic UUIDs |
| The button does nothing in TI's serial menu | Intentional: the button is named `CONFIG_NORDIC_BTN_LEFT` so the menu leaves it alone | None | Button notifies instead |
| `./build.sh` stops with `sysconfig_cli.sh: No such file or directory` | The SysConfig version in the script is not installed | `SYSCFG=<sysconfig>/sysconfig_cli.sh ./build.sh` | Hex is byte-identical |
| The Mac sees "FRL Nordic" but the connect times out, or Bluetooth turns itself off | Cause unknown (suspected: an old pairing for the same chip, or macOS) | Turn Bluetooth off and on, remove old pairings of this board, connect again | `system_profiler SPBluetoothDataType` shows `State: On` |
| `ATT_GetMTU` does nothing | In this SDK's `icall_ble_api.h` it is `(AssertHandler(0,0))` | Take the MTU from `ATT_MTU_UPDATED_EVENT`, as `app_nordic.c` does | Notifications sized MTU minus 3 |

All rows: `ports/nordic-apps/README.md`, "FAQ".

## Rules for the assistant

1. **Do not copy Nordic firmware.** Read the public headers for UUIDs and properties, write the service from them (`ports/nordic-apps/README.md`, "Where it comes from").
2. **Edit the service description, not the generated C.** Change `services/*.json` and regenerate with `tools/gatt_gen/gatt_gen.py --target ti-f3`; without that tool, edit `src/*` knowing the next regeneration overwrites it (`ports/nordic-apps/README.md`).
3. **Keep the advertisement at or under 31 bytes**: 21 in the advertisement and 30 in the scan response today (`ports/nordic-apps/README.md`).
4. **Say what was not seen.** The LED, the button and every phone app are not run; do not write "works on a phone" (`ports/nordic-apps/README.md`, "Honest limits").
5. **Back up before the first write.** Read main flash and CCFG with DSLite `memory -r`, and write them back at the end.
