# LP-MSPM0G3507 and LP-EM-CC2340R5: where the software paths differ

The LP-MSPM0G3507 and the LP-EM-CC2340R5 are close cousins (both Arm Cortex-M0+), and the steps to build, flash and run them differ at almost every point. This page lists the differences so you do not carry the habits of one board to the other. Checked 2026-10-05 on one Intel Mac from the runs in `mspm0g3507/` and `cc2340r5/`. Where a path was not run on one board, the cell says so. Terms: [vocab](vocab.md).

## Side by side

| | LP-MSPM0G3507 | LP-EM-CC2340R5 |
|---|---|---|
| What it is | General-purpose microcontroller | Bluetooth Low Energy wireless microcontroller |
| Debug probe | On the board | Separate board (LP-XDS110ET), clipped on |
| Cable to the Mac | USB-A end in the box. Needed a dongle. | USB-C on the probe |
| Shows up on the Mac as | XDS110 probe and two serial ports, no driver | Same |
| SDK | MSPM0 SDK 2.11.00.07 | SimpleLink Low Power F3 SDK 9.21.00.36 |
| SDK blink example | `gpio_toggle_output`, no operating system, DriverLib calls | `empty`, FreeRTOS, TI Drivers calls |
| Pin names | Port and number (PB22) | One list (DIO15) |
| Pin setup tool | SysConfig | SysConfig |
| Agent notes shipped in CCStudio | Yes | Yes |
| Built and flashed through CCStudio's MCP servers | Yes. 2 minutes 29 seconds first time, 10 seconds for a change. | - |
| SDK example built from the command line | - | Yes, 22 seconds, after passing three tool paths by hand |
| Flashed from the command line with TI's tool | - | Yes, UniFlash, 7 seconds. Needs a connection file the tool does not make. |
| Zephyr board name | `lp_mspm0g3507` | `lp_em_cc2340r5` |
| LEDs in Zephyr's board file | Blue only. Red and green need an overlay. | Green and red, plus both buttons |
| Zephyr's default blink | Blue | Green |
| Zephyr flash tool for the board | OpenOCD. Board page says build it from source; a prebuilt one works. | J-Link only. Not the probe the board is sold with. |
| What actually flashed Zephyr | `west flash` with prebuilt OpenOCD | UniFlash, outside `west` |
| File to flash | `zephyr.hex` | `zephyr_crc32.hex` |
| After flashing | A debugger soft reset left it stuck. Board reset fixes it. | Starts on its own. Verify reports one mismatch. |
| Way to lock yourself out | A clock bug, fixed in Zephyr 4.4.2 | A permission setting in the chip configuration. Defaults are safe. |
| Zephyr blinky size | 11.7 kB flash, 3.9 kB RAM | 9.9 kB flash, 4.0 kB RAM |
| Serial output from blinky | "LED state" lines, 115200, first serial port | Same |
| First Zephyr run | 9.5 minutes, nothing installed | 82 seconds, Zephyr already installed |
| Fresh agent with the docs, nothing installed | 2.8 to 10.7 minutes across four models | 3.0 to 3.6 minutes across four models |
| Worst time sink | No Intel macOS compiler in the current Zephyr SDK | Probe not pushed on all the way: 8 minutes |

## Measured side by side (2026-10-05)

Same Zephyr benchmark, same compiler (GCC 12.2.0), both boards. CoreMark per MHz is identical in the O3 build (1.75; that build is really -O2 with loop unrolling, because Zephyr's -O2 comes last), so the difference in raw speed is the clock: 139.88 iterations per second at 80 MHz against 84.07 at 48 MHz. The MSPM0G3507's math accelerator has no counterpart on the CC2340R5 (divide 100 cycles against 284, sine and cosine 116 against 9,659). Zephyr's cycle counter is 80 MHz on the MSPM0 and, despite its config, 1 MHz on the CC2340R5. Full tables: `mspm0g3507/mcu-bench/README.md` and `RESULTS-cc2340r5.md`.

On radio, the CC2340R5 joined the house Zigbee network with TI's example and answered 20 of 20 commands from Home Assistant, median 95 ms (`cc2340r5/zigbee/README.md`). Upstream Zephyr has no radio support for it; the radio runs on TI's SDK.

## Adding a third board: LP-MSPM0L1306 (2026-10-06)

Same bench, same methods, on a 32 MHz, 4 kB RAM MSPM0 (1 kB = 1,024 bytes). Full page: [`mspm0l1306/README.md`](../mspm0l1306/README.md).

| | LP-MSPM0G3507 | LP-EM-CC2340R5 | LP-MSPM0L1306 |
|---|---|---|---|
| Debug probe | On the board | Separate board | On the board |
| SDK example built from the command line | Yes (example sweep) | Yes | Yes, 156 of 170, 3.3 s for the blink |
| Flashed from the command line | OpenOCD | UniFlash | OpenOCD 0.6 s; UniFlash 15.0 s with a hand-edited connection file |
| Zephyr board name | `lp_mspm0g3507` | `lp_em_cc2340r5` | none upstream; the out-of-tree `lp_mspm0l1306` of this repo runs |
| Zephyr hello_world RAM | | | 3.7 of 4 kB |
| CoreMark per MHz, Zephyr GCC, O3 build (really -O2 with unrolling) | 1.75 | 1.75 | 1.87 |
| CoreMark per MHz, TI Arm Clang -O3, bare metal | 2.30 at 80 MHz, 2.46 at 32 | 2.31 | 2.446 |
| Way to lock yourself out | A clock bug, fixed in Zephyr 4.4.2; low-power examples needed a USB replug | A permission setting in the chip configuration | None found; no example run here lost the debugger |

The L1306 needs fewer cycles per CoreMark than the G3507 under Zephyr because its flash adds one wait state at 32 MHz and the G3507's two at 80 MHz. Moving code: plain C and Zephyr app code moved with board code only; TI's DriverLib examples move up from the L1306 to the G3507 easily (101 of 170 unchanged) and down badly (22 of 270), mostly on one G-series SysConfig setting the L-series lacks.

## What is already the same

- The probe family. Both show up as an XDS110, the same tools list them, and both can be picked by serial number.
- SysConfig for pin setup.
- A per-board notes file for AI agents inside CCStudio, in the same format.
- Zephyr's build command and its blinky sample, unchanged between boards.
- The serial port: same speed, same port position, same output.

## Traps on each board and what to do

1. **Flashing Zephyr.** On the LP-MSPM0G3507, `west flash` works with a prebuilt xPack OpenOCD. On the LP-EM-CC2340R5, Zephyr's flash tool is J-Link only, so flash `zephyr_crc32.hex` with UniFlash outside `west`.
2. **UniFlash from the command line** asks for a connection file and cannot write one. UniFlash's window detects the board; save its connection file and pass it to `dslite`.
3. **Which probe was used.** With two boards on a desk, print the probe serial number before every flash and pass it explicitly (`PROBE_SERIAL` in the setup scripts).
4. **LEDs in Zephyr.** The CC2340R5 board file has both LEDs and both buttons. The MSPM0G3507 file has one LED of three; add an overlay for red and green.
5. **Command-line builds on the F3 SDK.** `imports.mak` points at folders that do not exist on a fresh install. Fill in the three tool paths by hand before the first build.
6. **Locking the chip.** Each family has one way to make a chip stop answering: a clock bug on the MSPM0G3507 (fixed in Zephyr 4.4.2) and a permission setting in the CC2340R5 chip configuration. Leave the defaults alone and read the board's FAQ before changing either.
7. **The probe on the CC2340R5 is separate.** Seat it fully. "The target failed to see a correctly formatted SWD header" cost 8 minutes here, and the cause was a probe not pushed on all the way.

## What is shared in the setup scripts

- Same script shape for every board: `setup-intel-mac.sh` with `ZP`, `TC` and `PROBE_SERIAL`, refusing a missing or unnamed probe. Done for both boards.
- Same proof for every board: read the serial port (`tools/serial_led_check.py`) or the LED pin.

## Limits

Two cells are not run: the CC2340R5 through CCStudio's MCP servers, and the MSPM0 SDK example from the command line. Until they run, this page compares each board's first path, not its best one.
