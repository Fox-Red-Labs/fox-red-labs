# Recipes on Linux, Windows and other boards: what carries over

Terms: [vocab](vocab.md). Every recipe here was run on one Intel Mac (macOS 15.7). This page says what carries over to Linux and Windows and to boards not held on this bench, what changes, and where to look it up. Tested means it was run here; expected means TI's documents say the same tool exists there and no reason was found that it would differ; not run means exactly that.

## The short answer

The recipe is the same on every operating system for every Arm board here: TI's compiler, TI's SysConfig, the board's SDK, and a flasher, all driven from a terminal. Only four things change between operating systems: where TI's installers put things, how the serial port is named, whether a USB rule or driver is needed, and the script extension (`.sh` or `.bat`). The runner `start.sh` and the check `tools/check/blink.py` look in TI's default folder for each system and were run on the Mac only.

| | macOS (tested) | Linux (expected, not run) | Windows (expected, not run) |
|---|---|---|---|
| TI installers' default folder | `/Applications/ti` | `~/ti` | `C:\ti` |
| XDS110 probe | No driver (`mspm0l1306/README.md`) | Needs TI's udev rules once: `<ccs>/ccs/install_scripts/install_drivers.sh` (CCStudio user's guide, "Linux host support") | Drivers install with CCStudio or UniFlash |
| Board's serial port | `/dev/cu.usbmodem<probe serial>1` | `/dev/ttyACM0` (the first of the probe's two ports) | `COMn`; Device Manager shows two XDS110 ports, the application one is the board |
| SysConfig command line | `sysconfig_cli.sh` | `sysconfig_cli.sh` | `sysconfig_cli.bat` |
| UniFlash command line | `dslite.sh` | `dslite.sh` | `dslite.bat` |
| xPack OpenOCD | one zip, `openocd` | one zip, `openocd` | one zip, `openocd.exe` |
| Shell for `start.sh` | Terminal | any shell | Git Bash or WSL; a PowerShell twin is not written |
| Claude Code | native | native | native or WSL (Claude Code's install page) |

What is known is not the same: the IWRL6432 radar SDK ships Linux and Windows installers only, so on a Mac that one board runs in a VM (`mac-setup/README.md`); on Linux or Windows it is native and the VM step disappears. CCStudio's system requirements page lists x86_64 for every system; on Apple silicon the TI tools run under Rosetta and that was not timed.

## By board family: what is the same and what differs

The build is the same everywhere a family's SDK ships makefiles with TI Arm Clang, which is every Arm board here. The flasher is what differs, because it depends on the chip's debug interface.

| Family (boards here) | Compiler | Flasher from a terminal | Probe | What to check first on a sibling board |
|---|---|---|---|---|
| MSPM0 (L1306, C1104, G3507, G3519, G5187) and MSPM33 (C321A) | TI Arm Clang, SDK makefile, SysConfig | xPack OpenOCD 0.12.0-7 or later (the first with `ti_mspm0.cfg`), `target/ti_mspm0.cfg` | XDS110 on the board | The SDK has an `examples/nortos/<your LaunchPad>/` folder, and the LED pin in `gpio_toggle_output`'s `ti_msp_dl_config.c` |
| SimpleLink Low Power F3 (CC2340R5, CC2755P10) | TI Arm Clang, SDK makefile, SysConfig | UniFlash `dslite` with a `.ccxml` naming the chip and SWD | None on the board; LP-XDS110ET clips on | The `.ccxml` chip name, and the TGT VDD jumper on the probe |
| SimpleLink CC13xx and CC26xx (CC1354P10) | TI Arm Clang, SDK makefile, SysConfig | UniFlash `dslite` with cJTAG and the ICEPick router | XDS110 on the board | Probe firmware at least 3.0.0.43 (`xdsdfu -e` prints it); OpenOCD has no target for these |
| SimpleLink Wi-Fi (CC35X1, CC3235SF) | TI Arm Clang, SDK makefile, SysConfig | SimpleLink Wi-Fi Toolbox `programmer` (CC35X1 needs one-time activation) | None on the CC35X1; XDS110 on the CC3235SF | Whether the chip must be activated, and the external flash part name |
| C2000 (F2800137, F28P65X) | `cl2000`, 16-bit bytes, Driverlib makefile | UniFlash `dslite`; the F2800137 is cJTAG only | XDS110 on the board | Probe firmware 3.0.0.43 or later; portable Arm code needs the C2000 FAQ's byte-size fixes |
| AM13E (AM13E230) and AM26 (AM263P) | TI Arm Clang, SDK makefile, SysConfig | UniFlash `dslite` with a `.ccxml`; flash it with a system reset and read once (`am13e230/AGENTS.md`) | Isolated XDS110 on the board | The board ships with empty flash; only the power LEDs light until a program is loaded |
| mmWave radar (IWRL6432) | TI Arm Clang, mmWave L SDK makefile | the SDK's `arprog_cmdline.py`; Radar Toolbox has no flasher | XDS110 on the board | The host must be Linux or Windows (TI's SDK page); a Mac runs it in a VM |
| Linux boards (SK-TDA4VM, SK-AM62) | none on your computer | an SD card image (`.wic.xz`) written by any imaging tool | serial console over USB | The exact image file name and the console's baud rate; nothing about the host OS matters after the card is written |

Every row cites a run in that board's folder: `<folder>/AGENTS.md` carries the commands with the paths as installed on the Mac, and `<folder>/FAQ.md` has the exact error text seen and the fix.

## Predicting a board not held on this bench

Use the family table. A board is expected to work with a recipe when all three hold: it is in a family above, its SDK has a makefile example for that LaunchPad, and its probe is the one in the row. Then only three facts change, and all three are in TI's documents: the LaunchPad kit's folder name inside the SDK, the LED pin in the SDK's own blink example, and the probe's serial number on your desk. The project pages say the same thing in four words per board (tested with this setup, expected to work, needs adaptation, not assessed); the second word comes from this table.

A board needs adaptation when the flasher differs from its family (no OpenOCD target, a chip that must be activated, a probe that needs new firmware) or when the host OS is restricted (the radar SDK). A board is not assessed when TI's documents name a tool that has not been run here at all.

## Where the documents are

- The board: ti.com/tool/<board name> (for example `ti.com/tool/LP-MSPM0L1306`) links the user's guide, the schematic and the SDK. The user's guide has the jumpers, the LED pins and the probe.
- The SDK: `<SDK folder>/docs/` after install, or the same pages on dev.ti.com under Software. The examples are in `<SDK folder>/examples/`, one folder per LaunchPad, each with a `README.md` and a `ticlang/` or `gcc/` makefile folder.
- CCStudio and its AI panel: TI's user's guide at software-dl.ti.com/ccs/esd/documents/users_guide/, including the AI assistant and MCP pages (`index_ai.html`). Default install folders per operating system are on its "Installation" page.
- UniFlash command line: the `dslite` help in UniFlash's own folder and TI's UniFlash quick start guide.
- xPack OpenOCD: xpack-dev-tools on GitHub (releases for all three systems); the TI targets are in its `scripts/target/` folder.
- This repo's notes for your board: `<folder>/AGENTS.md`, then `<folder>/FAQ.md`, then `<folder>/README.md`; the whole index is `llms.txt`.

## What would change this page

A run on Linux or Windows, which turns a row's "expected" into "tested" or into a FAQ entry. The fastest way is `./start.sh <board> blink` on that computer and a report through the Community page with the operating system in it.
