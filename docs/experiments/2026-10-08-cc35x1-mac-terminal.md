<!-- experiment
id: E109
board: LP-EM-CC35X1
date: 2026-10-08
status: works
reached: verified
line: A Mac terminal does everything the LP-EM-CC35X1 needs with no Windows step: build, sign, activate, program, serial and reset. Edit to blinking LED took 60 s; programming alone takes 20 s.
stop: none
-->
# Can a TI LP-EM-CC35X1 be built, activated, programmed and read over serial from a Mac terminal, with no Windows step?

**Short answer.** Yes. Every step ran from the command line on an Intel Mac with macOS 15.7.9: build with the SDK's makefile, sign with the Toolbox's image builder, activate once, program, then read the serial port. An edit to a blinking LED took 60 seconds. Programming alone takes 20 seconds. The Toolbox's Radio Tool is the one part TI lists only for Windows and Ubuntu, and it was not needed.

Terms: [vocab](../vocab.md) (activation, FreeRTOS, SysConfig). Errors and fixes: [LP-EM-CC35X1 FAQ](../../cc35x1/FAQ.md). 1 kB = 1,024 bytes.

**Question.** TI's guides show the Wi-Fi Toolbox on Windows. Can a Mac user get from a sealed board to a changed blink without leaving the terminal?

**Vendor position.** The LaunchPad guide SWRU629C section 2.5.1.1 names Windows 10 and Ubuntu 18 or later for the Radio Tool. The Toolbox install folder ships a macOS command line (`simplelink-wifi-toolbox`) with `activation`, `custom_activation`, `programming`, `detect_flash_type` and `get_fuse_data` (Toolbox 4.3.23 user guide, read 2026-10-07).

**What was tried.** LP-EM-CC35X1 Rev A on an LP-XDS110ET (TGT VDD on EXT), Wi-Fi SDK 10.20.00.39, Toolbox 4.3.23, SysConfig 1.28.0, TI Arm Clang 5.1.1. Commands are in the [README](../../cc35x1/README.md). Steps: read the flash type (`IS25WJ064F`) and activation state; activate with TI's SDK example key (about 5 seconds); build TI's `drivers/empty` with a 200 ms toggle; sign; program; read serial with a Python `termios` script; reset with `xds110reset`.

**How far it got.** Verified. The red LED blinked about five times a second, seen by eye. TI's Network Terminal also ran and printed its banner on the probe's serial port.

**The line.** No limit reached. Three things to know, each with a fix in the FAQ: `get_fuse_data` stops with `Life cycle OPERATIONAL is not valid` until the chip is activated; `tool_settings.json` needs absolute paths; and the serial baud must be set by the program that reads the port. The chip's cycle counter reads 0 in programs loaded this way, so timings use the 1 ms RTOS tick.

**Ask.** If you have a board that arrived already activated, does `get_fuse_data` print the table straight away?
