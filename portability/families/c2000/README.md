# Port drivers and code to the C2000 family (LAUNCHXL-F2800137, LAUNCHXL-F28P65X) and between it and the Arm families

C2000 shares no driver layer with the Arm families, and its `char`, `short` and `int` are all 16 bits, so plain C ports in with a handful of quiet edits: BitNetMCU's 64-wide CNN reads all 10 test digits right on the F2800137 after three fixes (matrix X48), the shared 81-line app runs with 35 glue lines and a 5-line `uint8_t` preinclude (X53), and one field-oriented current-loop step moves out to the LP-AM13E230 and LP-AM263P with the file unchanged (X49, X50). There is no GCC, IAR, Keil, Zephyr, FreeRTOS (not in public C2000Ware), Arduino, PlatformIO, MicroPython, ESPHome or Rust target for the C28x.

Terms: [docs/vocab.md](../../../docs/vocab.md) and this family's [FAQ](FAQ.md). **Matrix X48** and the other X numbers are row ids in `portability/matrix.csv`. **C28x** is the C2000 CPU core; **16-bit char** means the smallest addressable unit is 16 bits and `sizeof` counts 16-bit words; **C2000Ware** is TI's software kit for C2000 chips and **driverlib** its register-level driver library; **TMU** is the trig and divide hardware; **CLA** is the control law accelerator, a second small processor; **PIE** is the interrupt controller that routes peripheral lines to the CPU; **cJTAG** is the 2-wire debug link; **FOC** is field-oriented control, the motor-control math. 1 kB = 1,024 bytes. The generated record: [EVIDENCE.md](EVIDENCE.md). The assistant's copy: [AGENTS.md](AGENTS.md).

Contents: the parts and their neighbors, a ten-minute path, the one-command build, the driver and function map, how to port into the family, moving inside it, moving to and from other families, what was measured, errors, sources.

## The board (two parts held)

| Part | Kit | Core and clock | Flash | RAM | On the board | Source |
|---|---|---|---|---|---|---|
| TMS320F2800137 | LAUNCHXL-F2800137 | C28x at 120 MHz, FPU, TMU | 256 kB | 36 kB | XDS110 on the board, 2-pin cJTAG only as shipped; LED on GPIO20; two 12-bit ADCs, 14 PWM channels, classic DCAN (no CAN FD) | `c2000-launchpad/AGENTS.md`, `c2000-launchpad/README.md` |
| TMS320F28P650DK9 | LAUNCHXL-F28P65X | dual-core C28x at 200 MHz, one CLA per CPU | 1.28 MB | 248 kB | XDS110 on the board; LED4 on GPIO12; no radio, no camera interface, no video block | `c2000-launchpad/AGENTS.md` |

Board folder: [`c2000-launchpad/`](../../../c2000-launchpad/AGENTS.md) (`README.md`, `AGENTS.md`, `FAQ.md`, `FAQ-f28p65x.md`). Only CPU1 and C28x-only builds were run on the F28P65X; CPU2 and the CLA are untouched.

### Part-to-part map: the two C2000 parts and the Arm parts their code moves to

A dash means the files here do not record it.

| | F2800137 | F28P65X | AM13E230 | AM263P |
|---|---|---|---|---|
| Core, clock | C28x, 120 MHz | dual C28x, 200 MHz | Cortex-M33, 200 MHz, FPU, TMU, NPU | four Cortex-R5F, 400 MHz |
| Flash, RAM | 256 kB, 36 kB | 1.28 MB, 248 kB | 512 kB, 128 kB | - |
| Byte size | 16 bits | 16 bits | 8 bits | 8 bits |
| Compiler | C2000 CGT 25.11.1.LTS | same | TI Arm Clang 5.1.1.LTS | - |
| Flash speed | 2 wait states at 120 MHz; 10 to 22% slower than RAM | - | - | - |
| FOC step with TMU | 194 cycles from RAM, 221 from flash | - | 117 cycles (0.585 µs) | - |
| PWM trigger to new PWM value | 2.58 µs | - | 1.33 µs | 1.24 µs |
| `INT_FREERTOS` in the PIE | group 12, line 6 | group 12, line 12 | - | - |
| CoreMark, patched, unofficial | 141.0 per second from flash (1.175 per MHz) | 211.79 from flash, 258.86 from RAM (1.294 per MHz) | - | - |

Sources: `c2000-launchpad/AGENTS.md`, `c2000-launchpad/README.md`, `c2000-launchpad/FAQ-f28p65x.md`, `docs/experiments/2026-10-11-current-loop-three-mcus.md` (the AM13E230 and AM263P figures), `am13e230/AGENTS.md`, `am263p/AGENTS.md`, matrix rows X49 and X50.

## Ten-minute path (about 12 minutes measured)

Carry the shared app to the LAUNCHXL-F2800137 and read it over the serial port. This is the 2026-10-25 run in `portability/three-ways/c2000/README.md`, which took about 12 minutes hands-on.

1. Terminal, no board. Build the app with C2000Ware driverlib glue; a 5-line preinclude supplies `uint8_t` for `app.c` only:

   ```bash
   portability/three-ways/c2000/build.sh
   ```

   You should see a RAM image of `5,064 B` in 2.7 to 4.0 s.
2. Terminal, board plugged in. Put your probe serial in `c2000-launchpad/tools/F2800137_CL371081.ccxml`; the connection file must set cJTAG 2-pin mode (`SWD Mode Settings` = 4).
3. Terminal. Back up the flash with the scripting call `s.memory.saveBinary(0x80000, 0x20000, <file>.bin)`. Read it twice and compare the hashes (`c2000-launchpad/AGENTS.md`).
4. Terminal. Load and read through the probe lock:

   ```bash
   python3 tools/frl/lock.py exec --serial CL371081 -- portability/three-ways/c2000/run_board.sh /dev/cu.usbmodemCL3710811 <log dir>
   ```

   You should see `serial_check.py` print PASS: 14 lines at 115200, gaps 0.999 to 1.000 s.
5. Terminal. Read the flash again and compare it with the backup by SHA-256; on the test run the two matched. The LED toggle is in the code and was not seen by eye.

## One-command build

```bash
c2000-launchpad/tools/c2kbuild.sh /tmp/led_ex1_blinky RAM
```

It runs SysConfig, compiles TI's blink with the projectspec flags and links: 4.6 s for the F2800137 (observed 2026-10-06, output not saved). It does not load. For the F28P65X use `c2000-launchpad/tools/c2kbuild_f28p65x.sh <example folder> RAM`, which builds CPU1 only (`c2000-launchpad/README.md`).

## Driver and function map

Columns: C2000Ware driverlib (the only TI driver layer here), plain C carried in, and the other layers.

| Function | C2000Ware driverlib (SDK examples) | Plain C carried in | Zephyr | FreeRTOS | MicroPython, Arduino |
|---|---|---|---|---|---|
| GPIO | Works: blink on GPIO20 (F2800137) and GPIO12 (F28P65X) | Works: shared app LED call in the code, not seen by eye (X53) | Not for this chip: C28x is not in upstream Zephyr (issue 96186), no port exists | Not for this chip: not in public C2000Ware | Not for this chip |
| UART (SCI) | Works: SCI loopback clean to 12.5 Mbaud (F28P65X) | Works: shared app printed 14 lines at 115200 (F2800137) | Not for this chip | Not for this chip | Not for this chip |
| ADC | Works: shared app read pin A6 floating, 2030 to 2156 | Works: same | Not for this chip | Not for this chip | Not for this chip |
| PWM and motor control | Works: PWM-triggered sample, FOC step and PWM update in 2.3 to 2.6 µs | Works: the same FOC file ran unchanged on AM13E230 and AM263P (X49, X50) | Not for this chip | Not for this chip | Not for this chip |
| SPI | not covered here | Works: the CC1407P packer gave the same octets as TI's structs on a Mac, 17 of 17 cases; no radio attached (X47) | Not for this chip | Not for this chip | Not for this chip |
| CAN | Classic DCAN only; a ported F280015x MCAN example does not apply | not covered here | Not for this chip | Not for this chip | Not for this chip |
| Math (TMU) | Works: sine 8 cycles per loop with `--tmu_support=tmu0 --fp_mode=relaxed`, against 82 without | Works: same flags | Not for this chip | Not for this chip | Not for this chip |
| Flash | Works: 10 to 22% slower than RAM at 120 MHz | Works: BitNetMCU 24.1 ms from flash against 21.3 ms from RAM (X48) | Not for this chip | Not for this chip | Not for this chip |
| Low power | Not working: `lpm_ex3_standbywake_gpio` and `lpm_ex5_haltwake_gpio` cannot be halted; the debugger reconnects after a wake-up button or reset | not covered here | Not for this chip | Not for this chip | Not for this chip |
| Display library | not covered here | Should work: u8g2 2.37.1 compiles and links, 19 kB flash and 8.3 kB RAM; not run on a display | Not for this chip | Not for this chip | Not for this chip |

Sources: matrix rows X47 to X50, X53; `c2000-launchpad/AGENTS.md`; `c2000-launchpad/README.md`; `c2000-launchpad/FAQ.md`; `portability/three-ways/c2000/README.md`; `data/support-matrix.json` (Zephyr, FreeRTOS, MicroPython, Arduino, PlatformIO, ESPHome, Rust cells); `ports/bitnetmcu/README.md`; `docs/experiments/2026-10-08-f2800137-u8g2.md`.

## How to port into this family

1. **Pick the layer.** C2000Ware driverlib is the only TI driver layer for the C28x; nothing from MSPM0 DriverLib or TI Drivers lands on it (`portability/everywhere/README.md`, rule on driver layers). Plain C is the portable route.
2. **Give the code a `uint8_t`.** Put `typedef unsigned char uint8_t;` before the library's headers, or `--preinclude` a file that does, for files that do not include driverlib (driverlib's `hw_types.h` already defines `uint8_t`, and a second `typedef` is an error). Use `unsigned char`, not C2000Ware's `uint16_t` form, or u8g2's fonts fail with error #145 (`c2000-launchpad/AGENTS.md`).
3. **Fix shifts.** `int` is 16 bits: write `1UL << n` (`1L << shift` in BitNetMCU) (`c2000-launchpad/AGENTS.md`).
4. **Do not lay structs or bit-fields over wire bytes.** `sizeof` counts 16-bit words. Use a field-by-field packer such as `portability/cc1407p-host/wire/trx_wire.c` (17 of 17 cases equal, 315 octets) (`docs/experiments/2026-10-25-f2800137-cc1407p-wire-packer.md`).
5. **Mask byte wraps.** Code that expects a byte to wrap at 256 breaks: write test digits as signed decimals and mask `xtime()` with `& 0xFF` (`c2000-launchpad/AGENTS.md`).
6. **Add the TMU flags** `--tmu_support=tmu0 --fp_mode=relaxed` for trig-heavy code (`data/results/f2800137.csv`, test `math_sincos`).
7. **Check a ported constant against this chip's own header.** Interrupt macros keep their names across the two parts and change their values (`c2000-launchpad/FAQ-f28p65x.md`).
8. **Check the map for sections outside RAM and main flash** before loading; `dcsm/dcsm_security_tool` writes into the security area at 0x78000 (`c2000-launchpad/AGENTS.md`).
9. **Back up and restore.** F2800137: `c2kflashback.js` and `c2krestore.js`, verified twice; F28P65X: no tested backup command (`c2000-launchpad/AGENTS.md`).

## Moving inside the family

| Move | Route | Measured | Rows and files |
|---|---|---|---|
| F2800137 to F28P65X, interrupt-latency test | Ported test with a hard-coded PIE bit | built and ran with no error, and measured nothing: `INT_FREERTOS` is group 12 line 6 on the F2800137 and line 12 on the F28P65X | `c2000-launchpad/FAQ-f28p65x.md` |
| F280015x MCAN example to F2800137 | Ported example | does not apply: the F2800137 has no MCAN | `c2000-launchpad/AGENTS.md` |

No other move between the two parts has run; the F28P65X has 32 rows in `data/results/f28p65x.csv`.

## Moving to and from the other families

| To or from | Route | Measured | Rows and files |
|---|---|---|---|
| F2800137 to AM13E230 | One FOC step, F28-style C, single precision | 1 tried: 1 unchanged; 1.33 µs trigger to PWM | X49 |
| F2800137 to AM263P | Same file | 1 tried: 1 unchanged; 1.24 µs | X50 |
| MSPM0G3507 to F2800137, BitNetMCU CNN | Plain C built for the C28x | 1 tried: 1 after edits; all 10 digits right; 2.55M cycles from RAM (21.3 ms at 120 MHz), 24.1 ms from flash, 6.7% under the author's count | X48, `ports/bitnetmcu/README.md` |
| Zephyr host to F2800137, CC1407P host driver | 7 files; packer instead of structs | 7 tried: 0 unchanged, 7 after edits; transmit path matches; no radio attached | X47 |
| MSPM0L1306 to F2800137, shared app | C2000Ware driverlib glue | `app.c` unedited; 35 glue lines, 29 config lines, `5,064 B` | X53 |
| ESP32-BLE-Gamepad descriptor | Plain C | `8,302 B` of code, run from RAM, not flashed; matches the library's bytes | `docs/experiments/2026-10-19-esp32-ble-gamepad-port-matrix.md` |
| MSPM0 guide, MSPM33 guide, AM13 guide | | | [MSPM0](../mspm0/README.md), [MSPM33](../mspm33/README.md), [AM13](../am13/README.md) |

## What was measured

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| C2000Ware examples that build by script | 138 of 139 | F2800137, CCStudio 21.0.1, C2000 compiler 25.11.1.LTS | measured | `c2000-launchpad/README.md` |
| Blink build and RAM load | 4.6 s and 3.9 s | observed 2026-10-06, output not saved | measured | `c2000-launchpad/README.md` |
| FOC step with TMU flags | 194 cycles from RAM, 221 from flash; 349 without the flags (RAM) | per loop iteration | measured | `data/results/f2800137.csv` |
| PWM trigger to new PWM value | 2.3 to 2.6 µs | RAM, flash, flash with loop in RAM | measured | `c2000-launchpad/README.md` |
| CoreMark, patched (unofficial) | 141.0 per second from flash, 155.3 from RAM | F2800137, 120 MHz | measured | `data/results/f2800137.csv` |
| CoreMark, patched (unofficial) | 258.86 from RAM, 211.79 from flash | F28P65X | measured | `data/results/f28p65x.csv` |
| Room-occupancy models, plain C, from flash | 1.8, 4.8, 17.6, 65.4 ms | 20 of 20 test vectors right | measured | `data/results/f2800137.csv` |
| Shared app on C2000Ware driverlib | `5,064 B` code and constants, `1,060 B` RAM | 35 glue, 29 config lines | measured | `portability/three-ways/c2000/README.md` |
| BitNetMCU 64-wide CNN | 2.55M cycles from RAM, 21.3 ms | 120 MHz, stack 1.6k of 2k words | measured | `ports/bitnetmcu/README.md` |

## If you see this

- If the build stops at `error #20: identifier "uint8_t" is undefined`, the C28x cannot address a byte and TI's `stdint.h` omits it: put `typedef unsigned char uint8_t;` before the library headers (`c2000-launchpad/AGENTS.md`).
- If you see `warning #64-D: shift count is too large`, `int` is 16 bits: write `1UL << n` (`c2000-launchpad/AGENTS.md`).
- If a ported test builds, runs and measures nothing, reread this chip's own `hw_ints.h` for the interrupt number (`c2000-launchpad/FAQ-f28p65x.md`).

The rest, with causes: [FAQ.md](FAQ.md).

## Where these facts come from

CCStudio 21.0.1, C2000 compiler 25.11.1.LTS, C2000Ware 26.02.00.00 LTS, SysConfig 1.28.1, UniFlash 9.6.0, probe firmware 3.0.0.43, on an Intel Mac with macOS 15.7.9; runs dated 2026-10-06 to 2026-10-25 in the files named. Data: `portability/matrix.csv`, `data/results/f2800137.csv`, `data/results/f28p65x.csv`, `site/data/ports.json`, `data/support-matrix.json`. The generated record: `portability/families/c2000/EVIDENCE.md`. Checked 2026-10-25.
