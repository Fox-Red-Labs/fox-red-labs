# Post-quantum secure join: the board-independent C code

Terms: [docs/vocab.md](../../docs/vocab.md). 1 kB = 1,024 bytes. Board that runs it: [cc1354p10/pqc-join/](../../cc1354p10/pqc-join/README.md). The Mac side is tools/pqc_join/mac_side.py.

A board and a Mac agree on a key with ML-KEM-768, derive an AES-128 key from it with HKDF-SHA256, and the Mac sends one AES-128-GCM encrypted line that the board decrypts. Beside it, the board signs and verifies with ML-DSA-44. This folder holds the code that is the same on every board.

| Path | What |
|---|---|
| `core/pqc_join.c`, `.h` | Known-answer tests, timing with stack painting, and the join. A board supplies a `pqc_hal_t` (cycle counter, random bytes, print, read a line, stack paint) |
| `core/pqc_sign.c`, `.h` | ML-DSA-44 known-answer tests and timing (keygen, sign, verify, and signing 16 messages for the spread), same `pqc_hal_t` |
| `core/pqc_sym.c`, `.h` | SHA-256, HMAC, HKDF, CRC-32, AES-128, AES-128-GCM (about 250 lines, no libraries) |
| `core/pqc_kat_data.h`, `core/pqc_mldsa_kat_data.h`, `kat/` | NIST ACVP vectors for ML-KEM-768 and ML-DSA-44 and where they came from (`kat/SOURCE.md`) |
| `host/board_host.c` | The board side compiled for the Mac, pipes for the UART |
| `test/run.sh` | The host test: builds, checks the symmetric code against OpenSSL, runs the ML-KEM-768 and ML-DSA-44 known-answer tests, runs the whole exchange against `mac_side.py` |
| `boards/mspm0g3507/` | Build-only check for the LP-MSPM0G3507 (never flashed) |
| `boards/nucleo-u083rc/` | Build-only images for the NUCLEO-U083RC (ST GCC 14.3.1, 40 kB RAM): `build.sh -O3 kem` (ML-KEM-768, 24.5 kB stack), `dsa-lowram` (ML-DSA-44 with `MLD_CONFIG_REDUCE_RAM`, 27.5 kB stack); the default ML-DSA-44 build overflows the RAM (`dsa-default <stack>` prints the shortfall). Never flashed |
| `boards/am243x/` | The LP-AM243 (Cortex-R5F, 800 MHz, 2 MB SRAM), loaded to RAM and run: the default build of both algorithms in one image with a 96 kB stack passes the known-answer tests in 3 of 3 loads; ML-KEM-768 0.95, 1.08 and 1.34 ms, ML-DSA-44 key pair 2.4 ms, sign 5.2 ms, verify 2.5 ms; stack peaks 14 to 49 kB. Build: `boards/am243x/build.sh` |
| `boards/cc2340r5/` | The LP-EM-CC2340R5 (Cortex-M0+, 48 MHz, 36 kB RAM), flashed and run: ML-KEM-768 and ML-DSA-44 (`MLD_CONFIG_REDUCE_RAM`, signing included) pass the known-answer tests; ML-KEM-768 takes 42, 50 and 61 ms at -Os. See its `PORT.md` |

ML-KEM itself is `third-party/mlkem-native/` (v2.0.0, unmodified), ML-DSA is `third-party/mldsa-native/` (v2.0.0, unmodified, built with `-DMLD_CONFIG_PARAMETER_SET=44 -DMLD_CONFIG_NO_RANDOMIZED_API`). Wire format: `core/pqc_join.h`. Run the host test with `portability/pqc-join/test/run.sh` (needs a C compiler, Python 3 with `cryptography`, and OpenSSL 3.5 or newer; macOS's built-in `/usr/bin/openssl` is LibreSSL and cannot do ML-KEM). It prints `pqc-join host test: OK`.

## LP-MSPM0G3507, build only

`boards/mspm0g3507/build.sh -Os 12288` compiles and links the same core with TI Arm Clang 5.1.1 and the MSPM0 SDK 2.11.00.07. Result: flash 31.4 kB (mlkem-native code 9.5 kB and read-only data 448 B; the rest is the NIST vectors, the symmetric code and the SDK), RAM 28.4 kB with a 12 kB stack. The link succeeds with a stack up to 15.5 kB and fails at 16 kB, because this harness keeps 16.7 kB of key and test buffers in static RAM. ML-KEM-768 used 15.1 to 20.5 kB of stack on the Cortex-M33 at -O3, so with this harness the 32 kB G3507 is **Unknown (suspected: Chip limit)**: the library alone fits, the harness as written does not leave room for the stack. Sharing the test buffers with the join buffers would free 9.4 kB; not run. The image was never flashed. Its random-number function is a fixed stand-in because the G3507 has no TRNG.
