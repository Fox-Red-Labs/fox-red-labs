# C2000 family porting notes for AI coding assistants (LAUNCHXL-F2800137, LAUNCHXL-F28P65X)

A C2000 byte is 16 bits, so portable Arm code breaks in quiet ways (a missing `uint8_t`, shifts that overflow, structs that no longer match wire bytes), and that the C28x has no GCC, Zephyr or FreeRTOS. Each line below comes from a run or a file in the Fox Red Labs repo.

Terms: [docs/vocab.md](../../../docs/vocab.md) and the [C2000 family FAQ](FAQ.md). **C28x** is the C2000 CPU core; **C2000Ware** is TI's software kit and **driverlib** its driver library; **TMU** is the trig and divide hardware; **CLA** is the control law accelerator; **PIE** is the interrupt controller; **FOC** is field-oriented control. 1 kB = 1,024 bytes. Checked 2026-10-25 with CCStudio 21.0.1, C2000 compiler 25.11.1.LTS, C2000Ware 26.02.00.00 LTS, UniFlash 9.6.0 and probe firmware 3.0.0.43, on one board of each kit.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths in backticks are in the Fox Red Labs repo, where the evidence for each line lives; they are not in your copy.

## The parts

| Fact | Value | Source |
|---|---|---|
| F2800137 | C28x at 120 MHz, FPU, TMU, 256 kB flash, 36 kB RAM; cJTAG 2-pin only as shipped | `c2000-launchpad/AGENTS.md` |
| F28P65X | dual-core C28x at 200 MHz, 1.28 MB flash, 248 kB RAM, one CLA per CPU; only CPU1 and C28x-only builds were run | `c2000-launchpad/AGENTS.md` |
| Compiler flags | `-v28 -ml -mt --float_support=fpu64 --abi=eabi`; add `--tmu_support=tmu0 --fp_mode=relaxed` on the F2800137 | `c2000-launchpad/AGENTS.md` |
| Types | `char`, `short`, `int` are 16 bits; `sizeof` counts 16-bit words | `c2000-launchpad/FAQ.md` |
| TI driver layers | C2000Ware driverlib only | `portability/everywhere/README.md` |
| Not for this chip | GCC, IAR, Keil, Zephyr (not in upstream, issue 96186, TI said no plans), FreeRTOS (not in public C2000Ware), Arduino, PlatformIO, MicroPython, ESPHome, Rust | `data/support-matrix.json` |

## Start here

| Step | F2800137 | F28P65X |
|---|---|---|
| Probe serial | `xdsdfu -e`; put it in `c2000-launchpad/tools/F2800137_CL371081.ccxml` | `c2000-launchpad/tools/F28P650DK9_CL650001.ccxml` |
| Build | `c2000-launchpad/tools/c2kbuild.sh <example folder> RAM` | `c2000-launchpad/tools/c2kbuild_f28p65x.sh <example folder> RAM` |
| Build the shared app | `portability/three-ways/c2000/build.sh` | not built |
| Load and check | `run.sh c2000-launchpad/tools/c2krun.js <prog>.out blink` samples GPIO20 | `c2krun_f28p65x.js`, GPIO12 |
| Back up | flash read twice with `saveBinary(0x80000, 0x20000, ...)`; saved image `c2000-launchpad/data/flash-as-received.bin` | no tested backup command |
| Restore | `c2kflashback.js`, then `c2krestore.js` | not tested |

## Rules for the assistant

1. **Define `uint8_t` as `unsigned char`** before the library's headers, or `--preinclude` it for files that do not include driverlib; driverlib's `hw_types.h` already defines it and a second `typedef` is an error (`portability/three-ways/c2000/README.md`).
2. **Never use C2000Ware's `uint16_t` form of `uint8_t` with u8g2**: both fonts fail with error #145 (`docs/experiments/2026-10-08-f2800137-u8g2.md`).
3. **Write `1UL << n`** for any shift above 15 bits (`c2000-launchpad/AGENTS.md`).
4. **Do not overlay structs or bit-fields on wire bytes.** Use a field-by-field packer (`portability/cc1407p-host/wire/trx_wire.c`); `LRF_*_byteCount` is half the octet count (`portability/matrix.csv` X47).
5. **Mask byte wraps** with `& 0xFF`; write test digits as signed decimals (`c2000-launchpad/AGENTS.md`).
6. **Reread this chip's `hw_ints.h` before trusting a ported interrupt number.** `INT_FREERTOS` is PIE group 12 line 6 on the F2800137 and line 12 on the F28P65X (`c2000-launchpad/FAQ-f28p65x.md`).
7. **Check the map for sections outside RAM and main flash** before loading (`c2000-launchpad/AGENTS.md`).
8. **Do not port the F280015x MCAN example**: the F2800137 has no MCAN (`c2000-launchpad/AGENTS.md`).
9. **Use a CPU timer, not the profile clock**, when a breakpoint is also in use: `There are not enough resources available` (`c2000-launchpad/AGENTS.md`).
10. **Time unofficial CoreMark as unofficial**: it needs a three-change patch for the 16-bit byte (`c2000-launchpad/AGENTS.md`).

## What ran here from elsewhere

| Code | Result | Source |
|---|---|---|
| BitNetMCU 64-wide CNN | all 10 digits right; 21.3 ms from RAM | `portability/matrix.csv` X48 |
| CC1407P host driver, wire packer | 17 of 17 cases equal to a Mac | X47 |
| FOC step to AM13E230 and AM263P | unchanged, 1.33 and 1.24 µs | X49, X50 |
| Shared three-ways app | serial check passed; 35 glue lines | X53 |
| u8g2 2.37.1 | compiles and links, 19 kB flash; not run on a display | `docs/experiments/2026-10-08-f2800137-u8g2.md` |

## Next

The family guide is `portability/families/c2000/README.md`; every port and test on record is `portability/families/c2000/EVIDENCE.md`; board bring-up is `c2000-launchpad/AGENTS.md`.
