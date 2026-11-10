# How do you get a TI LP-EM-CC2755P10 LaunchPad development kit from the box to a changed blink on a Mac?

> Disclosure: Fox Red Labs is a lab run by a TI employee on personal time. Contributors' projects are their own. Not affiliated with, sponsored by, or endorsed by Texas Instruments or any other manufacturer.

**Measured on one board: the blink built in 14 s and the first changed blink took 41 s of machine time from the command line (2026-10-09, agent-driven, board already mounted on its debugger).** CoreMark ran at 426.31 per second at 96 MHz. The numbers TI's data sheet gives for this chip are listed beside them as claims, not as measurements.

Terms: [docs/vocab.md](../docs/vocab.md). Expected problems, with fixes: [FAQ](FAQ.md). 1 kB = 1,024 bytes.

## What is in this folder, and what is not

**In this folder:** this README, `AGENTS.md` (notes for an AI assistant, every line from a run), `FAQ.md` (every error hit, with its exact text and the fix), `RESULTS-cc2755p10.md` (the measured results), `prebuild.sh` (builds every target that needs no board; it writes outside the repository), and the demos: `baremetal/`, `ble/`, `ble-light/` a Bluetooth light, `matter-smoke/`, `pqc/` post-quantum crypto runs, `recovery/` recovery scripts and `thread/` Thread demos. Results are rows in `data/results/cc2755p10.csv` at the top of the repository, not in this folder.

**Not in this folder:** TI's SDK, CCStudio, SysConfig and the compilers (you install them; the versions are under "Where these facts come from"), and built images (`.out`, `.hex`, `.bin`, `.elf`), which are not cleared for redistribution. You build them with the command below.

## The board

The CC2755P10 chip: Arm Cortex-M33 at 96 MHz with a floating-point unit, 1,024 kB flash, 162 kB RAM, a math unit (APU) and a security unit (HSM), and a 2.4 GHz radio up to +20 dBm for Bluetooth 6.0 (with Channel Sounding), Zigbee, Thread and Matter (TI data sheet SWRS306E, September 2026). The LaunchPad adds a red LED (DIO16), a green LED (DIO12), two buttons, a 1 MB SPI flash chip, a printed antenna with a U.FL socket, and 40 header pins. It has **no USB port and no debugger**: it plugs onto an LP-XDS110ET or LP-XDS110 debugger, sold separately, which powers and programs it. TI had no public user guide for the board on 2026-10-07.

What fits that did not fit on the LP-EM-CC2340R5 (36 kB RAM): Thread as a router, Bluetooth and 802.15.4 at once, and TI's Matter build. Those are the first things to try.

Source for the rows above: TI data sheet SWRS306E (September 2026), TI's product page (2026-10-04) and `cc2755p10/FAQ.md`.

## Ten-minute path (measured: 41 s by command line)

What you need (bench): the board, an LP-XDS110ET debugger and its USB cable, and the tools above.

1. Put the debugger's TGT VDD jumper on **XDS**, slide the board's 20-pin edge into the debugger, plug the debugger's USB-C into the Mac.
2. In a terminal, from this repository's folder, copy TI's blink example out of the SDK and build it:

```bash
cp -R /Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS/examples/nortos/LP_EM_CC2755P10/drivers/empty .
```

```bash
make -C empty/ticlang SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR=/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh TICLANG_ARMCOMPILER=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
```

3. Put your debugger's serial number (`/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e` lists it) in place of `SERIAL_UNKNOWN` in [CC2755P10.ccxml](CC2755P10.ccxml), then flash:

```bash
/Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c cc2755p10/CC2755P10.ccxml -e -f -u empty/ticlang/empty.hex
```

The red LED toggles once a second. Change the sleep time in `empty/empty.c`, build and flash again: the LED changes speed. That change is the finish line. `-e` in UniFlash's tool prints progress; it is not erase.

Measured 2026-10-09: copy, edit, build and flash took 41 s on the command line, and 198 s through CCStudio's MCP tools.

## One-command build

```bash
cc2755p10/prebuild.sh
```

Builds the blink, CoreMark at two optimization levels, the shared benchmark images (including the four room-occupancy neural nets), every SDK example for this board, and TI's Thread images, into a folder outside the repository (the script prints its path). It touches no board, installs nothing, and refuses to start while the Mac is busy (add `--wait` to wait instead).

## What was measured

Measured rows were read on one LP-EM-CC2755P10 mounted on an LP-XDS110 debugger. Claim rows are TI's data sheet or expectations from other boards' numbers that nobody here has checked on this board.

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| Time to a changed blink, command line | 41 s | agent-driven, board already mounted, SDK and tools already installed | measured | `time_to_blink` (cli) in `data/results/cc2755p10.csv` |
| Time to a changed blink, CCStudio MCP tools | 198 s | same setup; counter read by DSLite after the run | measured | `time_to_blink` (ccs-mcp) |
| Blink build | 14 s (13.6 s) | TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3 | measured | `blink_build` (cli) |
| CoreMark, TI Arm Clang -O3, bare metal | 426.31 per second | 96 MHz, code in flash, 3 boots, crc checked; above the 370 expected and unexplained | measured | `coremark` (ticlang-bare-o3), `data/results/cc2755p10.csv` |
| CoreMark, -Oz | 211.01 per second | same; ratio to -O3 is 0.495 | measured | `coremark` (ticlang-bare-oz), `data/results/cc2755p10.csv` |
| Bluetooth echo round trip, 20 bytes | 29.2 ms median | Mac as central via bleak, 3 runs of 50, printed antenna, RSSI about -47 | measured | `ble_echo` |
| Digital-power loop, trigger to duty write | Q15 282 cycles mean, worst 284 (29.6 percent of a 100 kHz period); float 294 mean, worst 342 (35.6 percent) | 96 MHz, -O2, code in flash, FPU on, ADC and LGPT0 at 48 MHz; same PID C code as the LP-EM-CC2340R5 (Q15 410, float 2,016 cycles there) | measured | `power_loop`, `power_loop_worst_pct`, `power_loop_jitter` |
| Digital-power loop, highest rate | Q15 331 kHz, float 318 kHz (-O3: 338 and 329); CPU load at 100 kHz 23.5 and 26.5 percent | 1,000 periods a trial with no missed interrupt, late duty write or ADC overflow, 3 runs agree to the timer count; the LP-EM-CC2340R5 holds 97 and 22 kHz | measured | `power_loop_max_rate`, `power_loop_cpu_load`, bench/power-loop/ |
| CoreMark expected, TI Arm Clang -O3 | about 370 per second (3.85 per MHz at 96 MHz) | data sheet figure | claim | TI data sheet SWRS306E |
| RAM left beside a Zigbee router | about 130 kB (the CC2340R5 had 799 bytes left) | the CC2340R5 numbers plus the RAM difference | claim | CC2340R5 numbers in `data/results/cc2340r5.csv` |
| Thread full device (`ot-cli-ftd`) | forms its own network; leader within 12 s of `thread start`; the CC2340R5 build failed by 7.5 to 23.8 kB | TI's `ot-ti`, one board | measured | `stack_support` (thread-ftd), [thread/README.md](thread/README.md) |
| Current running CoreMark | 5.8 mA at 96 MHz, 3.3 V | data sheet | claim | TI data sheet SWRS306E |

The full list of 237 rows, including Zigbee, Thread, Matter and power, is in `data/results/cc2755p10.csv`.

## If you see this

- If you see `Error -2001` or `Unable to authenticate with Secure Debug Manager`, read the line before it and do not run a chip erase to "reset" the chip: the debug port opens only when both configuration areas, CCFG at 0x4E020000 and SCFG at 0x4E040000, are valid ([FAQ](FAQ.md#if-the-debugger-says-error--2001-or-unable-to-authenticate-with-secure-debug-manager-what-do-you-do)).
- If you see `Debug is not allowed based on` after a chip erase, the chip is not bricked: the erase cleared the SCFG and a valid one has to be written back ([FAQ](FAQ.md#after-a-chip-erase-every-command-on-the-lp-em-cc2755p10-fails-with-debug-is-not-allowed-based-on-and-error--2001-is-it-bricked-and-how-do-you-get-it-back)).
- If you see nothing at all when the board is plugged in, there is no USB port: mount it on an LP-XDS110ET or LP-XDS110 with the TGT VDD jumper on XDS ([FAQ](FAQ.md#the-board-is-plugged-in-and-nothing-happens-where-is-the-usb-port)).

The rest, including C++ and linker errors: [FAQ](FAQ.md).

## Limits

The data-sheet current in the table is a claim; no EnergyTrace power number is measured.

## Where these facts come from

Checked 2026-10-07 on an Intel Mac, macOS 15.7.9, and measured 2026-10-09: SimpleLink Low Power F3 SDK 9.21.00.36 LTS, TI Arm Clang 5.1.1.LTS (from CCStudio 21.0.1), SysConfig 1.26.3 (1.28.1 for the CCStudio MCP build), UniFlash 9.6.0. Files: `data/results/cc2755p10.csv`, `cc2755p10/AGENTS.md`, `cc2755p10/FAQ.md`.
