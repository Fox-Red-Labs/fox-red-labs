# What should an AI coding assistant know before working on a TI LAUNCHXL-F2800137 or LAUNCHXL-F28P65X LaunchPad development kit?

Short answer: a C2000 byte is 16 bits, so portable Arm code breaks in quiet ways (missing `uint8_t`, wrong checksums, shifts that overflow), and that TI's debug tools on a Mac need a matching probe firmware and a connection file made for the exact chip. This file is the short list. Each item comes from runs on these boards unless it says otherwise.

Terms: [docs/vocab.md](../docs/vocab.md), the [LAUNCHXL-F2800137 FAQ](FAQ.md) and the [LAUNCHXL-F28P65X FAQ](FAQ-f28p65x.md) in the Fox Red Labs repo. In short: the **probe** is the XDS110 debugger built into the board; a **connection file** (`.ccxml`) tells TI's tools which probe and chip to use; the **TMU** is the F2800137's trig and divide hardware; **SysConfig** is TI's tool that writes pin and clock setup code; **C2000Ware** is TI's software kit for C2000 chips; **CoreMark** is a standard CPU speed test; **PIE** is the interrupt controller that routes peripheral lines to the CPU; **CLA** is a second, smaller processor on some C2000 chips. 1 kB = 1,024 bytes. Checked 2026-10-06 (F2800137) and 2026-10-08 (F28P65X) on an Intel Mac, macOS 15.7.9, with CCStudio 21.0.1, C2000 compiler 25.11.1.LTS, C2000Ware 26.02.00.00 LTS, UniFlash 9.6.0 and probe firmware 3.0.0.43, on one board of each.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths below in backticks starting `c2000-launchpad/` are in the Fox Red Labs repo, where the evidence for each line lives; they are not in your copy, and links in this file only work inside that repo.

## What both boards share

| Part | Fact |
|---|---|
| CPU | C28x core with floating-point unit; `char`, `short` and `int` are all 16 bits and `sizeof` counts 16-bit words (`c2000-launchpad/FAQ.md`) |
| Debugger | XDS110 on the board, USB; the F2800137 is wired for 2-pin cJTAG only (4-pin needs R44 and R45 fitted) (`c2000-launchpad/FAQ.md`) |
| Compiler flags used | `-v28 -ml -mt --float_support=fpu64 --abi=eabi` |

## Start here

Built and checked with CCStudio 21.0.1, C2000 compiler 25.11.1.LTS, SysConfig 1.28.1, C2000Ware 26.02.00.00 LTS, UniFlash 9.6.0 and probe firmware 3.0.0.43. The `tools/` scripts expect CCStudio in `/Applications/ti/ccs2101` and C2000Ware in `/Applications/ti/c2000ware_core_26_02_00_00`; set `TI_ROOT` to the folder that holds them if yours differ (`c2kbuild_f28p65x.sh`; `c2kbuild.sh` reads `C2000WARE` or you edit its top lines). Find the probe serial with `xdsdfu -e` (in CCStudio's `ccs_base/common/uscif/xds110/` folder). Put it in place of `CL371081` in `c2000-launchpad/tools/F2800137_CL371081.ccxml` (F2800137) or the serial in `c2000-launchpad/tools/F28P650DK9_CL650001.ccxml` (F28P65X).

| Step | LAUNCHXL-F2800137 | LAUNCHXL-F28P65X |
|---|---|---|
| Backup | No script. The flash was saved on arrival: 0x20000 words from 0x80000, read twice, identical SHA-256 (`c2000-launchpad/data/flash-as-received.bin`). The scripting call that reads it is `s.memory.saveBinary(0x80000, 0x20000, <file>.bin)` | No tested backup command |
| Build | `cp -R /Applications/ti/c2000ware_core_26_02_00_00/driverlib/f280013x/examples/led /tmp/led_ex1_blinky` then `c2000-launchpad/tools/c2kbuild.sh /tmp/led_ex1_blinky RAM` | `c2000-launchpad/tools/c2kbuild_f28p65x.sh <example folder> RAM` (CPU1 only; finds CCStudio and C2000Ware under `TI_ROOT`, default `/Applications/ti`, or from `C2000WARE`, `CGT` and `SYSCFG`, and says which one is missing) |
| Load and check | `/Applications/ti/ccs2101/ccs/scripting/run.sh c2000-launchpad/tools/c2krun.js /tmp/led_ex1_blinky/build-RAM-Ooff/led_ex1_blinky.out blink` samples GPIO20 eight times | `/Applications/ti/ccs2101/ccs/scripting/run.sh c2000-launchpad/tools/c2krun_f28p65x.js <prog>.out blink` samples GPIO12 (LED4) |
| Keep after power-off | Build with `FLASH` in place of `RAM` and run the same command | Same, with `FLASH` |
| Restore | `run.sh c2000-launchpad/tools/c2kflashback.js <abs>/flash-as-received.bin <abs>/<readback>.bin`, then `run.sh c2000-launchpad/tools/c2krestore.js`. Verified twice | Not tested |

A RAM load is gone at power-off. Separate UniFlash command-line calls each reconnect, so use the scripting runner to watch a running program. The F2800137 blink built in 4.6 s and loaded in 3.9 s; the F28P65X blink was built and seen changed by eye, not timed.

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| `error #20: identifier "uint8_t" is undefined` | The C28x cannot address a byte, so TI's `stdint.h` omits `int8_t` and `uint8_t` | Put `typedef unsigned char uint8_t;` before the library's headers | The file compiles |
| `error #145: a value of type "char [772]" cannot be used to initialize an entity of type "const uint8_t [772]"` (u8g2) | C2000Ware's `typedef uint16_t uint8_t;` clears error #20 but breaks the fonts | Use the `unsigned char` typedef, not the `uint16_t` one | The fonts compile |
| `warning #64-D: shift count is too large` | `int` is 16 bits | Write `1UL << n` (`1L << shift` in BitNetMCU) | No warning |
| `ERROR: ee_u16 is not a 16b datatype!` | Unmodified CoreMark assumes bytes | Use this repo's three-change patch; the score is then unofficial | CoreMark validates |
| `unresolved symbols remain` naming `code_start` (F2800137) | A SysConfig-only example's entry branch is not generated | Add `f280013x_codestartbranch.asm`, `--abi=eabi` and every `.c` SysConfig writes | The link succeeds |
| `unresolved symbols remain` naming `TransferRingBuffer_init` | `transfer_ringbuffer.c` is in neither the projectspec nor SysConfig output | Add `utilities/transfer/source/transfer_ringbuffer.c` | The link succeeds |
| `Operation was aborted on 'C28xx_CPU1'` on `lpm_ex3_standbywake_gpio` or `lpm_ex5_haltwake_gpio` | The chip is asleep and cannot be halted | Press the wake-up button or reset | The debugger reconnects |
| `(Error -261 @ 0x0) Invalid response was received from the XDS110.` | Seen once under heavy Mac load | Retry the load | The same image loads |
| `There are not enough resources available` | The profile clock and a hardware breakpoint share debug hardware | Time with a CPU timer instead | The cycle count prints |
| `fatal error: cannot open source file "inc/stw_dataTypes.h"` | A ported F280015x MCAN example; the F2800137 has no MCAN | Do not port it | Not applicable |
| `cannot set '$assign' to 'EPWM2'`, `'GPIO25'`, `'GPIO30'` or `cannot set 'ecapBase' to 'ECAP3_BASE'` | The F2800137 lacks those parts | Pick pins and modules this chip has | SysConfig accepts it |
| `invalid attribute for "struct TRX_Phy_s"` with `uint8_t` undefined (TI CC1407P host driver) | `__attribute__((__packed__))` written directly in TRX.h; no 8-bit type | `--preinclude` `portability/cc1407p-host/c28x/c28x_compat.h`; it builds, but the message bytes are still wrong on a 16-bit char (run 2026-10-24: 9 units sent against 10 bytes), so add a field-by-field packer | 12 of 12 files compile; CRC-16 of `123456789` is 0x29B1 |
| The CC1407P host driver sends different bytes than on Arm, or `LRF_*_byteCount` is half what you expect | Structs and bit-fields laid over the SPI buffer assume 8-bit bytes; `sizeof` counts 16-bit words | Use the field-by-field packer `portability/cc1407p-host/wire/trx_wire.c` (run 2026-10-25: 17 of 17 cases equal, 315 octets, same digest as a Mac) | The stream digest DC1DF5B1 matches |
| `error: invalid redeclaration of type name "uint8_t"` when preincluding a `uint8_t` typedef into a file that includes driverlib | `driverlib/inc/hw_types.h` already defines `uint8_t` | Preinclude only the files that do not include driverlib (`portability/three-ways/c2000/build.sh`, run 2026-10-25) | The file compiles |
| `8888808888` for every digit (BitNetMCU), or a wrong tiny-AES-c ciphertext | Code assumes a byte wraps at 256 | Write the test digits as signed decimals; mask `xtime()` with `& 0xFF` | Digits differ; FIPS-197 C.1 passes |

Traps shared with other boards: TRAPS.md.

## Traps on both boards

1. **Check a ported constant against this chip's own header.** Interrupt macros keep their names across chips and change their values (see the F28P65X section).
2. **Check an image's map for sections outside RAM and main flash before loading it.** `dcsm/dcsm_security_tool` puts data in the one-time-programmable security area at 0x78000 and was skipped (`c2000-launchpad/FAQ.md`).

## LAUNCHXL-F2800137

| Part | Fact |
|---|---|
| Chip | TMS320F2800137, 120 MHz, 256 kB flash, 36 kB RAM, TMU, two 12-bit ADCs, 14 PWM channels, CAN (classic DCAN, no CAN FD), no LIN port, two eCAP modules (`c2000-launchpad/README.md`, `c2000-launchpad/FAQ.md`) |
| LED | TI's blink toggles GPIO20 (`c2000-launchpad/README.md`) |
| Crystal | Y2 on X1 and X2; 20 MHz is assumed by TI's `device.h`, not measured |
| Not on the chip | No AES hardware, no FreeRTOS in C2000Ware's public copy for this chip |

- **TMU.** Add `--tmu_support=tmu0 --fp_mode=relaxed`; sine takes 8 cycles per loop iteration against 82 without (`data/results/f2800137.csv`).
- **Flash is 10 to 22% slower than RAM** at 120 MHz (2 wait states) (`c2000-launchpad/FAQ.md`).
- **TI's DCC examples** report fail as shipped; build with `--define=USE_PLL_SRC_XTAL` (ex1, ex2, ex4 also `--define=CPU_FRQ_100MHZ`) or add `dcc1.fclk1 = 100;` (ex3 syscfg).
- **Read chip identity** from PARTIDH 0x5D00A and REVID 0x5D00C: the chip here read `0x06FF0500` and revision B (`c2000-launchpad/FAQ.md`).

## LAUNCHXL-F28P65X

| Part | Fact |
|---|---|
| Chip | TMS320F28P650DK9, dual-core C28x at 200 MHz, 1.28 MB flash, 248 kB RAM, one CLA per CPU |
| LED | LED4 on GPIO12 (`data/results/f28p65x.csv`) |
| Not on the chip | No radio, no camera interface, no video block |

- **Ported interrupt test never fires.** `INT_FREERTOS` is PIE group 12 line 12 here (`0x00DB0C0CU`, low byte line, next byte group), line 6 on the F2800137. No compiler error (`c2000-launchpad/FAQ-f28p65x.md`).
- **Only CPU1 and C28x-only builds were run.** CPU2 and the CLA are untouched.

## What ran here

- F2800137, blink: 4.6 s build, 3.9 s RAM load; box to blink with a flash write not measured (`c2000-launchpad/FAQ.md`).
- F2800137, FOC step with TMU: 194 cycles from RAM, 221 from flash; PWM trigger to new PWM value 2.3 to 2.6 µs (`data/results/f2800137.csv`).
- F2800137, patched CoreMark: 141.0 per second from flash (1.175 per MHz), 155.3 from RAM; Dhrystone 0.362 DMIPS per MHz from flash (`data/results/f2800137.csv`).
- F2800137, occupancy models 500 to 30k: 1.8 to 65.4 ms plain C from flash, 20 of 20 vectors (`data/results/f2800137.csv`).
- F2800137, TI examples: 138 of 139 build from a terminal.
- F28P65X, patched CoreMark: 258.86 per second from RAM (1.294 per MHz), 211.79 from flash; SCI loopback clean to 12.5 Mbaud (`data/results/f28p65x.csv`).
- F28P65X, occupancy models: 0.893 ms to 47.9 ms, 20 of 20 vectors; `occ_cnn_30k` did not fit the shared RAM layout and ran from flash (`data/results/f28p65x.csv`).

<!-- ports:start (tools/render_ports.py) -->
## Ports that ran here

Code written for other chips and tools, brought to this board. The files to copy, the command and what you see for each are in `docs/ports.md`; the method and a prompt to port your own are there too. Works ran and did what it should; Could work ran in part.

| Port | Status | Result | Evidence |
|---|---|---|---|
| BitNetMCU on a C2000 control chip (LAUNCHXL-F2800137) | Works | BitNetMCU's 64-wide CNN reads all 10 test digits right on the LAUNCHXL-F2800137. | `ports/bitnetmcu/README.md` |
| C2000 current-loop code on the LP-AM13E230 | Works | The same current-loop step, trigger to new PWM, runs about twice as fast on the LP-AM13E230 as on the LAUNCHXL-F2800137. | `docs/experiments/2026-10-11-current-loop-three-mcus.md` |
| ESP32-BLE-Gamepad (a Bluetooth HID gamepad library) on TI, ST and C2000 boards | Works | The ESP32-BLE-Gamepad descriptor and reports match the library's output byte for byte on four boards, and 15 of 15 button changes from the LP-EM-CC2340R5 reached a Mac as gamepad reports. | `docs/experiments/2026-10-19-esp32-ble-gamepad-port-matrix.md` |
| CC1407P host driver on the LAUNCHXL-F2800137 | Could work | The CC1407P driver files compile for the LAUNCHXL-F2800137 with a compatibility header, and a field-by-field packer gives the same 315 octets as an Arm host. | `docs/experiments/2026-10-25-f2800137-cc1407p-wire-packer.md` |
<!-- ports:end -->

## Where the evidence lives

`c2000-launchpad/README.md`, `c2000-launchpad/FAQ.md`, `c2000-launchpad/FAQ-f28p65x.md`, `ports/bitnetmcu/c2000/`, `data/results/f2800137.csv` and `data/results/f28p65x.csv`. If you find something this file gets wrong on your board, the FAQ is where it should be corrected first.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-24. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

**LAUNCHXL-F2800137**

No lockup is recorded for this board.

**LAUNCHXL-F28P65X**

No lockup is recorded for this board.

<!-- recovery:end -->
