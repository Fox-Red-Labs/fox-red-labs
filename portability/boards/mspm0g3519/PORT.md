# Port: the post-quantum and classical crypto pack on the Cortex-M0+ (LP-MSPM0G3519)

The crypto pack from the CC1354P10 builds and runs on the Cortex-M0+ of the LP-MSPM0G3519 with the algorithm files unmodified. The changes are the compiler flags, a SysTick cycle counter, a RAM log the debugger reads, and a 60 kB stack in the second SRAM bank.

Terms: [docs/vocab.md](../../../docs/vocab.md). 1 kB = 1,024 bytes. Started from `cc1354p10/pqc-join` (Cortex-M33, SimpleLink SDK, UART text report) and `mspm33c321a/tools/build_crypto.sh` (TI example plus a DWT count). Done 2026-10-17 with MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS and SysConfig 1.26.3.

## What changed

| Part | CC1354P10 (M33) | LP-MSPM0G3519 (M0+) |
|---|---|---|
| Algorithm code | `portability/pqc-join/core`, mlkem-native 2.0.0, mldsa-native 2.0.0 | the same files, unmodified |
| Compiler flags | `-mcpu=cortex-m33 -mfloat-abi=hard` | `-mcpu=cortex-m0plus -march=thumbv6m -mfloat-abi=soft`, `-O3` |
| Cycle counter | SysTick with wrap counter (DWT cross-checked) | SysTick on the CPU clock with a wrap counter (the bench timer header); the M0+ has no DWT. 1 tick = 1 CPU cycle |
| Startup and clock | SimpleLink NoRTOS `main_nortos.c` | SysConfig output of TI's `sysctl_mclk_syspll` example (80 MHz from SYSPLL), TI's `startup_mspm0g351x_ticlang.c` |
| Report path | XDS110 UART, parsed by the Mac | none: jumpers J12/J13 may be on BP and the loop wires are fitted. Every line is written to a RAM log (`pqlog`), the debugger reads it when `pq.magic` is `0x50510001`. Same `R,pqc,...` lines |
| Randomness | hardware TRNG | fixed-seed generator (timing only; the join step is not built) |
| Stack | 64 kB in the linker | 60 kB, placed in the second SRAM bank (`SRAM_BANK1`); ML-DSA-44 signing needs 51.8 kB, more than one 64 kB bank leaves after .data/.bss |
| Linker | SimpleLink cmd | the SysConfig `device_linker.cmd` with `--stack_size` raised and one line adding `.stack > SRAM_BANK1 (HIGH)` |

Files: `pqc_board.c` (glue), `build_pqc.sh`, `run_pqc.sh`, `sec_board.c` and `sec_uecc.c` (SHA-256, ECDSA P-256, ECDH P-256 with TI's MSP Crypto Library and with micro-ecc), `sec_vectors.h` (TI's CAVP vectors), `build_sec.sh`, `build_maxtasks.sh`, `run_maxtasks.sh`. Board parameters are environment variables (`BOARD`, `STARTUP`, `LIBDIR`, `GEN`, `STACK`), so another MSPM0 M0+ board needs a board folder with only its own values.

## If you see this

- **A running image never finishes when you poll it.** Attaching OpenOCD to this chip resets it, so repeated attaches restart the image each time. Do the whole run in one OpenOCD connection (reset run, sleep, halt, read), as `run_pqc.sh` does.
- **`Crypto_ECDH_getSharedSecret` returns `0x3C` (`CRYPTO_EC_STAT_X_COORD_TOO_LONG`).** It did so for every input form tried (big-endian bytes, byte-reversed, curve length in bytes and in words), each time after 96.8k cycles. The ECDH row for TI's library is `failed`; micro-ecc supplies the ECDH figure.
- **DSLite fails or waits while another flash runs on the same Mac.** `dsflash.sh` retries.
