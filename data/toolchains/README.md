# Toolchain claims

What each installed TI SDK says it supports, counted from the build folders it ships. This is what TI claims, not what was built. Nothing here was compiled. Terms: [docs/vocab.md](../../docs/vocab.md).

## Files

- `claims.csv`: one row per (sdk, board, rtos_or_kind, toolchain). `examples` is how many examples ship a project for that toolchain. `kind_total_examples` and `board_total_examples` are the denominators, so `examples / board_total_examples` is the share of that board's examples with the toolchain. `file_types` says which project files were found (projectspec, makefile, ewp, uvprojx, CMakeLists.txt).
- `sdk-compilers.json`: per SDK, the compilers and versions TI names, each with the source file and the quoted line. `cmake_toolchains` lists CMake toolchain files where an SDK has them.
- Scanner: `tools/toolchains/inventory.py` (stdlib only, read only).

## Rerun

    python3 tools/toolchains/inventory.py             # rewrite claims.csv and print the summary
    python3 tools/toolchains/inventory.py --summary   # print only
    python3 tools/toolchains/inventory.py --markdown  # the table below
    python3 tools/toolchains/inventory.py --check-quotes   # every quote in sdk-compilers.json still in its file

`sdk-compilers.json` is written by hand from the cited files. `--check-quotes` is the guard when an SDK is updated. The PDF quotes for C2000Ware are marked `quote_checkable: false`.

## How a count is made

- An example is one (board, kind, example folder). The same example on two boards counts twice. Multi-core examples (AM263Px) count once per board and kind.
- kind is nortos, freertos, tirtos7, threadx, rtos (no RTOS subfolder), system (AM263Px multi-core system project), or, for C2000Ware, the top folder of the tree.
- A toolchain counts when its folder (ticlang, gcc, iar, keil, ti_arm_clang, iar_arm, keil_arm_clang, ti-arm-clang) holds a project file. cmake counts when a CMakeLists.txt sits next to the example. ccs is a C2000Ware .projectspec; the board column for C2000Ware is the device family (f28003x), not a LaunchPad.
- C2000Ware library projects (static libraries) are skipped. `c2000ware_core` is a subset copy of C2000Ware; do not add the two together.
- Only `examples/` (and `solutions/` for AM13E230x) are scanned, except C2000Ware, which is scanned whole. Third-party folders (ThreadX ports, TF-M) are not examples and are ignored.

## Summary (examples that ship each toolchain)

| SDK | examples | ticlang | gcc | iar | keil | ccs | cmake |
|---|---|---|---|---|---|---|---|
| mspm0_sdk_2_11_00_07 | 1911 | 1911 | 1453 | 1357 | 1307 | 0 | 0 |
| mspm33_sdk_1_04_00_00 | 259 | 259 | 176 | 166 | 157 | 0 | 0 |
| am13e230x_sdk_26_01_00_03 | 193 | 193 | 0 | 165 | 1 | 0 | 193 |
| simplelink_lowpower_f3_sdk_9_21_00_36_LTS | 643 | 643 | 461 | 554 | 0 | 0 | 0 |
| simplelink_wifi_sdk_10_20_00_39 | 64 | 64 | 64 | 0 | 0 | 0 | 0 |
| c2000ware_26_02_00_00 | 3584 | 0 | 0 | 0 | 0 | 3584 | 0 |
| c2000ware_core_26_02_00_00 | 397 | 0 | 0 | 0 | 0 | 397 | 0 |
| mcu_plus_sdk_am263px_26_01_00_08 | 640 | 640 | 0 | 0 | 0 | 0 | 0 |
| mmwave_l_sdk_05_05_04_02 | 461 | 461 | 0 | 0 | 0 | 0 | 0 |

## What stands out

- TI Arm Clang is the one toolchain every example has in every Arm SDK. GCC, IAR and Keil are partial.
- MSPM0: of 1.9k examples, 1.5k have GCC, 1.4k IAR, 1.3k Keil (1,453, 1,357 and 1,307 in claims.csv). Share varies by board: LP_MSPM0G3507 has GCC for 198 of 282, IAR for 181, Keil for 172. CUSTOM_BOARD has all four for all 55.
- MSPM33: GCC 176, IAR 166, Keil 157 of 259.
- AM13E230x: no per-example GCC folder. GCC works through the CMake toolchain file and the command line only, and TI says CCS does not build it. IAR covers 165 of 193 (the lp board 86 of 100, som 79 of 93). Keil has a single example. The SDK defaults enable only TI Arm Clang.
- F3 (CC23xx/CC27xx): GCC 461 and IAR 554 of 643; no Keil. The release notes name no Keil.
- SimpleLink Wi-Fi (CC35x1): TI Arm Clang and GCC only, 64 examples each. No IAR or Keil in any example.
- C2000Ware: CCS projectspecs with the TI C2000 compiler only. No GCC, IAR or Keil, and no Arm Clang.
- AM263Px and mmWave L: TI Arm Clang only. AM263Px's imports.mak sets an IAR path, but no example ships an IAR project.
- No CC13xx/CC26xx SimpleLink SDK is installed under /Applications/ti or ~/ti.

## How the search was done, 14 Oct 2026

The cells on the Software page's grid that say "None found" or "Community" come from a search on 14 Oct 2026. The pages named here were read, not just their search snippets. "None found" means it was not found on these pages. It does not mean TI or the vendor has none.

- **IAR for C2000.** IAR's TI partner page lists Arm, 8051 and MSP430 columns, and IAR's VS Code extension lists no C28x target. No IAR compiler for C28x was found. For the Arm families, TI's SDK notes name IAR 9.70.2 (MSPM0, CC23xx and CC27xx, MSPM33) and 9.70.1 (AM13E2x).
- **Zephyr.** Upstream Zephyr 4.4.0 has boards for the LP-MSPM0G3507, LP-MSPM0G3519, LP-MSPM0L2228 and LP-EM-CC2340R5. The LP-MSPM33C321A and LP-AM13E230 are on Zephyr's main branch only. TI keeps its own trees: SimpleLink Zephyr for the CC23xx and CC27xx (v4.4.0-ti-10.10.00, marked GA) and msp-zephyr for the MSPM33 (v4.2.0-ti-1.02.00.00, early access).
- **PlatformIO.** PlatformIO has no official TI Arm platform. One community platform exists for the MSPM0, `Berkays/platform-ti-mspm0`, which its author calls very experimental. Nothing was found for the CC23xx, CC27xx, MSPM33 or CC35xx.
- **MicroPython.** MicroPython's Zephyr port runs on any board Zephyr supports. No dedicated MSPM0 or CC23xx port was found, and whether the Zephyr port builds on those boards.
- **Rust.** `embassy-mspm0` is in the embassy repository and targets the MSPM0 and MSPS003 families. crates.io holds only a placeholder, so it is used as a git dependency. No Rust crate was found for the CC23xx, CC27xx or MSPM33.
- **Arduino.** One community core exists for the MSPM0 (`sckulkarni246/arduino-mspm0`), which builds the MSPM0G3507 only. No current core was found for the CC23xx or CC27xx.
