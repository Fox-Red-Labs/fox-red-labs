# Assistant notes for porting code to the TI CC35xx Wi-Fi family (LP-EM-CC35X1)

The chip must be activated once, by burning a key into fuses, before it takes any program. Its program cannot be backed up. TI Drivers app source moves unchanged between the F3 SDK and the Wi-Fi SDK. Each line below comes from a run or a file in the Fox Red Labs repo.

Terms: [docs/vocab.md](../../../docs/vocab.md) and the [Wi-Fi family FAQ](FAQ.md). **TI Drivers** is TI's driver layer shared by the F3 and Wi-Fi SDKs; **activation** is the one-time fuse step; the **Toolbox** is TI's SimpleLink Wi-Fi Toolbox (activates, signs, programs); **SysConfig** writes pin and driver code from a `.syscfg` file; **OTA** is an over-the-air update. 1 kB = 1,024 bytes. Checked 2026-10-25 with Wi-Fi SDK 10.20.00.39, Toolbox 4.3.23, SysConfig 1.28.0, TI Arm Clang 5.1.1.LTS and F3 SDK 9.21.00.36, on one LP-EM-CC35X1 Rev A.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths in backticks are in the Fox Red Labs repo, where the evidence for each line lives; they are not in your copy.

## The part

| Fact | Value | Source |
|---|---|---|
| Chip | CC3551E, Cortex-M33 at 160 MHz, 1.1 MB RAM, Wi-Fi 6 on 2.4 and 5 GHz, Bluetooth LE 5.4; 8 MB external flash | `cc35x1/AGENTS.md` |
| Debugger | none on the board: LP-XDS110ET or LP-XDS110 with TGT VDD jumper on EXT, J4 and J6 on the lower two pins, and a second USB-C cable into the board | `cc35x1/AGENTS.md` |
| Serial | `/dev/cu.usbmodem<probe serial>1`; the port ending in 4 stays silent | `cc35x1/AGENTS.md` |
| TI driver layers | TI Drivers (same names as F3) and FreeRTOS; no DriverLib | `portability/matrix.csv` X26 |
| Not on this chip | 802.15.4 (no Thread or Zigbee); no Arduino core, MicroPython port, PlatformIO platform or Rust crate; no IAR or Keil support | `data/support-matrix.json` |

## Start here

| Step | Command or file |
|---|---|
| Check activation (reads only) | `./simplelink-wifi-toolbox programmer -i XDS110 -param1 <probe serial> get_fuse_data --flash_type IS25WJ064F --activation_type sdk_example_key --verbose` |
| Back up | not possible; the Toolbox has no read-back. Save the output of the check above |
| Build the shared app | `portability/three-ways/tidrivers/build_cc35x1.sh` |
| Build a TI example | `make` in `freertos/ticlang` with `SIMPLELINK_WIFI_SDK_INSTALL_DIR`, `SIMPLELINK_WIFI_TOOLBOX_INSTALL_DIR`, `SYSCONFIG_TOOL`, `TICLANG_ARMCOMPILER` set and `GCC_ARMCOMPILER=` empty |
| Sign and program | Toolbox image builder, then `programming --tool_settings <absolute path>`; reset with S1 or `xds110reset -s <probe serial>` |
| Restore | reprogram TI's prebuilt Network Terminal from the Toolbox folder; after the Wi-Fi CSI program run `full_flash_erase` first |

## Rules for the assistant

1. **Never run the activation step without asking.** It cannot be undone; `Life cycle OPERATIONAL is not valid` means it is needed (`cc35x1/AGENTS.md`).
2. **Overwrite the Wi-Fi SDK example's `.c` files with the F3 ones** for the 11 same-name examples; do not edit the app lines (`portability/matrix.csv` X26).
3. **Use SysConfig 1.28.0** for the Wi-Fi SDK (`portability/everywhere/README.md`).
4. **Time with the FreeRTOS tick**, not `DWT->CYCCNT`, which reads 0 here (`cc35x1/AGENTS.md`).
5. **Use absolute paths in `tool_settings.json`** (`cc35x1/AGENTS.md`).
6. **Do not flash a raw `.bin` from address 0** (`cc35x1/AGENTS.md`).
7. **Free disk before a batch of builds.** The Toolbox and linker die without a message at a few hundred MB free.
8. **Type the Wi-Fi network name**; never read it from `ipconfig getsummary en0`, which prints `<redacted>` on macOS 15 (`ports/wifi-provisioning-ble/README.md`).
9. **Reuse TI's provisioning service, `mqtt_client` interface and NVOCMP storage** instead of another vendor's code (`ports/wifi-provisioning-ble/README.md`).

## What ran here from elsewhere

| Code | Result | Source |
|---|---|---|
| CC2340R5 TI Drivers examples | 11 of 11 unchanged, all programmed | `portability/matrix.csv` X26 |
| Shared three-ways app | serial check passed; TI Drivers glue 57 lines | X53, `portability/three-ways/tidrivers/cc35x1/` |
| ESPHome Wi-Fi device | 20 of 20 switch commands, median 120 ms | `docs/experiments/2026-10-13-esphome-cc35xx-platform.md` |
| DNS sinkhole | 71,492 blocked names, median 6 ms | `docs/experiments/2026-10-20-cc35x1-dns-sinkhole.md` |
| Matter thermostat | pairs into Apple Home | `docs/experiments/2026-10-10-cc35x1-matter-thermostat.md` |

## Next

The family guide is `portability/families/wifi/README.md`; every port and test on record is `portability/families/wifi/EVIDENCE.md`; board bring-up is `cc35x1/AGENTS.md`.
