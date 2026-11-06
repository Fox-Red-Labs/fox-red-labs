# Port: the crypto pack on the LP-MSPM0G5187 (Cortex-M0+, 32 kB RAM)

The same crypto pack fits in 32 kB of RAM with ML-KEM-768 built at -Os and ML-DSA left out.

Terms: [docs/vocab.md](../../../docs/vocab.md). 1 kB = 1,024 bytes. Started from `../mspm0g3519` (same core, same 80 MHz clock, same SDK); that folder owns the harness (`pqc_board.c`, `sec_board.c`, `sec_uecc.c`, `build_pqc.sh`, `build_sec.sh`). This folder holds only what the G5187 changes. Done 2026-10-17 with MSPM0 SDK 2.11.00.07 and TI Arm Clang 5.1.1.LTS.

## What changed from the G3519

| Part | G3519 | G5187 |
|---|---|---|
| RAM | 128 kB in two 64 kB banks, stack in the second bank | 32 kB in one bank; `NO_BANK=1` leaves the SysConfig linker file as it is |
| ML-DSA-44 | built and timed (stack 31 to 52 kB) | not built (`NO_DSA=1`); no stack that large exists |
| Join buffers | linked | `-fdata-sections` and `--unused_section_elimination=on` drop the 7.3 kB the pack never uses |
| RAM log | 12 kB | 3 kB (`PQLOG=3072`) |
| ML-KEM-768 | -O3 | -Os: -O3 needs 20.5 kB of stack for encapsulation and only 19.4 kB is left |
| Clock and SysConfig | `sysctl_mclk_syspll` example | a G5187 SysConfig file (80 MHz PLL). TI's `sysctl_mclk_syspll` hangs on this board waiting for a low-frequency oscillator bit |
| Startup, crypto library | `startup_mspm0g351x`, `mspm0gx51x` | `startup_mspm0g518x`, `mspm0gx51x` (same library folder) |
| OpenOCD | `lp_mspm0g3519` cfg | the `lp_mspm0g3507` OpenOCD cfg |

`build.sh pqc [-Os]` and `build.sh sec -O2 [ti|uecc-asm|uecc-c]` set these and call the G3519 scripts; `run_pqc.sh` is the G3519 script with the probe, cfg and flasher of this board. The edits to the G3519 scripts are environment switches (`NO_BANK`, `NO_DSA`, `EXTRA_CC`, `EXTRA_LD`, `PQLOG`, `PROBE`, `CFG`, `DSF`); their defaults leave that board's build unchanged.

## If you see this

- **The G3519 harness does not link.** 21.7 kB of `.bss` (join buffers, RAM log) plus a 20 kB stack does not fit 32 kB. Drop unused sections and shrink the log, as in the table above; that leaves room for a 19 kB stack.
- **`Crypto_ECDH_getSharedSecret` returns `0x3C` here too.** micro-ecc gives the ECDH row. Details on the [G3519 page](../mspm0g3519/PORT.md).
- **`run_pqc.sh` runs the wrong board.** Use the copy of `run_pqc.sh` in this folder: it sets this board's probe, cfg and flasher.
