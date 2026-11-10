# TI LP-MSPM0L1306 LaunchPad on a Mac: errors and fixes

Every problem hit bringing up the TI LP-MSPM0L1306 LaunchPad (MSPM0L1306 chip: Arm Cortex-M0+, 32 MHz, 64 kB flash, 4 kB RAM; onboard XDS110 probe) from a Mac terminal on 2026-10-06 and 2026-10-07. Each answer starts with the fix, names what caused it, and quotes the exact error text so a search finds it. Versions: Intel Mac, macOS 15.7.9, MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, UniFlash 9.6.0, xPack OpenOCD 0.12.0+dev, Zephyr 4.4.2 with GCC 12.2.0.

**What caused it**, one tag per answer:

| Tag | Meaning |
|---|---|
| Our mistake | A mistake in this repo's runs. Listed so you do not repeat it |
| Setup or handling | How the hardware was plugged in or set up |
| TI example default | A TI example's default works on TI's bench but not for this job |
| Tool or docs | A TI tool bug, a missing tool, or a missing or wrong instruction |
| Chip limit | The silicon cannot do it, usually RAM |
| Third-party behavior | Upstream Zephyr, OpenOCD, macOS or open-source code, not TI |
| Unknown | Not found |

Contents: [Setting up CCStudio and the AI on a Mac](#setting-up-ccstudio-and-the-ai-on-a-mac), [Connecting and flashing](#connecting-and-flashing), [Memory](#memory), [Analog and sensors](#analog-and-sensors), [Zephyr](#zephyr), [Building and moving code](#building-and-moving-code), [DriverLib, TI Drivers and Zephyr](#driverlib-ti-drivers-and-zephyr).

Words used below (more in [docs/vocab.md](../docs/vocab.md)): the **probe** is the XDS110 debugger built into the LaunchPad; it shows up on the Mac as two serial ports, `/dev/cu.usbmodem<serial>1` (the board's UART) and `...4`. **OpenOCD** is an open-source flashing and debug tool. **NONMAIN** is the chip's boot configuration area at 0x41C00000. 1 kB = 1,024 bytes.

## The one-command route (`./start.sh` and `tools/frl`)

Checked on 2026-10-14 on an Intel Mac with this board, MSPM0 SDK 2.11.00.07, SysConfig 1.26.3, TI Arm Clang 5.1.1.LTS and xPack OpenOCD 0.12.0-7. The route itself is in [docs/frl.md](../docs/frl.md); every code is in [docs/frl-errors.md](../docs/frl-errors.md).

### Do I need an AI account or CCStudio to blink a pattern on the LP-MSPM0L1306?

No. `./start.sh mspm0l1306 blink --probe <serial> --pattern 150,150,150,900` builds, flashes and checks it with TI's SDK, SysConfig, compiler and xPack OpenOCD, no assistant and no CCStudio window; about 13 s of machine time on this bench. An assistant is optional (`--assistant claude` or `codex`).

### `./start.sh` says "FRL-E001 REFUSED: no XDS110 debug probe found on USB". The board's light is on. Why?

The cable is most likely charge-only, or it is in the wrong connector. Use a USB data cable in the connector on the top half of the board (the XDS110 debugger), straight into the computer.

### It says "FRL-E002 REFUSED: 2 probes on USB". Which one is my board?

Run `python3 tools/frl/frl.py detect`, unplug the other board, run it again: the serial that stays is this board. Pass it with `--probe`. The tool never picks one by order.

### It says "FRL-E004 ... could not be opened; another program (CCStudio, UniFlash or another OpenOCD) may hold it". What do I close?

CCStudio's or UniFlash's debug connection on this board, or another terminal still running OpenOCD. If the message names another frl run instead ("in use by ... run ..."), wait for it to finish. The tool never stops them itself.

### It says "FRL-E006 FAILED: OpenOCD could not attach to the board" with "DAP Connection Error. This could be caused by the device having gone to low power mode." What now?

The program on the chip sleeps right after starting. Hold S1 (PA18), press and release S3 (reset), keep S1 held 2 more seconds, then at once run `./start.sh` again, which flashes the blink program; unplug and replug USB afterwards. If there is no low-power line, check all nine J101 jumpers are on.

### It says "FRL-E021 REFUSED: sysconfig 1.28.0 installed; this route was validated with 1.26.3". Must I install another SysConfig?

Either install 1.26.3, or keep yours and name it: `export FRL_SYSCONFIG=/Applications/ti/sysconfig_1.28.0`. The run is then recorded as unvalidated. The same code with "has no target/ti_mspm0.cfg" means the OpenOCD found is too old (Homebrew's 0.12.0): install xPack OpenOCD 0.12.0-7.

### It says "FRL-E070 REFUSED: the project folder ... has a space in its path". Why can't it build there?

TI's example makefile passes generated option files unquoted, so a space splits the path (`tiarmclang: error: no such file or directory: '@é/ticlang/device.opt'` for a folder named `my blink é`). Set `FRL_PROJECTS` to a folder with no spaces. The same code also refuses a pattern outside the limits: each time 50 to 5,000 ms, at most 8 on/off pairs, one cycle at most 6 s.

### It says "FRL-E040 FAILED: LED2 red (PA26) does not follow the asked pattern". The build and flash passed. What is on the board?

Suspected: another program than the one just built, for example because the flash step did not run or a different board was flashed. The message says what the LED did. Run the same `./start.sh` command again; it rebuilds, flashes and checks. TI's unchanged blink shows "on 498, off 497" here.

### `./start.sh mspm0l1306 print-a-message` says "FRL-E040 FAILED: nothing arrived on /dev/cu.usbmodem...1". Why is the port silent?

J16 and J17 are probably on BP (left), which sends the UART to the header pins instead of the USB port. Move both to XDS (right, as shipped) and run the command again. Another program holding the serial port (a terminal, CCStudio's serial view) can also take the bytes first.

### Can Codex, the ChatGPT app or Claude Code flash my LP-MSPM0L1306 for me?

Yes, from the computer the board is plugged into, through the same local tool. The Codex command-line tool that comes inside the ChatGPT app backed up, built, flashed and checked the board in 56 s; Claude Code did the same through `./start.sh --assistant claude` in 92 s, and the check passed both times. Nobody has done it from an app's own chat window; docs/frl-desktop-apps.md has the setup.

### Does the route work on Linux?

Building does: in an Ubuntu 24.04 x86_64 container, emulated on an Apple Silicon Mac, with TI's Linux installers for TI Arm Clang 5.1.1.LTS and SysConfig 1.28.1 (no login) and the MSPM0 SDK copied from a Mac (its download needs a myTI login), `frl build` made the image in 3.9 s, and that image passed the check on this board. SysConfig 1.28.1 is not the tested 1.26.3, so set `FRL_SYSCONFIG` and the run is recorded as unvalidated. Flashing from Linux is untested (no board on that computer).

### Do I have to pass `--probe` with only one board plugged in?

No: with exactly one XDS110 on USB the tool uses it. With two or more it stops (`FRL-E002`) until you name one. The single-board case passed this repo's tests with simulated hardware; every run on the real bench had several boards and named the probe.

### `print-a-message` refuses my text with "FRL-E070 REFUSED: use plain letters, digits, spaces and punctuation, without quotes or backslashes". What may the message contain?

1 to 40 plain characters: letters, digits, spaces and punctuation, but no quotes or backslashes, because the text goes into a C string in TI's example.

### On Windows, `print-a-message` ends "not verified". Did it work?

Maybe: the tool does not look for the serial port on Windows (`FRL-E042 ... could not find the serial port ... on this OS (untested here)`). Open the board's first USB serial port at 115200 baud in a terminal program and look for your message.

### How do I share what went wrong without sharing my computer's details?

Run `python3 tools/frl/frl.py status --report`. It prints the last run's summary with your home folder, user and computer name and IP addresses removed, and the probe serial shortened to its last two characters. Read it before you paste it anywhere; the tool sends nothing.

### A desktop app's `frl_verify` or `frl_flash` call fails asking for `probe`. Why, with one board plugged in?

The tools that read or write a board through an app (`frl_backup`, `frl_verify`, `frl_verify_image`, `frl_flash`, `frl_restore`) always need the probe serial named, so an app never picks a board on its own. Call `frl_detect` first and pass the serial it lists.

### How do I put TI's out-of-box demo back after using `./start.sh`?

`python3 tools/frl/frl.py restore "<first backup folder>" --probe <serial>`. The first backup holds the demo the board shipped with; later runs back up whatever was on the board then. Restore writes main flash, reads it back and compares; it ran five times on this bench, each with a match.

### Does the route work on an Apple Silicon Mac?

Building does: on an Apple Silicon Mac (macOS 26.3) with TI Arm Clang 5.1.1.LTS for arm64, SysConfig 1.28.1 (it runs under Rosetta) and xPack OpenOCD for arm64, `frl build` made the image in 14.7 s, and that image passed the check on this board. Flashing from an Apple Silicon Mac is untested, and SysConfig 1.28.1 is not the tested 1.26.3, so set `FRL_SYSCONFIG` and the run is recorded as unvalidated.

### Codex says "MCP tool call requires approval, but approval policy is never". Is the tool broken?

No. Codex refuses tools that write the board when it cannot ask you. Run it interactively, or approve the one tool for that run (`-c 'mcp_servers.frl.tools.frl_flash.approval_mode="approve"'`). If Codex sits at "Reading additional input from stdin...", start it with `< /dev/null`. Details: docs/frl-desktop-apps.md.

### A desktop app's `frl_host` reports a Linux VM or 0 probes. Can it reach the board?

No: that app runs the tool somewhere without your USB devices (a cloud run or a VM). Run `./start.sh` in a terminal on the computer with the board, and use the app to read and explain.

### It says "FRL-E020 REFUSED: ... missing: mspm0_sdk not found; sysconfig not found; ...". What do I install?

Run `python3 tools/frl/frl.py doctor --board mspm0l1306`: it lists every missing tool with its download link and size (MSPM0 SDK 2.11.00.07, SysConfig 1.26.3, TI Arm Clang 5.1.1.LTS, xPack OpenOCD 0.12.0-7). Install them, or point to copies elsewhere with `FRL_MSPM0_SDK`, `FRL_SYSCONFIG`, `FRL_TICLANG` or `FRL_OPENOCD`. Have about 12 GB free if the compiler comes with CCStudio.

### macOS refuses to open the xPack OpenOCD because it was downloaded from the internet. How do I run it?

Clear the download flag once: `xattr -dr com.apple.quarantine ~/xpack-openocd-0.12.0-7`. The exact wording of macOS's warning differs between versions; it did not appear on the Mac used here, where OpenOCD came with the Zephyr toolchain.

### Claude Desktop cannot connect to the Fox Red Labs extension; its log says "Couldn't start for Cowork and Code sessions. Error: Connection closed". What do I do?

Open the extension's log, `~/Library/Logs/Claude/mcp-server-Fox Red Labs host check.log` on a Mac (on Windows: not run): the lines under that error say why the server stopped. Check that `python3 --version` prints 3.9 or later, then remove the extension and install the newest `.mcpb` (`python3 tools/frl/cowork/build_mcpb.py <folder>` builds it). The current build asks for no folder and should start. If it still fails this way, include the log's last lines in a report. Details: docs/frl-desktop-apps.md.

### `make` or `python3` is not found on my Mac. What do I install?

Apple's command line tools: `xcode-select --install`. They bring `make` and Python 3.9, which the route needs.

### A desktop app lists no frl tools. What is wrong?

The app cannot start the server. Its log says why: on a Mac, `~/Library/Logs/Claude/mcp-server-<extension name>.log` for Claude Desktop. Check `python3 --version` prints 3.9 or later, and reinstall the newest extension; the current ones carry the tool and ask no questions.

### The assistant says it is done, but the check failed. Which do I believe?

The check. It read the board; the assistant's words did not. The message says what the LED did, and `python3 tools/frl/frl.py status --handoff` gives a summary to continue from.

### Can Claude's Cowork reach my board?

Anthropic's Cowork architecture page says local MCP servers do not run in cloud runs. Install the read-only `frl-hostcheck.mcpb` first (`python3 tools/frl/cowork/build_mcpb.py <folder>` builds it) and ask a new Cowork run to call `frl_host`; if it reports this computer and your probe count, the board tools can work there.

## Connecting and flashing

### OpenOCD says "Could not find MEM-AP to control the core" and DSLite says "Peripheral mis-configuration (e.g improper watchdog or clock)". The chip resets itself in a loop. How do I get back in?

Mass-erase it through the debug mailbox, which works while the core is unreachable. Cause: a window-watchdog example that refreshed the watchdog inside the closed window, and the chip reset every few milliseconds from the first boot. The command that recovered it, with xPack OpenOCD 0.12.0-7: `openocd -f interface/xds110.cfg -c "adapter serial ML130001" -f target/ti_mspm0.cfg -c "mspm0x.cpu configure -defer-examine" -c init -c mspm0_mass_erase -c shutdown`. The script then throws `wrong # args: should be "error message ?stacktrace?"` from its reset helper; the erase has already happened and the next DSLite load works. Without a terminal: hold S1 while pressing S3 to enter the bootloader. The port that hit it is `ports/cms32/README.md`.

### Will a low-power example knock the LP-MSPM0L1306 off the debugger, as it did the MSPM0G3507?

Not in the runs made here. All 13 of TI's low-power, watchdog and flash-protection examples for this board (12 STOP, STANDBY, SHUTDOWN and watchdog examples plus `flashctl_dynamic_memory_protection`), and 9 more whose SysConfig selects STOP or STANDBY, were flashed and run by a script, and the debugger reconnected every time. On the LP-MSPM0G3507 the same script lost the board twice and needed a USB replug. Each run started with a system reset that keeps the debugger attached, which may stop the chip from fully entering the deep modes; no meter was available to check. If you lose the board, unplug and replug its USB cable.

### What is the quickest way to flash the LP-MSPM0L1306 from a Mac terminal?

OpenOCD with its MSPM0 script, choosing the probe by serial number: a 440-byte blink took 0.6 seconds to program and verify. No TI command-line tool writes the connection file its own flasher needs. TI's own command-line flasher (UniFlash's `dslite.sh`) also works but took 15.0 seconds for the same image, and it needs a connection file (`.ccxml`) that no TI command-line tool writes; the file used here is the G3507 file with two names changed (`MSPM0L1306_ML130001.ccxml`). The OpenOCD wrapper is `tools/ocd.sh`.

### OpenOCD prints "Error: checksum mismatch - attempting binary compare". Did the flash fail?

No. The next line, `** Verified OK **`, is the real result. OpenOCD's quick checksum check fails on every MSPM0 image flashed here, on this board and the LP-MSPM0G3507, and it falls back to reading the image back, which passes.

### OpenOCD says "Unable to reset target", then every connection fails with "Could not find MEM-AP to control the core". Is the chip dead?

No. With main flash blank, the chip's boot ROM keeps the debugger off the CPU after a reset. Connect, run TI's `mspm0_mass_erase` (it halts the CPU), then `flash write_image erase <file> 0x0 bin` with no reset, then `mspm0_board_reset`. Cause: on an MSPM0G3507, all of main flash was erased before OpenOCD's `program`, which resets first; `program` erases what it needs on its own. `mspm0_mass_erase` erases main flash only, not the boot configuration.

### Why does my program restart every time I connect the debugger?

OpenOCD's `init` resets the MSPM0 target, so each new connection starts the program again; the serial port shows the boot text twice. Read results in the same OpenOCD connection that started the program, or leave the program alone and use the serial port.

### The board came with a program on it. What is it, and can I get it back?

It is TI's out-of-box demo (`examples/nortos/LP_MSPM0L1306/demos/out_of_box`), built with an older toolchain than today's SDK: 9.5 kB, against 9.6 kB from SDK 2.11. Save it before you flash anything: `tools/ocd.sh "dump_image flash-as-received.bin 0x0 0x10000"`. The copy from this board is in `data/out-of-box-as-received.bin` and restores with `tools/ocd.sh "reset halt" "program data/out-of-box-as-received.bin 0x0 verify" "reset run"`.

### The out-of-box demo is silent on the serial port. Is it broken?

No. It answers commands and sends nothing until asked. Send a MessagePack map ending in a newline, at 115200 baud, for example the bytes `81 AB 6C 69 67 68 74 45 6E 61 62 6C 65 C3 0A` (`{"lightEnable": true}`). It then streams `lsADC` light readings; `{"thermEnable": true}` streams `thADC`, the board temperature in degrees Celsius times 32. The README points to a GUI and says "Please contact your local sales representative or TI support team to request access to the GUI." The message format is only in the source (`MSP_GUI/GUI_mpack.c`). Without the newline the demo ignores the message.

## Memory

### Why does my program print garbage at the end, or fault, on a 4 kB chip?

Check the stack size. TI's examples for this chip reserve 256 bytes of stack, and `printf("%f")` alone needs more than 400. The CoreMark build passed its own checks while its final `printf` ran 148 bytes past a 2.8 kB stack and overwrote the end of the result text, with no fault. The chip has no memory protection unit, so nothing stops an overflow. Fill RAM with a pattern before running and look for the lowest overwritten word (`tools/paintrun.sh`), and set the stack in the linker file (`--stack_size`).

### Embench's wikisort builds for this chip and then never finishes. Why?

Its main function reserves 4.1 kB of stack in one instruction, more than the chip's 4 kB of RAM. That is the benchmark as written, at -O2 and -Os. The stack pointer lands below RAM (0x1FFFFEE0) and the first write faults into the default handler. The linker cannot see this; only a run does. `third-party/README.md`.

### Which of the room-occupancy neural nets fit?

The 500, 2k and 8k parameter models run, 20 of 20 test vectors each, the 8k one in 84 to 100 ms at 32 MHz. The 30k model does not link: `program will not fit into available memory` for `.bss`. Its buffers alone need more than 4 kB as generated; reusing buffers between layers was not run.

## Analog and sensors

### Why does the internal temperature sensor read about -165 °C?

The factory calibration word on the chip tested (`TEMP_SENSE0` at 0x41C4003C, 369) does not match its sensor. With the typical value from TI's manual instead, the same reading gives about 21.7 °C, and the board's thermistor read 22.8 °C. A public TI forum thread (E2E 1311550) reports the same impossible result on LP-MSPM0L1306 boards, and TI's answer there is that early samples carry a wrong temperature trim. Whether this chip is an early sample is not confirmed. Use the onboard thermistor (PA15, ADC channel 9) or calibrate against a known temperature.

### Is there an internal temperature sensor example for the L1306?

No. Only the G-series has one (`adc12_internal_temp_sensor_mathacl`), and it uses the G-series math accelerator. It ports with four SysConfig changes and no C changes: device and package lines, board line, `genLibIQVersion` from `MATHACL` to `RTS`, and remove `SYSCTL.forceDefaultClkConfig`.

### Why does TI's "max frequency" ADC example reach 1.45 million samples per second when the data sheet says 1.68?

Its 8-cycle sample window sets the rate. With 4 cycles it does 1.77 million and with 2 cycles 1.99 million, but on the board's thermistor every shorter window reads lower: 1.7% low at TI's 8 cycles, 5.3% at 4, 9.9% at 2, against a 64-cycle reading. The input must settle onto the sampling capacitor; the sensor's source impedance was not measured and no buffered input was tried. Use a longer window for sensors, or buffer the input with an op-amp..

### I changed the clock in SysConfig and nothing changed. Why?

If the file has `SYSCTL.clockTreeEn = true`, the clock tree setting wins: `system.clockTree["SYSOSC"].frequencySelect = 4` sets 4 MHz, and `SYSCTL.SYSOSC_Freq = 4000000` alone is ignored with no warning. After any clock change, read `CPUCLK_FREQ` in the generated `ti_msp_dl_config.h`.

### Why does my UART return garbage at 5 to 6 Mbaud on the LP-MSPM0L1306?

The UART fails between 5 and 6 Mbaud at 32 MHz with TI's DriverLib baud setup and works on both sides of that band; other clocks were not tested. In a loop test (PA8 wired to PA9, 2 kB of random bytes per speed, 2026-10-06) every byte came back right from 9,600 baud to 4.5 Mbaud and from 6.4 to 10.67 Mbaud; 5 Mbaud got 38 right, 5.33 Mbaud 1 to 5, 5.5 Mbaud 178, 6 Mbaud 94%, with framing, break and overrun flags (`RXDATA` bits `0x100`, `0x400`, `0x800`). The LP-MSPM0G3507 gives nearly the same counts. At 12 Mbaud DriverLib sets a divisor of 0 and nothing is sent, with no error. Neither chip's errata (SLAZ742H, SLAZ741D) lists it, and the UART block was not separated from DriverLib's setup. Stay at or below 4.5 Mbaud, or test your exact speed first. Detail: `data/results/mspm0l1306.csv`.

### I wired PA8 to PA9 for the UART loop and nothing comes back. Why?

Move jumpers J16 and J17 to the BP side (left) first, then wire J1 pin 4 (PA8) to J1 pin 3 (PA9). Out of the box those two jumpers route the chip's UART to the debugger's USB serial port, so a wire on the header connects to nothing; TI's loopback example README does not mention them. Move them back to XDS (right) afterwards or the board's serial window stays silent.

## Zephyr

### Does Zephyr support the LP-MSPM0L1306?

Zephyr 4.4.2 has the chip but no board for this LaunchPad. The out-of-tree board in this repo (`zephyr/boards/ti/lp_mspm0l1306/`) runs hello_world and blinky. Zephyr's own GPIO driver does not compile for this chip and there is no pin-mux file for the MSPM0L130x family; both are worked around in the board folder. `zephyr/README.md`.

### Zephyr's blinky fails with "error: 'IOMUX_PINCM38' undeclared here (not in a function)". Why?

Zephyr 4.4.2's MSPM0 GPIO driver has the pin table of the 80-pin MSPM0L222x, and this 32-pin chip has no `IOMUX_PINCM38` to `PINCM74`. Use the corrected copy of the driver in this repo's board folder (`zephyr/boards/ti/lp_mspm0l1306/`), which keeps only the pins this chip has.

### Every Zephyr program that fills RAM stops at boot with "***** HARD FAULT *****". Why?

Set `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=0` if the program does not call `malloc`. Zephyr's C library heap defaults to all RAM left after the program. With 8 to 16 bytes left, setting up the heap writes past the end of RAM: the fault address was in `sys_heap_init`, called from `malloc_prepare`. Nothing warns at build time.

### My interrupt test works on the MSPM0G3507 but the handler never runs on the L1306. Why?

Use an interrupt line that has a peripheral on this chip. The L1306 implements only lines 0, 1, 2, 4, 9, 13, 15, 16, 18, 20, 24, 25 and 31 (the enable register reads back `0x8315a217` after writing all ones). Pending any other line does nothing, and neither CMSIS nor Zephyr's `IRQ_CONNECT` complains. The test had borrowed line 28, which is AES on the G3507; line 25 (I2C1, unused) works.

### Why does a Zephyr "-O3" build run at -O2?

Put the optimisation flag on the file itself with a per-file compile option. Zephyr adds its own `-O2` after the application's flags, and GCC uses the last one, so a project-wide `-O3` is overridden (and loop unrolling stays on). `zephyr/README.md` shows the per-file option.

### Why does a Zephyr sample fail with "region `RAM' overflowed"?

As shipped, most do: synchronization needs 6.1 kB, logger 8 kB, shell_module 12.5 kB and philosophers 17 kB, mostly in stacks (Zephyr's interrupt stack alone defaults to 2 kB). That is Zephyr's sample defaults, not the chip: with stacks and buffers cut, the shell (minimal setting, 48 bytes left), the logger in both modes, synchronization and all eight Thread-Metric tests ran on the board. philosophers was not run. The exact settings are in `zephyr/extra/README.md`.

## Building and moving code

### Do Cmsemicon's CMS32F04x example programs run on the LP-MSPM0L1306?

34 of the 53 variants in Cmsemicon's Keil pack build and ran on this board on 2026-10-07, 29 with their `main.c` unchanged, through a glue layer of 845 lines of C that implements Cmsemicon's function names on DriverLib. Of the 19 not built, 7 need CAN, a segment LCD, USB fast charge or a clock monitor the L1306 does not have. The other 12 are I2C and SPI examples that need a second device on the bench. Two more things to know: the temperature-sensor example prints a wrong temperature because the glue's trim constants are placeholders, and the flash example's 512-byte pages land on the L1306's 1 kB sectors, so two of its erases wipe a neighbour it just wrote (both, detail on the port page). Detail and the per-example record: `ports/cms32/README.md`.

### My ported example's LED never blinks, but the same code blinks at -O0. Why?

The example polls a plain global (`extern uint32_t g_ticks`) that an interrupt decrements, without `volatile`, so at -O2 the compiler turns the wait into an endless branch. Cmsemicon's Keil projects build at optimization level 0, which hides it. Build that file at -O0, or add `volatile`. Seen with TI Arm Clang 5.1.1 on 2026-10-07, `ports/cms32/README.md`.

### Some SDK examples for the L1306 will not build from the terminal. Which, and why?

14 of 170. 9 have no makefile (CCStudio project only), 2 have a path from TI's build server in their makefile (`/home/developer/.cache/bazel/...`: `dali_led_demo`, `drv8329aevm_gui_firmware`), and the three `bsl_software_invoke_app_demo_*` stop at link with `symbol "interruptVectors" redefined`. None fails to compile.

### I moved an MSPM0C1104 example to the L1306 by changing the device in SysConfig. It runs, but the LED does not blink. Why?

It is blinking a pin with no LED. Unlock the pin assignments, or set the LED pin by hand (PA0 red, PA26, PA13 and PA27 for the RGB LED). Changing the device keeps the old board's pins: the ported `gpio_toggle_output` toggled PA22 and PA2, the C1104 kit's LED and test pins, and on this board PA22 is the light sensor's amplifier output. SysConfig gives no warning. With the assignments removed it picked PA0, the red LED.

### Does FreeRTOS run on the LP-MSPM0L1306? TI's settings file will not link

Yes, with static allocation and smaller stacks: a one-task blink uses 1 kB of RAM, 11 small tasks fit, and a task switch takes 12 to 16 microseconds. TI ships no FreeRTOS example for this chip, and its `kernel/freertos/builds/LP_MSPM0L1306/release/FreeRTOSConfig.h` sets a 3 kB heap and 512-byte task stacks, which needs 5.4 kB of zero-filled RAM on a 4 kB chip. Settings and build: `freertos/README.md`.

### Why is there no Trace or EnergyTrace item in CCStudio 21 on my Mac? Can I measure current on the LP-MSPM0L1306 from a Mac?

Not in CCStudio 21.0.1 as installed. Two functions in the IDE's own code return "not supported" on macOS (the comment says "not supported on Mac OS, only windows and linux"), so the Run, Trace menu is hidden. Behind that switch, TI's Mac trace server library fails to load with `Library not loaded: @rpath/libti_logger.dylib` (it ships with a build-server path and a Linux-style search path) and the capture library, loaded on its own, with `Library not loaded: @rpath/libxdsfast3.dylib`. **Unknown (suspected: Tool or docs).** It cannot see the probe driver folder by itself; the debugger may load it differently. With the switch removed and three library paths corrected in copies placed in a mirror of the install tree (the install itself was not changed), the three libraries tested (`libCloudTrace`, `libenergytraceccs`, `libTraceFactory`) load; the other 72 in that folder were not tested. The IDE path then dies anyway: the Mac DSLite answers `Unknown sub module: "trace"`. So the capture runs without the IDE: `tools/energytrace/etcap.py` talks to the probe through CCStudio's XDS110 driver library and gives the numbers in `data/results/mspm0l1306.csv`. Apple silicon was not tested (the files checked are Intel builds). TI's command-line capture tool `stune` is documented for Windows only and is not in the Mac install. Two more libraries in `ccs_base/common/bin` carry the same Linux-style search path, and the five files checked (the three patched libraries, DSLite and the app) are not code-signed, so the patch is expected not to trip macOS library validation. TI's answer on E2E is CCStudio 12.8, whose last supported macOS is Ventura (TI staff, E2E thread 1579854), and TI's tracker entry EXT_EP-12168 (open since 2025-01) gives "Use CCS 12.x" as the workaround. To measure today: `python3 tools/energytrace/etcap.py --serial ML130001 --seconds 20 --csv out.csv`, or a meter across the J101 3V3 jumper (user's guide section 2.6). Detail and the patch: `docs/experiments/2026-10-06-mac-energytrace-ccstudio.md`.

### My sleep-current reading never drops below about 2.6 mA, whatever power mode I set. Why?

A debugger has been attached since the last power-on reset. The debug subsystem stays enabled until a power-on reset, and while it is enabled the chip stays in SLEEP0 whatever policy the code sets. Make the program check the reset cause on boot and trigger one software power-on reset (the deep-sleep test image does), or unplug and replug the board. After that OpenOCD cannot reach the sleeping chip ("Debug AP not available"); DSLite can: `DSLite flash -c mspm0l1306/MSPM0L1306_ML130001.ccxml -f -v <file.out>`. Numbers:

### After flashing a SHUTDOWN test, nothing can connect: "DAP Connection Error. This could be caused by the device having gone to low power mode." Is the board bricked?

No, but it is locked out. A program that enters SHUTDOWN right after boot runs back into it on every reset, before any debugger can attach. OpenOCD's mailbox mass erase did not reach it either (OpenOCD exits as soon as the debug port fails to answer). What worked: hold S1 (the bootloader button, PA18) while pressing and releasing S3 (reset), keep S1 held two more seconds; the chip sits in TI's bootloader with its debug port alive, and OpenOCD attaches within a second. After that rescue the chip kept starting in the ROM bootloader at every debugger reset until a plain USB power cycle (seen on a later use of the board); unplug and replug once the new image is on. Flash any non-sleeping image at once. Prevention: every deep-sleep image waits 3 s at boot.

### EnergyTrace reads 1.5 mA with the chip in SHUTDOWN, where the data sheet says 61 nA. Is the tool wrong?

No. A pin is. The test image drives PA1 low, and the LaunchPad pulls PA1 up to 3.3 V through 2,200 Ω when jumper J10 is in, so the pin sinks 1.31 mA whatever the chip is doing. The user guide SLAU869E says it in the jumper table: "J10: OFF if pin initialized as output low or input with pullup or pulldown" (the same for J9 and PA0). With PA1 high the same STANDBY0 image reads 0.198 mA, which is the thermistor divider and the light-sensor circuit, not the chip. Before any power run on this board: read table 2-1 of the user guide, pull J9 and J10 or keep PA0 and PA1 high, and expect about 0.2 mA of board circuits on top of the chip. The tool's scale was never in doubt: CoreMark at 32 MHz and 4 MHz differ by 72.9 µA per MHz against the data sheet's 71, and SLEEP0 minus SHUTDOWN is 940 µA against 978. Numbers:

### Does TI's CMSIS-DSP library run on the L1306, and what fits?

Yes. The SDK's prebuilt Cortex-M0 library ran FFTs, FIR and biquad filters with the same errors as the C code on a Mac: a 256-point q15 FFT takes 2.25 ms at 32 MHz, a 512-point one 5.2 ms. The largest transforms that fit 4 kB of RAM are 512 points in q15 and 256 in q31; a 512-point q31 FFT's buffer alone is 4 kB. A 256-point FFT in 32-bit float also runs, in software because the chip has no FPU: 25.7 ms, 11 times the q15 time.

### Why does the L1306 score 0.73 DMIPS per MHz when tables often say about 1?

Those tables usually let the compiler inline across Dhrystone's procedures, which the benchmark's rules forbid; with that on, this chip reads 1.04. And at 32 MHz the flash adds a wait state, which costs Dhrystone 17% (0.88 DMIPS per MHz at 4 MHz). The number depends on the rules and clock you pick.

### Embench's st benchmark reports FAIL. Is the chip's maths wrong?

No. It fails its own check at both optimization levels on this chip and on the MSPM0G3507; on the G3507 the difference was traced to one or two steps of the last bit of a double, finer than Embench's 1e-13 tolerance. Not re-traced on the L1306.

### Can I take code written for the MSPM0G3507 and run it on the L1306?

Plain C, and code that only uses DriverLib calls both chips have, moves with SysConfig changes only: CoreMark, the occupancy models and TI's temperature example all did. What does not move is anything that needs a block the L1306 lacks (math accelerator, AES, TRNG, 12-bit DAC, CAN, port B pins) or more than 4 kB of RAM. Counts for TI's own examples:

## DriverLib, TI Drivers and Zephyr

These come from building one small app (ADC reading, serial line, LED) three ways on this board and the LP-MSPM0G3507: `portability/three-ways/README.md`. MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, Zephyr 4.4.2.

### My TI Drivers program on the L1306 sends binary at 115200 instead of text. Why?

Not found; use DriverLib or Zephyr for serial output on this chip until it is. The image runs and keeps sending: runs of zero bytes and a repeating 32-bit word 0x0000136f at 115200, and nothing readable at six other rates from 9600 to 460800 (observed, not saved). The suspect is the hand-ported configuration, since TI ships no TI Drivers example for this chip. Ruled out: 1-byte against 32-byte UART ring buffers (no change), a missing `UART_init()` (this SDK has no such function), and the clock (a 32 MHz bus clock, confirmed by the working DriverLib build).

### TI Drivers on FreeRTOS will not link on the L1306: "run placement with alignment fails for section ".bss" size 0x1458". Why?

TI's FreeRTOS settings file for this board does not fit its 4 kB of RAM. The linker needs 0x1458 (5.1 kB) of `.bss` against 0x1000 (4 kB) of RAM, mostly the 3 kB heap that TI's `FreeRTOSConfig.h` for the LP-MSPM0L1306 asks for. FreeRTOS alone fits with static allocation (`freertos/README.md`); that was not run with TI Drivers.

### Does Zephyr have an ADC driver for the MSPM0?

Not in Zephyr 4.4.2: its `drivers/adc` folder has no MSPM0 file. Call DriverLib from your Zephyr app for the ADC and add `dl_adc12.c` and `dl_vref.c` to `CMakeLists.txt` yourself, because Zephyr's MSPM0 Kconfig only switches on the GPIO, UART, timer and CAN parts of DriverLib. That glue is the part you rewrite for another vendor's chip.

### Zephyr's DriverLib headers differ from the MSPM0 SDK's. Does that matter?

Not for this app, as far as is known. In Zephyr's copy of TI's DriverLib, `dl_adc12.h` and `dl_vref.h` differ from MSPM0 SDK 2.11.00.07 by 143 diff lines (93 and 50, counted as the lines `diff` marks on either side); the `dl_adc12.c` and `dl_vref.c` files are identical. Inference, not checked: the differences are version drift.

### Why is MSPM0 TI Drivers configuration hand-written C?

Because MSPM0 TI Drivers have no SysConfig module: write the configuration tables in C, starting from a TI example's `ti_drivers_config.c`. No MSPM0 TI Drivers example has a `.syscfg` file; checked by listing the 40 TI Drivers example folders in SDK 2.11.00.07 (`examples/rtos/<board>/drivers/`, 10 examples on each of four LaunchPad kits). For the same small app the configuration is 136 lines of C against 35 lines of SysConfig for DriverLib on the LP-MSPM0G3507, and 130 against 34 on this board. `portability/three-ways/README.md`, problem 3.

### Is there a TI Drivers example for the LP-MSPM0L1306?

No. SDK 2.11.00.07 ships the driver library for this chip (`drivers_mspm0l11xx_l13xx.a`), the no-RTOS driver porting layer and a FreeRTOS settings file, but no example and no board configuration that uses them. The C configuration tables were ported from TI's G3507 `adc_singlechannel` and `uart_echo` examples (130 code lines), and the startup and linker files come from the SDK's device folder.

### A TI Drivers GPIO table entry without a pin-mux index sets up PA0. Is that expected?

Give every entry its pin-mux index, or mark it `GPIO_DO_NOT_CONFIG`. Read from the source, not run: `GPIO_init` takes the pin-mux index from each entry's low byte (`pincm = pinConfig & 0xFF`), and 0 is PA0's pin control register, which drives LED1 on this board. TI's G3507 `adc_singlechannel` table has four entries without one.

### TI Drivers' ADC driver does not turn on the internal reference. Do I have to?

Yes, as far as the source shows: turn the reference on with DriverLib in your configuration. `ADCMSPM0.c` sets the reference choice in the ADC but never touches the VREF block. Not checked on a board, because no TI Drivers image here has printed a reading.

### The CC1407P host build prints its banner and the first SPI frame on the LP-MSPM0L1306, then nothing. Is the board hung?

With no radio on SPI0 (PA6, PA5, PA4, PA3, PA7, PA2) that is the expected stop: the driver waits for the radio in the SPI-complete handler (halted in `hb_delay_us`, 3 s after reset). Wire the BP-CC1407P-9-FE as in `mspm0l1306/radio-host/README.md`; the first frame `00 04 11 00 02 32 00 00 e9 71` is the same ten bytes the LP-MSPM0G5187 sent in its recorded link. Build with the pool cut to 4 buffers (`NBUF=4`, the default there): TI's 24 need 5,244 B of the chip's 4,096 B. Run 2026-10-24, no radio attached, so the link itself is untested here.

## Setting up CCStudio and the AI on a Mac

These come from setting up CCStudio 21.0.1, the MSPM0 SDK 2.11.00.07 and Claude Code inside CCStudio on the same Intel Mac (macOS 15.7.9) on 2026-10-04, before the board was plugged in. They are the setup steps of the [Blink project's Start here box](../site/project-blink.html#start).

### How much free disk do I need for CCStudio and the MSPM0 SDK?

Have 12 GB free before you start, and delete each installer file once it has finished. The full CCStudio install with every chip family is 8.1 GB, more than the 6.0 GB TI's page recommends, and the SDK adds 2.2 GB; together 10.3 GB once installed, more while an installer file sits beside its install. A Mac that started with 13 GB free dropped to 2.5 GB after CCStudio with its installer still on disk. Measured sizes: `mac-setup/README.md`, "Disk space you need".

### I clicked the MSPM0 SDK download and nothing happened in CCStudio. Where is it?

Look in your browser's downloads folder. The SDK file (`mspm0_sdk_2_11_00_07.app.zip`, 0.4 GB) downloads in the browser from ti.com/tool/MSPM0-SDK, not inside CCStudio; unzip it and run the installer. CCStudio's first screen gives no hint that an SDK is needed, so it is easy to look for the download inside CCStudio. The padlock beside each file on TI's page marks a short export approval form, about a minute.

### CCStudio lists no SDK after I installed the MSPM0 SDK and turned on the AI. What do I do?

Quit CCStudio and open it again. On CCStudio 21.0.1 it listed no SDK until it was restarted; after the restart it listed all six installed SDKs, the MSPM0 SDK among them. Observed 2026-10-04.

### Should I pick Global or Workspace for Scope in CCStudio's AI panel?

Pick Workspace. CCStudio's AI panel opens on Global, and the panel itself does not say which file each choice changes. Global writes into `~/.claude/settings.json`, the settings file every other Claude Code run on the Mac uses; Workspace keeps the change inside the CCStudio workspace folder.

### Several Claude Code chat panels opened in CCStudio. Is something wrong?

No. Close the extras; one chat is enough. Extra panels can open during setup; the cause was not found. The chat answered, and CCStudio's four tool servers for the AI (project, debug, SysConfig, serial) all responded afterwards.

### Does the LP-MSPM0L1306 come with a USB cable, and will it fit my Mac?

TI's user guide lists one micro-USB cable in the box (SLAU869E, section 1.3.1); the plug type on the computer end is not stated. If it does not fit a USB-C-only Mac, use a USB-C to micro-B cable or an adapter. An LP-MSPM0G3507 came with a cable that had a USB-A plug on the computer end.

### Does this work on an Apple Silicon Mac?

Not tested. Every time and size on this page and in the Blink Start here box comes from one Intel MacBook (Core i7-8750H) on macOS 15.7.9. TI's download page offers CCStudio and the MSPM0 SDK for macOS; neither was run on an Apple Silicon Mac.

## Benchmark programs

### Why does the PID power loop on the LP-MSPM0L1306 miss periods at 100 kHz in Q15 when it is clean at 10 kHz?

Run the Q15 loop at 92 kHz or below: a rate sweep on the board found 92.75 kHz the highest rate with no missed period in 1,000, and at 50 kHz about 57 percent of the CPU is left for other work. Only code in flash at -O2 was tried. The lead: each interrupt takes 273.5 cycles of CPU with entry and return, and the ADC takes about 65 cycles from trigger to interrupt, about 338 together, close to the 345-count period the sweep found. At 32 MHz, the chip's top clock, a 100 kHz period is 320 cycles, and the loop's worst time from trigger to the end of the interrupt was 313. In the 100 kHz pass 6 of 1,000 interrupts ran past the end of their period, and one ADC result was overwritten before it was read. Interrupt entry took up to 152 cycles in that pass against at most 59 in the 10 kHz pass, which suggests some interrupts started late; why was not found. The 10 kHz pass had no such period and the same 228-cycle mean. MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1 -O2, code in flash; rows in `data/results/mspm0l1306.csv`.

### How fast can the LP-MSPM0L1306 run the C2000's FOC motor-control loop?

1.83 kHz in software float and 16.94 kHz with TI's IQmath fixed-point library, each the highest rate with no missed period in 1,000, found by a rate sweep on the board. The Cortex-M0+ has no floating-point unit and this chip has no math accelerator, so every sine and cosine is software; a sine lookup table was not run. The float run at a 50 kHz trigger stopped at the program's time limit after 80 of 1,000 interrupts, so its worst case, 13k cycles from trigger to PWM value (4,131.2 percent of a 100 kHz period), rests on those 80, where the angle stayed under 0.8 radians. The older 1,000-interrupt run at 1 kHz, same `foc.c`, reached 17k cycles (5,399.7 percent). For scale, the LAUNCHXL-F2800137 holds the same loop at 301.5 kHz from flash. TI Arm Clang 5.1.1 -O2, 32 MHz, code in flash; rows in `data/results/mspm0l1306.csv`.

### Should I run my control loop from RAM on the LP-MSPM0L1306 to skip the flash wait state?

Only if every routine the loop calls fits in RAM with it. The FOC step in TI's IQmath ran 2.6% faster from RAM (1,744.9 cycles against 1,700.2), but the float step ran 6.3% slower (12,701.1 against 13,497.3), because its library routines do not all fit the 4 kB SRAM (the map shows it 3.9 kB full, with the double-precision helpers left in flash) and each call between SRAM and flash goes through a trampoline of about 12 cycles. Only one stack size was built; a smaller stack might fit the helpers too. With the flash prefetch and cache on (the reset setting), code in flash costs only about 3% here. TI Arm Clang 5.1.1 -O2, 32 MHz, measured 2026-10-08.

### SYSCTL MCLKCFG on the LP-MSPM0L1306 reads FLASHWAIT 2 at 32 MHz. Does it run with two flash wait states?

No, it runs with one: ignore the `FLASHWAIT` field on this chip. The field only applies when the main clock comes from HSCLK, which the MSPM0L130x does not have; with the clock from SYSOSC the hardware picks the wait state itself, and the data sheet gives 0 up to 24 MHz and 1 up to 32 MHz. The register reads 0x00000200 after TI's SysConfig clock code; driverlib's `dl_sysctl_mspm0l11xx_l13xx.h` says "Flash wait states are managed automatically by SYSCTL when MCLK is running from SYSOSC or LFCLK". MSPM0 SDK 2.11.00.07.

### Is GCC as fast as TI Arm Clang on the LP-MSPM0L1306?

No: CoreMark at -O3 and 32 MHz scores 78.27 with TI Arm Clang 5.1.1 and 54.62 with GCC 12.2.0 (the Zephyr SDK's), bare metal, the same source files, both validated. The compiler and C library you pick move this score by 43% (TI Arm Clang with its own library against GCC with newlib-nano; the compiler alone was not separated); with `-funroll-loops` GCC reaches 58.65. The SDK's GCC startup file and linker script build it with no changes. Measured 2026-10-08; rows in `data/results/mspm0l1306.csv`.

### I read my benchmark's results from the LP-MSPM0L1306 with a second OpenOCD call after the run, and got all zeros. Did the run fail?

Probably not: read the results from the SRAM dump taken in the same OpenOCD connection as the run, not with a new connection. A second OpenOCD call restarts the chip as it connects, and the start-up code zeroes the results array (`fom`, at 0x20000800), so the read gives zeros. The same connection's dump (`sram.bin`) holds the finished array; read results from the dump. xPack OpenOCD 0.12.0+dev.

### The SPI speed test on the LP-MSPM0L1306 stops after its first two passes and never sets its done marker. Is the SPI broken?

No: the two CPU-fed passes at 16 MHz, the chip's maximum, completed and give 1,644 kB/s with 8-bit frames and 1,785 kB/s with 16-bit frames. The next pass is the first DMA-fed one; it and every later pass (the 8 MHz passes and the loopback check) read zero, and the done marker stayed 0 (`spi: magic 0x0 (NOT DONE)`). The same code runs to the end on the MSPM0G3507.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-11-08. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- An image enters SHUTDOWN right after boot: DAP Connection Error, low power mode (`mspm0l1306/FAQ.md`)
- A window-watchdog example resets the chip every few milliseconds: Could not find MEM-AP (`mspm0l1306/FAQ.md`, `ports/cms32/README.md`)
- A deep-sleep port with only the watchdog timer as wake source (ran, woke every 0.997 s) (`ports/msp430-to-mspm0/lpm3-wake/PORT.md`, 2026-10-21)
- TI timx_timer_mode_periodic_standby enters STANDBY: every debugger read answers ack 4, and a reset pulse with a halt does not catch it (`docs/experiments/2026-11-06-mspm0l1306-live-chip-view.md`, 2026-11-06)
- Found silent: OpenOCD could not attach (FRL-E006), frl identify saw no answer on SWD or JTAG (FRL-E010), an OpenOCD reset halt returned nothing; the same signature as the STANDBY lockup. Which image was on the board is not known (the last recorded run was the 2026-11-06 live chip view). Recovered the same day with the DSLite method below: the chip answered after the write; DSLite's own verify step fails with 'Memory map prevented reading 0x0000FFF8', so the proof is an OpenOCD read-back (backup.py save), which matched the written image (`mspm0l1306/FAQ.md`, 2026-11-08)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| Hold S1 (PA18), press and release S3 (reset), keep S1 held 2 s, then flash a non-sleeping image at once; replug USB afterwards (for the SHUTDOWN lockup; the chip then starts in the ROM bootloader until a plain power cycle) | hands | proven | `mspm0l1306/FAQ.md`, `mspm0l1306/AGENTS.md`, date not recorded |
| OpenOCD mailbox mass erase (mspm0_mass_erase) (for the watchdog reset loop) | remote | proven | `mspm0l1306/FAQ.md`, date not recorded |
| OpenOCD mailbox mass erase (for the SHUTDOWN lockup: OpenOCD exits when the debug port fails to answer) | remote | failed | `mspm0l1306/FAQ.md`, date not recorded |
| uhubctl power-off of the probe port (the tick counter restarted; the hub location is not recorded) | remote | proven | 2026-10-21 |
| DSLite flash from UniFlash 9.6.0 with a non-sleeping image and the board ccxml (for the STANDBY lockup; the first try worked, no hands; second success 2026-11-08 (the 2026-11-02 backup image written back; expect DSLite's verify to fail at 0xFFF8 and check with backup.py instead; the image path must contain no space)) | remote | proven | `docs/experiments/2026-11-06-mspm0l1306-live-chip-view.md`, 2026-11-08 |

USB hub ports recorded for this board:

- The probe's hub port does not cut power, 2026-10-27. SRAM marker test: a word written at 0x20000FF0 through the DAP (no CPU connect, no reset) survived a 15 s `uhubctl -a off`; the chip and the XDS110 kept power. The port does drop the data link: the probe leaves USB within 1 s and is back 1.0 s after `on` on every port of this hub (nine ports tried), and the first debugger connect after a cut answers Error -261, the second works.

<!-- recovery:end -->

## Watching the chip run (live chip view)

### Which browsers and boards does the chip view work on?

Chrome or Edge on a Mac, Windows or Linux computer, with an LP-MSPM0L1306, LP-MSPM0G3507, LP-MSPM0G3519 or LP-MSPM0G5187 plugged in over USB. The page reaches the board's XDS110 probe through WebHID, which Safari and Firefox do not have. Close CCStudio, UniFlash and OpenOCD first, because only one program can hold the probe. The view has been read live on the L1306, G3519 and G5187, with CPU load checked against a known load only on the L1306 (E317).

### Does the chip view halt or slow my program? Does it change a power measurement?

It does not halt, reset or write to the chip, and it costs the program no instructions, but do not measure power while it is attached. The reads go through the debug port beside the CPU. The debug connection can keep the chip out of its deepest sleep, so the current you measure is not the current of the program alone. A program that enters STANDBY drops the debug link; the page then says "Not answering". If every read answers "ack 4", flash a program that does not sleep with TI's DSLite from UniFlash 9.6 and the board's ccxml. E317

### The view shows addresses like 0x00000234 instead of function names. Where do I get the names?

Choose the program's .out file in the page. The chip does not store function names, only code, so the page can show the address the CPU is on. The .out is the build output that CCStudio or `make` writes in the project's Debug folder. The page reads it in the browser to turn addresses into names.

### The page says "This page has no map for this chip yet". Which chips are covered?

Four: the MSPM0L1306, MSPM0G3507, MSPM0G3519 and MSPM0G5187. A map is a file of the chip's blocks, pins and interrupts, made from the SDK headers. The page matches the chip's DEVICEID against the maps, and a chip without one is refused. Other MSPM0 chips need a new map file.

### Why does a PWM pin show no rate, or show "fast"?

A pin driven by a timer does not change its output register, so there is no rate to count; the page shows the timer's period instead. A pin that the program switches in software shows "fast" when it changes more than about 100 times a second, because the page reads the port about 500 times a second and cannot count more than that. TI's PWM example read a 16.0 ms period from its timer registers (load 1999, prescale 256, 32 MHz).

### Does the chip view work with my own program?

Yes, with no change to it, as long as the program does not put the chip into a sleep deep enough to drop the debug link. Load, powered blocks, pin setup, slow pin changes and the running code were read from TI's own examples, a CoreMark build and a test program; none needed a change. A program that enters STANDBY or SHUTDOWN needs the recovery above.
