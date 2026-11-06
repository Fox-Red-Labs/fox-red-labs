# Assistant notes for porting code to the TI CC23xx, CC27xx and CC13xx radio family (LP-EM-CC2340R5, LP-EM-CC2755P10, LP-CC1354P10-1)

TI Drivers code moves between the three boards with the app files untouched. MSPM0 code needs a shim and has no DriverLib landing. Two boards can lock out their own debug port: a chip erase on the CC2755P10, a sleeping image on the CC1354P10. Each line below comes from a run or a file in the Fox Red Labs repo.

Terms: [docs/vocab.md](../../../docs/vocab.md) and the [radio family FAQ](FAQ.md). **TI Drivers** is TI's driver layer shared by the F3 and F2 SDKs; **SysConfig** writes pin and driver code from a `.syscfg` file; **CCFG** and **SCFG** are boot configuration areas; **shim** is a header of renames and stubs with no logic; **DMM** is TI's scheduler that shares one radio between two stacks. 1 kB = 1,024 bytes. Checked 2026-10-25 with F3 SDK 9.21.00.36, F2 SDK 8.33.00.16, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3 and 1.28.0, UniFlash 9.6.0 and Zephyr 4.4.2, on one board of each kit.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths in backticks are in the Fox Red Labs repo, where the evidence for each line lives; they are not in your copy.

## The parts

| Part | Fact | Source |
|---|---|---|
| CC2340R52 | Cortex-M0+ at 48 MHz, 512 kB flash, 36 kB RAM; no USB port, needs an LP-XDS110ET or LP-XDS110 probe; F3 SDK | `cc2340r5/AGENTS.md` |
| CC2755P10 | Cortex-M33 at 96 MHz, 1,024 kB flash, 162 kB RAM; no USB port, needs a probe; F3 SDK; a chip erase closes the debug port | `cc2755p10/AGENTS.md` |
| CC1354P10 | Cortex-M33 at 48 MHz, 1 MB flash, 256 kB SRAM for programs; XDS110 on the board; F2 SDK; debugger connects only in the first seconds after power-on | `cc1354p10/AGENTS.md` |
| TI driver layers | TI Drivers in F3 and F2 with the same names; no DriverLib | `portability/mspm0-to-wireless.md` |
| Missing | I2C target driver in the F2 SDK; Matter build for the CC2340R5; Zephyr board for the CC2755P10 and CC1354P10 | `portability/matrix.csv` X14, `cc2340r5/AGENTS.md`, `cc2755p10/AGENTS.md`, `cc1354p10/AGENTS.md` |

## Start here

| Step | Command or file |
|---|---|
| Build a TI example | `make -C <example>/ticlang SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR=<SDK> SYSCONFIG_TOOL=<sysconfig_cli.sh> TICLANG_ARMCOMPILER=<ti-cgt-armllvm_5.1.1.LTS>` (the F2 SDK uses `SIMPLELINK_CC13XX_CC26XX_SDK_INSTALL_DIR`) |
| Build the shared app | `portability/three-ways/tidrivers/build_cc2340r5.sh`, `build_cc2755p10.sh` |
| Build MSPM0 apps inside the F3 projects | `portability/shim/build_twins.sh m0_to_f3 <work> <results.jsonl>` |
| Back up | the two `dslite.sh --mode memory` reads in `cc2340r5/AGENTS.md` and `cc2755p10/AGENTS.md`, taken before the first flash; the CC1354P10 has no tested backup command |
| Check the image | `python3 tools/preflight_image.py <board id> <image>` |
| Flash | `dslite.sh --mode flash -c <board>.ccxml -e -f -u <image>.hex`; never `-v`, never the `.bin`, never a chip erase on the CC2755P10 |

## Rules for the assistant

1. **Write to TI Drivers, not DriverLib.** MSPM0 DriverLib has no landing on a radio chip (`portability/everywhere/README.md`). Keep `DL_*` calls out of app code (`portability/mspm0-to-wireless.md`).
2. **Use the shim for MSPM0 TI Drivers apps.** Force-include `portability/shim/mspm0_on_f3/fr_mspm0_on_f3.h` and define each `CONFIG_*_IOMUX` name as 0 (`portability/shim/README.md`).
3. **Copy the target board's example and overwrite its `.c` files** when moving between radio boards; SysConfig and the linker file stay the target's (`portability/matrix.csv` X11).
4. **Check RAM left before porting a stack.** CC2340R5: Bluetooth `basic_ble` 5.1 kB, Zigbee router 799 B with logging on, full Thread device fails to link (`cc2340r5/AGENTS.md`).
5. **On the CC2755P10 flash only a hex whose CCFG and SCFG are valid.** A DSLite load erases first and clears the SCFG; TI Arm Clang builds of SDK examples carry both, `ot-ti` GCC builds do not (`cc2755p10/AGENTS.md`).
6. **On the CC1354P10 load straight after power-on.** Use CCStudio scripting (`cc1354p10/tools/ccs_load_run.py`), or switch the probe's hub port off for 6 s and load at once; a DSLite flash does not start a new program until `xds110reset -s <probe serial>` (`cc1354p10/AGENTS.md`).
7. **Pass the SysConfig path your Mac has.** The SDK's `imports.mak` names folders a Mac lacks; SysConfig 1.28.0 builds the F3 9.21 examples when 1.26.3 is missing (`cc2340r5/AGENTS.md`).
8. **Reconfigure a Zephyr pin interrupt after the pin is configured.** Zephyr 4.4.2 `gpio_cc23x0` clears the edge-detect bits on configure (`portability/arduino/zephyr/README.md`).
9. **For PWM on the CC2340R5 red LED use LGPT1 channel 2.** The board file's `<&pwm5 0 ...>` does not reach DIO14 (`ports/micropython/zephyr-cc2340r5/README.md`).
10. **Probe I2C with a one-byte register read, not a scan.** The scan sends an empty write that fails on the CC23x0 driver (`portability/arduino/zephyr/README.md`).
11. **Keep the protocol in plain C with a host test and the vendor calls in one glue file** (`portability/driver-layering.md`).

## What ran here from elsewhere

| Code | Result | Source |
|---|---|---|
| CC2340R5 driver examples on CC2755P10 | 33 of 33 unchanged, ran | `portability/matrix.csv` X11 |
| CC2340R5 driver examples on CC1354P10 | 8 of 8 unchanged, ran | X12 |
| MSPM0G3507 TI Drivers apps on CC2340R5 | 0 of 10 unchanged, 10 of 10 with the shim, first 5 ran | X1 |
| MSPM0G3507 TI Drivers apps on CC1354P10 | 4 of 8 with glue, 4 of 4 ran | X13 |
| CC35X1 and CC2340R5 TI Drivers twins | 11 of 11 each way | X26, X27 |
| ML-KEM-768 and ML-DSA-44 from the CC1354P10 | 6 of 6 on the CC2755P10, MSPM33C321A and MSPM0G3519; 3 of 6 unchanged on the MSPM0G5187 | X39 to X42 |
| Arduino Blink and AnalogReadSerial | ran on the CC2340R5 | `portability/arduino/zephyr/README.md` |
| MicroPython 1.29.0 | ran on the CC2340R5, 174.8 kB flash, 29.9 kB RAM | `ports/micropython/zephyr-cc2340r5/README.md` |
| Nordic LED Button and UART services | Mac client works; phone apps not checked | `ports/nordic-apps/README.md` |

## Next

The family guide is `portability/families/radio/README.md`; every port and test on record is `portability/families/radio/EVIDENCE.md`; board bring-up is in `cc2340r5/AGENTS.md`, `cc2755p10/AGENTS.md` and `cc1354p10/AGENTS.md`.
