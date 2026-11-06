<!-- experiment
id: E216
board: LP-MSPM0G3519, LP-AM13E230, LP-MSPM33C321A, LP-MSPM0C1104, LP-EM-CC2340R5, LP-EM-CC35X1, LAUNCHXL-F2800137, LP-AM263P
date: 2026-10-18
status: in progress
reached: verified
line: An open tool programmed TI's blink on the LP-MSPM0G3519 (upstream OpenOCD, 0.95 s) and the LP-EM-CC2340R5 (TI's own OpenOCD fork, 6.2 s, verify step broken); the LP-AM13E230 and LP-MSPM33C321A were read byte for byte identical to DSLite or the build, write not run; the CC35X1 needs TI's Toolbox to open its debug port; the C2000 UART flasher's host is Windows-only source.
stop: tools; the CC35X1 debug port opens only through TI's Toolbox, the C2000 host program is Win32 code, and two boards stayed read-only by decision
-->
# Can a reader flash every TI family on the bench with an open tool instead of CCStudio, UniFlash or the Toolbox?

For four of six families on USB today, yes or nearly: upstream OpenOCD built from git programmed TI's blink on the LP-MSPM0G3519 in 0.95 s, and TI's own OpenOCD fork programmed it on the LP-EM-CC2340R5. The LP-AM13E230 and LP-MSPM33C321A have open drivers that read the chip exactly; their write was not run here. The LP-EM-CC35X1 cannot be opened without TI's Toolbox, and the C2000 UART flasher does not build on a Mac as shipped.

Terms: [docs/vocab.md](../vocab.md). **SACI** is the CC23xx device management interface on its security access port. **NONMAIN** is the MSPM0 and MSPM33 configuration flash. **nRST** is the chip's reset pin, which the XDS110 can drive. 1 kB = 1,024 bytes.

| Board | Open tool | Result | Gate reached | Time |
|---|---|---|---|---|
| LP-MSPM0G3519 | OpenOCD master, `mspm0` driver (TI's, upstream) | Works | Backup, identity, dump equal to DSLite, blink, restore, read back equal | 0.95 s blink (6 runs, 0.86 to 1.09 s) |
| LP-EM-CC2340R5 | TI's fork `ti-openocd`, `cc23xx` driver | Works, with a broken verify step | Backup, blink written and read back right, restore, read back equal | 6.2 s, command exits 1 at verify |
| LP-AM13E230 | OpenOCD master, `am13e230x` driver (TI's, upstream) | Not run (write) | Identity and dump equal to the motor demo's build | 6.7 s for the 520 kB read |
| LP-MSPM33C321A | TI's fork, `mspm33` driver; master has none | Not run (write) | Identity and dump equal to DSLite, both tools | 5.3 s (fork), 7.8 s (master, generic) |
| LP-MSPM0C1104 | OpenOCD master, `mspm0` driver | Can't today | Identity through the configuration port only | 0.5 s |
| LP-EM-CC35X1 | TI's fork, `ti_cc35x1e_launchpad.cfg` | Can't | Debug port identity only; core port closed | n/a |
| LAUNCHXL-F2800137 | C2000Ware `serial_flash_programmer` | Can't on a Mac as shipped | Source read; host not built | n/a |
| LP-AM263P (returned) | MCU+ SDK `uart_uniflash.py`; OpenOCD `am263p-launchpad.cfg` | Not run | Docs read | n/a |

**Question.** For every TI family on the bench, can a reader flash it with an open tool (OpenOCD from git, or an open UART flasher TI ships in source) instead of CCStudio, UniFlash or the Toolbox?

**Vendor position.** OpenOCD master (`bb5a52da`, 2026-10-16) carries TI's own contributions: `tcl/target/ti/mspm0.cfg` with `src/flash/nor/mspm0.c`, `tcl/target/ti/am13e230x.cfg` with `src/flash/nor/am13e230x.c` (both "Copyright (C) 2026 Texas Instruments"), and `am263p-launchpad.cfg` on the `k3` target. Zephyr's `lp_mspm33c321a` board page says: "OpenOCD support for MSPM33 is not upstream in the OpenOCD project. A custom TI OpenOCD fork must be built before using `west flash`" and gives `git clone -b ti-release https://github.com/TexasInstruments/ti-openocd` (read 2026-10-18). TI hosts Mac builds of that fork for its VS Code debug extension at <https://software-dl.ti.com/ccs/esd/vscode/ti-embedded-debug/ti-openocd.html> (1.3.1.50, 2026-03-31; the page says later builds move to ti.com). C2000Ware's `serial_flash_programmer/README.txt`: "Before running serial_flash_programmer.exe, the microcontroller must be attached to a COM port, must be running the SCI boot loader."

**What was tried.** Intel Mac, macOS 15.7.9, 2026-10-18 14:50 to 15:30. Two OpenOCD builds:

| Build | Source | Build time on this Mac |
|---|---|---|
| Upstream | `openocd-org/openocd` master `bb5a52da`, built earlier today for the CC23xx port (`ports/openocd-cc23xx/README.md`) | 37 s make |
| TI's fork | `TexasInstruments/ti-openocd` branch `ti-release`, head `cb22a31e` (2026-07-02), GPL-2.0-or-later per its source headers (GitHub shows no license tag), `./configure --enable-cmsis-dap --enable-cmsis-dap-v2 --enable-xds110 --disable-werror` | clone 13 s, bootstrap and configure 45 s, make 54 s; reports itself as "0.12.0+dev" |

The fork adds, over master: flash drivers `mspm33.c`, `cc23xx.c`, `cc27xx.c` with a shared `cc_lpf3_base.c` and `cc_lpf3_flash.c`, its own `mspm0.c` and `am13e230x.c`; target files `ti_mspm33.cfg`, `ti_cc23xx.cfg`, `ti_cc27xx.cfg`, `ti_cc2341.cfg`, `ti_cc35x1e.cfg`; board files for the LP-MSPM33C321A, eleven CC23xx and CC27xx LaunchPad development kits and the CC35x1E LaunchPad. For the MSPM33 it adds `mspm33_board_reset`, `mspm33_factory_reset`, `mspm33_mass_erase` and `mspm33_start_bootloader`.

Every board command ran with one tool at a time on the probe, after `lsof` showed no process on its serial ports. Each board was backed up before any write, and the backups are not in the repo.

**How far it got.**

*LP-MSPM0G3519 (<probe serial>).* The board was found with main flash blank (all 0xFF; data flash and NONMAIN equal to the 2026-10-08 backup). Neither DSLite ("Connection to MSPM0 core failed") nor OpenOCD ("Could not find MEM-AP to control the core") could reach the core; the debug port answered and the configuration port read DEVICEID 0x0BBA902F. OpenOCD's `mspm0_board_reset` (nRST from the XDS110) brought the core port back. Then: DSLite backup twice, identical; OpenOCD read of main (512 kB), data (16 kB) and NONMAIN identical to DSLite; master named the part, "Chip is MSPM0G3519SPZR rev 0", where xPack 0.12.0-7 says "Unidentified PART". TI's `gpio_toggle_output` (480 bytes, built in 6 s) programmed with `program blink.hex verify reset exit` in 1.04, 0.97 and 1.09 s, and with `program blink.hex verify` plus `mspm0_board_reset` in 0.93, 0.86 and 0.89 s, each "Verified OK". PB22, PB26 and PB27 toggled about every 500 ms, read through the debugger by a second tool. Restore: TI's demo (the first 13.0 kB of the backup) written with OpenOCD, main erase 0.94 s, write 4.5 s; DSLite read main, data and NONMAIN back identical to the 2026-10-08 backup; the core runs the demo's delay loop. The board is on TI's demo, not blank: that is the state its bench row records.

*LP-EM-CC2340R5 (<probe serial>).* DSLite backup twice, identical, equal to this morning's restored tag image. TI's fork, `program blink-cc2340r5.hex verify reset exit` with TI's SDK `drivers/empty` hex: SACI chip erase, main (3 sectors) and the CCFG written over SACI, "MAIN and CCFG Programmed", then "Verify Failed: Target not examined", exit 1, no reset, 6.2 s. Read back with this repo's `cc23xx` build: main flash equal to the hex, the rest erased, CCFG equal to the board's (the user record kept); after a reset DIO14 toggled about once a second. Two more tries at 15:20 failed to connect at the same moment five probes dropped off USB (below); they are not counted. Restore with this repo's `cc23xx` driver: 512 kB written in 10.2 s, verified in 0.9 s, read back with the CPU held in the boot ROM identical to the 15:16 backup, main and CCFG; left running the tag image.

*LP-AM13E230 (<probe serial>).* Read-only by choice: the board runs the guarded FOC image with a Hall calibration. OpenOCD master with TI's `am13e230x-launchpad.cfg` (JTAG): CPUID 0x411FD210, DID 0x2F, "MAIN flash: 512 KB (256 sectors), 2 bank(s)", NONMAIN 8 kB. Main flash and NONMAIN dumped in 6.7 s; two dumps identical; main flash equal to the motor demo's `foc.out` of 2026-10-17 23:18 (71.1 kB of load image, the rest 0xFF). DHCSR read 0x01000001 before and after: the core never halted. DSLite was not used, because it connects with a halt and the FOC README says a halt leaves the PWM at its last duty. The same driver wrote this board on 2026-10-10 (12.6 kB in 0.106 s, [AM13E230 Zephyr page](2026-10-09-am13e230-zephyr-upstream.md)).

*LP-MSPM33C321A (<probe serial>).* DSLite read of main (1 MB at 0x10000000), data (32 kB) and NONMAIN (2 kB), equal to the 2026-10-17 backup. Master has no MSPM33 file, so a generic Cortex-M33 over SWD was declared: DPIDR 0x6BA02477, AP0 an AHB5 memory port, DEVICEID 0x0BBB702F from the configuration port, CPUID 0x411FD210, all three dumps identical to DSLite in 7.8 s. TI's fork with `ti_mspm33_launchpad.cfg`: three flash banks found, the same three dumps identical in 5.3 s. No write, by decision: the demo's backup is the only way back to it.

*LP-MSPM0C1104 (MC110001).* DEVICEID 0x3BBA102F from the configuration port; no core port, before or after `mspm0_board_reset` ("DebugSS failed to come up within timeout"). This board had not shown its core port since 2026-10-15. Found 2026-10-19: its program faulted before `main` and reset the chip in a loop; recovered without an unplug ([mspm0c1104-lockout-root-cause](2026-10-19-mspm0c1104-lockout-root-cause.md)), and upstream `board/ti/mspm0-launchpad.cfg` now connects to it normally.

*LP-EM-CC35X1 (<probe serial>).* The fork's `ti_cc35x1e.cfg` declares no flash bank: it is debug only. Its `init_board` runs `simplelink-wifi-toolbox programmer -i XDS110 -param1 <serial> debug --debug_action_req_path <file>` on every connect, so TI's Toolbox opens the debug port first. With `init_board` stubbed out: DPIDR 0x4BA02477, access ports 0x102E0001, 0x002E0002 and 0x002E0000, and no memory port for the core. Nothing was halted or reset.

*LAUNCHXL-F2800137.* Read only, board untouched. The host source (`serial_flash_programmer.cpp` and four files in `source/`, 5,640 lines) includes `windows.h` and `conio.h` and talks to the port with `CreateFile`, `DCB`, `ReadFile` and `WriteFile`; it ships as a Visual Studio 2019 solution and a Windows `.exe`. It lists `f280013x`. It was not built: a Mac needs that serial layer rewritten. The flash kernel for this chip is not prebuilt in the programmer folder; it is `driverlib/f280013x/examples/flash/CCS/flash_ex3_sci_flash_kernel.projectspec`, whose post-build step runs `hex2000 -boot -sci8 --boot_align_sect -a`. On the board (user guide SPRUJ32, tables 3-3 and 3-4): boot switch S3 to GPIO24 = 0 (left), GPIO32 = 1 (right) for "Boot from SCI / Wait boot", and S2 SEL1 = 0 (the default) so GPIO28 and GPIO29 reach the XDS110's serial port.

*LP-AM263P (returned to its owner).* From the MCU+ SDK 26.01.00.08 docs: `tools/boot/uart_uniflash.py` is Python (pyserial, tqdm, xmodem, pyelftools), named for COM ports and `/dev/ttyACM0`, so it should run on a Mac. It needs the board in UART boot mode (the SDK's EVM setup page shows the LaunchPad kit's switch positions as a picture), the prebuilt flash writer `sbl_prebuilt/am263px-lp/sbl_uart_uniflash.release.tiimage` and a config such as `mcelf_sbl_ospi.cfg` that lists the bootloader and the application; it writes the external OSPI flash. OpenOCD's `am263p-launchpad.cfg` sets the XDS110, JTAG at 250 kHz and the `k3` target, with no flash bank: debug and RAM load only.

**The line.**

| Family | Where it stops | Cause tag |
|---|---|---|
| MSPM0 | Nowhere for a reader. A chip with blank flash hides its core until an nRST | Tool or docs |
| CC23xx | TI's fork writes correctly but its `program` command fails at verify: its examine-start hook reads `$DEVICE_BLANK`, which is never set (`tcl/target/ti_lpf3.cfg` line 415), so the target is never examined and the reset never runs | Tool or docs |
| AM13 | Not reached: write not run here | None |
| MSPM33 | Not reached: write not run here; upstream has no driver | Tool or docs |
| CC35xx | The debug port opens only after TI's closed Toolbox sends its signed request; no flash bank for the external flash in any OpenOCD | Tool or docs |
| C2000 | No C28x core in OpenOCD; TI's UART flasher host is Windows code | Tool or docs |

How TI's fork and this repo's driver differ on the CC23xx (source read and the run above):

| | TI's fork `cc23xx` | This repo's `cc23xx` |
|---|---|---|
| Erase | SACI chip erase, once per run; later erase requests are "discarded" | The boot ROM's sector erase, only the sectors an image covers |
| Main flash write | SACI pipelined program, sectors padded with 0xFF, then a SACI check | The boot ROM's program call on the halted CPU, 2 kB at a time |
| CCFG | Written over SACI, user record skipped | Never written; refused unless equal |
| Blank chip | Its examine-start hook rewrites the CCFG permission fields from the image to allow debug and flash, then programs (code read, not run) | Not handled |
| Blink on the CC2340R5 | 6.2 s, exit 1 at verify, no reset | 0.37 s with verify and reset |

The two times are not like for like: the fork erases the whole chip and writes the CCFG; the driver here erases three sectors.

What happened along the way: at about 15:20 five probes dropped off USB together (<probe serial>, <probe serial>, <probe serial>, <probe serial>, <probe serial>) and came back by 15:25. The cause is not known; the two failed fork tries on the CC2340R5 fell in that window.

**Ask.** Would TI take the fork's `mspm33`, `cc23xx` and `cc27xx` drivers upstream, and fix the `DEVICE_BLANK` line in `ti_lpf3.cfg`? Would TI publish the CC35xx debug-unlock request so OpenOCD can open the port without the Toolbox? Would C2000Ware ship the serial flash programmer's host with a POSIX serial layer?
