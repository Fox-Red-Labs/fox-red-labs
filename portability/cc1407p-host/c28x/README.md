# CC1407P host driver on the C28x (F2800137): compile shim and fake-radio bench

TI's host driver for the BP-CC1407P does not compile for the C2000 compiler as shipped, and the shim in this folder makes it compile but not talk. One preinclude header, `c28x_compat.h`, makes all 7 driver files, the bare HAL and a bench program build and link with no change to TI's files. The built image runs on the LAUNCHXL-F2800137, and what it does there is **wrong**: the driver puts different bytes on the wire than on an Arm core. The shim fixes the compile, not the wire format. Checked 2026-10-24 with C2000 compiler 25.11.1.LTS. Terms: [docs/vocab.md](../../../docs/vocab.md).

| File | What it is |
|---|---|
| `c28x_compat.h` | The shim. `--preinclude` it. It gives `uint8_t` and `int8_t` as 16-bit `unsigned char` and `signed char`, and defines `__packed__` as empty so TRX.h's `__attribute__((__packed__))` stops failing (error #1167). No run-time code |
| `hooks_fake.c` | A radio that is not there: each SPI transfer completes at once, the radio is always ready, returns zeros, and every transmitted unit is logged. Same file on the C28x and on a PC |
| `bench_main.c` | Records `sizeof` of 12 message structs, the CRC of `123456789`, then calls `TRX_Host_open`; the first transfer ends the run at `bench_done`. `-DHOST_BUILD` builds it on a PC for the reference bytes |
| `build_c28x.sh` | `build_c28x.sh <outdir>` builds the RAM image; `build_c28x.sh <outdir> noshim` compiles without the shim to show the errors |

Run it on the board: `run.sh c2000-launchpad/tools/c2krun_trx.js <outdir>/cc1407p_c28x.out`. The PC reference: build the same files with `clang -std=c99 -DHOST_BUILD` and `hal_bare.c`; the bytes it prints are in the table below.

## Result

| Check | Arm-style 8-bit byte (PC reference) | C28x on the board |
|---|---|---|
| Compile, without the shim | | 11 of 12 files fail, 750 errors (682 `uint8_t` undefined, 63 packed attribute) |
| Compile, with the shim | | 12 of 12 files, 0 errors, 51 warnings (29 shifts past 16 bits) |
| Flash | | 4.2k words = 8.4 kB (text 3,879, const 400, init 30) |
| RAM | | 4.3k words = 8.7 kB (bss 4,394, data 38; includes about 300 words of bench log), stack used 86 of 1k words |
| CRC-16 of `123456789` | 0x29B1 | 0x29B1 |
| `sizeof` of 12 message structs | 4 2 2 4 3 48 3 4 8 6 42 2 | 4 1 1 2 2 32 2 3 6 2 11 1 |
| First SPI transfer (CONFIG_LIST request) | 10 bytes: `00 04 11 00 02 32 00 00 E9 71` | 9 units: `00 11 00 00 01 00 00 00 C8` |

The CRC routine works. The first message does not: header length, request number and CRC all differ, and one unit is missing, so a real CC1407P would not answer. `TRX_Host_open` then waits for a reply that never comes, so the bench stops after the first transfer. Causes (all in TI's files, none fixable by a header):

1. Message structs are laid over the SPI buffer. A C28x struct member takes a whole 16-bit word, so a 2-byte header becomes 1 word and the layout shifts.
2. `TRX_TransactionHeader` is a 32-bit bit-field union; its `u8[4]` and `u32` views are not the same 4 wire bytes here.
3. 29 shifts such as `(1 << 16)` overflow a 16-bit `int`, so event flags above bit 15 are wrong.

A working C2000 host needs a packer that writes and reads each message field by field. That code does not exist. Nothing here ran against a radio.
