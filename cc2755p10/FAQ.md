# What goes wrong with the TI LP-EM-CC2755P10 LaunchPad development kit on a Mac, and how do you fix it?

Problems with the LP-EM-CC2755P10 (CC2755P10 chip: Arm Cortex-M33, 96 MHz, 1 MB flash, 162 kB RAM) from a Mac terminal. 1 kB = 1,024 bytes. Terms: [docs/vocab.md](../docs/vocab.md).

**What caused it**, one tag per answer:

| Tag | Meaning |
|---|---|
| Our mistake | Something was done wrong in the run. Listed so you do not repeat it |
| Setup or handling | How the hardware was plugged in or set up |
| Tool or docs | A TI tool bug, a missing tool, or a missing or wrong instruction |
| Chip limit | The silicon cannot do it |
| Unknown | Not found |

## Connecting and flashing

### The board is plugged in and nothing happens. Where is the USB port?

There is none: plug the board onto an LP-XDS110ET or LP-XDS110 debugger and plug the debugger into the Mac, with the debugger's TGT VDD jumper on XDS. TI's tool page says the debugger is "sold separately and required"; the board has no USB port and no power input of its own, so on the EXT setting it stays dark. TI published the board's user guide, SLVUE03, in August 2026 (found 2026-10-08).

### If the debugger says Error -2001 or "Unable to authenticate with Secure Debug Manager", what do you do?

Look at the line before the error, because there are two different cases, and do not run a chip erase to "reset" the chip. A CC27xx chip only opens its debug port when both of its configuration areas are valid: CCFG at 0x4E020000 and SCFG at 0x4E040000 (TI's CC27xx technical reference manual SWCU195A, section 9.1.3). The two answers below cover the case where they are still valid and the case where they are blank. Seen with UniFlash 9.6.0 (DSLite) and CCStudio 21.0.1 on macOS, 2026-10-09.

### Error -2001 right after a TI example ran on the LP-EM-CC2755P10, and the message says debug authentication is not required. How do you get the debugger back?

Reset the chip through the probe, then cut its power, and read the boot status after each step. It appeared after the SDK's `zigbee/rf_test` example, before any erase; the authentication message was seen in that run but its log was not saved. On this board neither `xds110reset -a toggle -d 100 -s <probe serial>` nor connect under reset (`Error -615`, log not saved) nor a `uhubctl` power cycle of the probe's hub port brought it back, and later tests showed that hub cycle most likely does not cut the board's power. Unplugging the probe's USB cable for ten seconds has not been tried on this board. How to read the boot status and power-cycle remotely: [docs/remote-board-recovery.md](../docs/remote-board-recovery.md).

### After a chip erase, every command on the LP-EM-CC2755P10 fails with "Debug is not allowed based on" and Error -2001. Is it bricked, and how do you get it back?

No. Program it over the chip's device management interface (SACI), which needs no debug connection, then use DSLite as usual. This board recovered this way on 2026-10-09 in under a minute of programming. The erase was a mistake in the run. A chip erase on a CC27xx clears the CCFG (0x4E020000) and SCFG (0x4E040000), and the debug port stays closed until both are valid again (TI's technical reference manual SWCU195A, section 9.1.3). DSLite, UniFlash's command-line tool, then stops in its connect script with `Debug is not allowed based on`, `Unable to authenticate with Secure Debug Manager!` and `Error -2001`, and has no other route in. The way back is TI's flow in SWCU195A section 9.2.4.1, "Initial Programming of a New Device": over SACI, program main flash, the SCFG and the CCFG, then reset. [cc2755p10/recovery/saci_program.js](recovery/saci_program.js) does it with the XDS110 probe and CCStudio's scripting runner (`ccs/scripting/run.sh`). Run it with `check` first: it writes nothing and confirms that the chip answers and that the CCFG and the first 8 kB of flash are blank. Then run it with `write`, giving TI's SDK `drivers/empty` blink (`.hex`), a CCFG and an SCFG. It programs the blink, the SCFG and the CCFG, checks each by CRC, and resets. It refuses a CCFG or SCFG that closes debugging, forbids any permission, or sets keys or secure boot. It skips the chip erase that the TRM's flow starts with: on an already erased chip that erase would also remove the HSM firmware, because with no valid CCFG nothing is retained (SWCU195A 9.3.2.3.1). After the reset the debug port was open (boot status 0xC2), and [restore.sh](restore.sh) put the shipped image back and read it back identical. The HSM firmware survived the erase: the boot ROM reports version 4.7.6 ([recovery/saci_info.js](recovery/saci_info.js) asks it, read-only). That is newer than the HSM image in the F3 SDK 9.21.00.36 (`CC2745R10_Q1_hsm_fw_v3.5.0.bin`), so do not program the SDK's file over it. SEGGER says its J-Link does the same recovery on connect; none was needed here. Details: docs/arrivals/cc2755p10.md, "What happened".

## Building

### Why did building TI's Thread images use more than a gigabyte of disk?

TI's OpenThread build (ot-ti, thread-v1.4-ti-2.1) also writes each image as a raw `.bin` file, and that file runs from the start of flash to the configuration area at a high address, so each one is about 1.22 GB. Two images and their build folders took about 1 GB of a nearly full Mac disk on 2026-10-08. Flash the `.out` file instead, and delete the `.bin` files and the build folder once the images are copied; `cc2755p10/prebuild.sh` now does both.

### `west build --shield ti_bp_cc140xp` for `lp_em_cc2755p10` stops with "undefined node label 'boosterpack_spi'". Can the CC1407P radio run on this board?

Not run on a radio (2026-10-24: all four samples flashed and started with no radio fitted, see the next question); it builds without the shield. TI's Zephyr fork (simplelink-zephyr v3.7.0-ti-1.00.01_ga) ships the radio's shield with a board fix-up for the LP-EM-CC2745R10-Q1 only, and the LP-EM-CC2755P10 board file defines `boosterpack_header` but not `boosterpack_spi`. Build with no `--shield` and pass `-DEXTRA_DTC_OVERLAY_FILE=ports/cc1407p-zephyr/boards/lp_em_cc2755p10.overlay`, which declares the radio on SPI0 with the same pins as the CC2745 fix-up. With a cross-compile toolchain instead of the Zephyr SDK (the Intel Mac setup here), also add `-DTOOLCHAIN_HAS_NEWLIB=ON -DCONFIG_NEWLIB_LIBC=y`, because the samples need `strtok`, `sscanf` and `rand`. Four samples build this way; none has run on a radio. The hardware needs the red LED jumper off and one wire from the radio's pin 23 to pin 10. 2026-10-10.

### A program ported from the LP-EM-CC2340R5 runs but its serial console is silent. Why?

The GPIO driver index on the CC27xx is the DIO number, not the position in the SysConfig list. A copy that toggled "pin 0 and pin 1" for the LEDs set DIO1, the UART TX pin, to an input in its start-up code, so the CPU ran (SysTick ticked, the program polled the UART) and nothing came out. Use `CONFIG_GPIO_LED_0` (red, DIO16) and `CONFIG_GPIO_LED_1` (green, DIO12) from `ti_drivers_config.h`. Found with uLisp on 2026-11-09 by reading SysTick and the program counter over the probe; after the change the REPL answered at 115200 baud on the probe's first serial port.

### Can I run Arduino sketches or MicroPython through TI's Zephyr fork on this board?

Not as of 2026-11-09. Both were tried on the fork's `lp_em_cc2755p10` board (Zephyr 3.7.0, tag `v3.7.0-ti-1.00.01_ga`): the ArduinoCore-zephyr module failed with `'const struct device' has no member named 'ops'` and a blobs key the fork's schema rejects, and MicroPython v1.29.0 with `alloca.h: No such file or directory` (the build kept the minimal C library). The fork has no ADC driver for this chip. The errors are in `cc2755p10/bench/zephyr-fork/`. A Lisp prompt does run without Zephyr: `ports/ulisp/boards/cc2755p10/`.

### A CC1407P host (TI's Zephyr sample or this repo's plain-C one) prints nothing, or stops after its first line, on the LP-EM-CC2755P10. Why?

With no BP-CC1407P connected, TI's host driver waits forever: `TRX_Host_open` ends with `while(TRX_HOST_IS_PENDING(handle));` and has no timeout. On 2026-10-24 the plain-C host (`cc2755p10/host-cc1407p/`) printed `X45 start spi_open=1` and stayed in that call with no fault after one reset pulse, one SPI transfer and two timer one-shots. TI's rf_diagnostics sample printed `RF Diagnostics Example` and stopped; genfsk_tx, genfsk_rx and carrier_wave printed nothing, not even the Zephyr boot line (likely the waiting main thread starves the log thread; not confirmed with a debugger). If you see this, check the radio is on the header, the red LED jumper is off, and one wire joins BoosterPack pin 23 to pin 10; or put your own timeout around the open call.

### Do TI's TI Drivers examples for the LP-EM-CC2340R5 run on the LP-EM-CC2755P10 as they are?

Yes. All 33 shared F3 SDK examples, built with the LP-EM-CC2340R5 `.c` files in the LP-EM-CC2755P10 project, ran on 2026-10-24: 22 showed TI's documented behavior, 10 waited for hardware not on the bench (SD card, I2C sensor, second board, input signal). Only the `log` example uses 3,000,000 baud; every other UART example is at 115200.

### `frl identify --board cc2755p10` stops with `FRL-E099 FAILED: KeyError: 'deviceid'`. How do you identify the chip?

Use the probe form: `python3 tools/frl/frl.py identify --probe <serial>` printed `probe <serial> is attached to CC2755P105E0WRHAR` on 2026-10-25, while the `--board cc2755p10 --probe` form raised the KeyError. The failing step is this repo's identify route for the board name, not the chip.

### `./start.sh cc2755p10 blink` fails with "Unknown Property: FlashEraseSetting" or with "stayed at one level". What do you do?

The first is DSLite on this chip's loader: it has no `FlashEraseSetting` (that is the CC2340R5's), and erases by default while keeping the HSM sectors, so the route lists `RetainMainSectors=true` and no chip-erase line. The second is a sampling fault, not a dark LED: each DSLite read takes about 4 s, close to three periods of the 1.35 s default pattern, so back-to-back reads kept landing on one phase. Both are fixed in `tools/frl/lpf3.py` and `data/routes.json` (2026-11-02, three runs in a row passed, `data/results/cc2755p10.csv` row `blink_flash_repeat`). The route backs up main flash, CCFG and SCFG first and refuses an image with no SCFG (`FRL-E036`), because a chip erase clears the SCFG and the debug port stays closed without one.

### How fast can the CC2755P10 run a digital-power PID loop, and what changes from the CC2340R5 program?

Q15 holds 331 kHz and float 318 kHz at -O2 (338 and 329 at -O3), with the interrupt entered 128 CPU cycles after the PWM zero and the duty written 282 (Q15) or 294 (float) cycles after it. The CC2340R5 program ports with three changes: take both clocks from `portability/boards/boards.csv` (CPU 96 MHz, LGPT0 and ADC 48 MHz), use ADC pin DIO27_A1, and count the timer ratio over 50k CPU cycles. Build with SysConfig 1.28.0; 1.26.3 is gone and the build says `sysconfig_cli.sh: No such file or directory`. Details: bench/README.md, rows `power_loop*` in `data/results/cc2755p10.csv`.

### How do I read the battery voltage on the LP-EM-CC2755P10 with nothing wired?

Add the ADC instance with the internal battery channel: `adc0.internalSignal = "Battery Channel";` in SysConfig. It samples VDDS/3 and read 1374 to 1375 counts at about 3.3 V (about 1365 expected). The app, unchanged, ran on TI Drivers + FreeRTOS: 12 lines at 115200 on the backchannel port ending in `1`, 1.004 to 1.005 s apart (checked 2026-10-25).

## Building C++ (ESPHome) with TI's Zigbee flags

### A C++ file stops with "`<cerrno>` tried including `<errno.h>` but didn't find libc++'s `<errno.h>`". What do you change?

Put TI's POSIX folder after the compiler's own headers for C++ files: `-idirafter` on the SDK's `source/ti/posix/ticlang` folder in place of `-I`. The SDK's Zigbee makefile lists that folder first, and its `errno.h` hides the one libc++ needs. Every SDK example is C, so the makefile never meets it. Used in `ports/esphome-ti/build.sh`.

### A C++ file stops with "cannot initialize return object of type 'char *' with an rvalue of type 'const char*'" in `strings.h`. What do you change?

Add `-U_INLINE` for C++ files. At `-Oz` TI Arm Clang 5.1.1 turns on its inline `index()` and `rindex()` in `strings.h`, which return `char *` from `strchr` and `strrchr`; in C++ those return `const char *`.

### Including `hw_fcfg.h` from C++ says "field has incomplete type 'serialRomBldrParam_t'". What do you change?

Read the chip's MAC address, reset and temperature from a C file and call it from C++. The driverlib header declares a struct inside a union inside another struct, which means something different in C++. Example: `ports/esphome-ti/shim/ti_glue.c`.

### The linker says "invalid option: --wrap=..." What do you use instead?

Nothing replaces it in TI's linker (`tiarmlnk`). If you only need the wrapped call for logging, drop the flag; if you need the hook, define the `__real_` names as stubs for the calls TI's libraries leave undefined. Both done in `ports/esphome-ti/`.

### A Zigbee image links but uses 14 kB more RAM than it should. Why?

Include `zb_mem_config_lprf3.h` in the file with your `main`. TI's `on_off_light.c` includes it, and it sets how big ZBOSS's tables are; without it they take the large default (47.6 kB of RAM instead of 33.6 kB in the ESPHome link test, 14 kB more).

### `esphome logs` shows nothing, though the board runs. What do you check?

`logger:` must be in the YAML, and the platform must pass ESPHome's build flags to the compiler. ESPHome sets the log level as a compiler flag (`ESPHOME_LOG_LEVEL`), not in a header; a build that drops the flag compiles every log call out and the UART stays silent, though a direct write to it works. Use this folder's `ports/esphome-ti` from 2026-11-10 or later, open the probe's first serial port (`/dev/cu.usbmodem<probe serial>1`) at 115200 baud, and run `esphome logs <yaml> --device <port>` ([E319](../docs/experiments/2026-11-10-esphome-ti-logger.md)).

### ESPHome says "This feature is only available on [esp32, esp8266, rp2, nrf52, host]" for `i2c:` or `spi:`. What do you do?

For `i2c:`, use this folder's ESPHome platform from 2026-11-10 or later; it lets its names through that check. `spi:` and `uart:` are not supported yet, and `spi:` has no nrf52 entry to follow ([E321](../docs/experiments/2026-11-10-esphome-ti-buses.md)).

### SysConfig says "CONFIG_GPIO_RLED ... Resource conflict. DIO14/39 is currently in use by CONFIG_FRL_PWM_0". What do you change?

Nothing in the YAML: the platform drops TI's LED or button GPIO instance when a driver takes that pin. If you wrote your own SysConfig file, remove the `LED_R` instance and define `CONFIG_GPIO_RLED` as the DIO number; the GPIO driver indexes by DIO.

### The compiler says "static assertion failed ... is_iec559" in light_color_values.h. What do you change?

Build with this folder's platform, which removes that assertion in the build copy. TI Arm Clang's C++ library reports `numeric_limits<float>::is_iec559` as false although the chip does single-precision IEEE 754; any ESPHome `light` fails the check otherwise.

### SysConfig says "DIO21 is not a pin name" for an ADC pin on the CC2755P10. What do you write?

The name with its analog suffix, `DIO21_A4`. The platform does this from its pin table; write `DIO21` in the YAML.

## Mistakes to avoid

- A chip erase on a CC27xx to clear a connect error closed the debug port until a full image was programmed over SACI (the entries above). On a CC27xx, never erase to reset; reset with the probe and replug.
- The first restore script put back main flash and the CCFG but not the SCFG. Its one run after the erase stopped at the connect, before erasing; had it run on a working chip, its erase would have left the chip locked. A CC27xx backup and restore must cover main flash, CCFG and SCFG; `cc2755p10/restore.sh` now adds an SCFG and restored this board on 2026-10-09.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-25. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- Error -2001 or -615 after TI's zigbee/rf_test example, cause unknown (`cc2755p10/FAQ.md`, `docs/remote-board-recovery.md`, 2026-10-09)
- A chip erase closes the debug port: Debug is not allowed based on, Error -2001, until main flash, CCFG and SCFG are programmed again (`cc2755p10/FAQ.md`, `docs/remote-board-recovery.md`, 2026-10-09)
- A GCC ot-ti Thread image leaves the CCFG and SCFG checksums at 0, so the port stays closed (`cc2755p10/AGENTS.md`)
- SACI blank-check commands aimed at the HSM area time out and leave boot status 0x3F (`docs/remote-board-recovery.md`, 2026-10-09)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| tools/recovery/cc2x_status.js: read the boot status (reads only), before and after each step | remote | proven | `tools/recovery/cc2x_status.js`, `docs/remote-board-recovery.md`, 2026-10-09 |
| xds110reset -a toggle -d 100 (took boot status 0x3F to 0xBA once) | remote | proven | `docs/remote-board-recovery.md`, 2026-10-09 |
| xds110reset -a toggle -d 100 (after zigbee/rf_test) | remote | failed | `docs/remote-board-recovery.md`, `cc2755p10/FAQ.md`, 2026-10-09 |
| uhubctl power cycle of the probe's hub port, 5 s (the board most likely kept power) | remote | failed | `docs/remote-board-recovery.md`, 2026-10-09 |
| Connect under reset (Error -615, one try) | remote | failed | `docs/remote-board-recovery.md`, 2026-10-09 |
| SACI programming: recovery/saci_program.js check, then write (blink hex, CCFG, SCFG); skip the chip erase on a blank chip (for the erase lockout, under a minute; HSM firmware 4.7.6 was intact) | remote | proven | `cc2755p10/recovery/saci_program.js`, `docs/remote-board-recovery.md`, 2026-10-09 |

USB hub ports recorded for this board:

- The probe's hub port does not cut power, 2026-10-09. A 5 s cycle left the chip at boot status 0x3F, so the board most likely kept power.

Restore: cc2755p10/recovery/saci_program.js; cc2755p10/restore.sh builds the image (not run since the SCFG was added). Regions: main flash, CCFG 0x4E020000 (2 kB), SCFG 0x4E040000 (1 kB); HSM firmware through UniFlash if missing (not the SDK's 3.5.0 file). (`cc2755p10/restore.sh`, `docs/remote-board-recovery.md`)

<!-- recovery:end -->
