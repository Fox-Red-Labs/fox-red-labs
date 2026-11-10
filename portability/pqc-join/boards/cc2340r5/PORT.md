# Port: post-quantum known-answer tests and timing on the LP-EM-CC2340R5 (Cortex-M0+, 48 MHz, 36 kB RAM)

ML-KEM-768 and ML-DSA-44 (built with `-DMLD_CONFIG_REDUCE_RAM`) both pass NIST's known-answer vectors on the CC2340R52 and run in its 36 kB of RAM, ML-DSA-44 signing included.

Terms: [docs/vocab.md](../../../../docs/vocab.md). 1 kB = 1,024 bytes. The core is `portability/pqc-join/core`; this folder holds only the board glue. Tools: SimpleLink Low Power F3 SDK 9.21.00.36, TI Arm Clang 5.1.1, SysConfig 1.26.3. Measured 2026-11-09. Evidence: cc2340r5/bench/pqc/ (three flashes per image), rows `pqc_*` in `data/results/cc2340r5.csv`.

| File | What |
|---|---|
| `build.sh kem\|dsa\|dsa-nosign [-Os\|-O3]` | Builds one image; the stack is all the RAM the static data leaves (`STACK=<bytes>` to fix it). Build only |
| `run.sh <image folder> <log prefix> [runs]` | Flashes with DSLite and captures the serial report per load. Run it under the probe lock |
| `pqc_board.c` | Board glue: SysTick cycle counter, UART2 output at 115200, stack painting. Prints the same `R,pqc,...` lines as the other boards |
| `pqc.syscfg` | The bench's base configuration (LED, backchannel UART) |

## Results (median of 3 flashes, 3 calls each; all agree within 1 part in 10,000)

| Step | -Os | -O3 | Stack at -Os | Stack at -O3 |
|---|---|---|---|---|
| ML-KEM-768 keygen | 42.0 ms | 41.5 ms | 14.1 kB | 14.9 kB |
| ML-KEM-768 encaps | 49.9 ms | 49.6 ms | 17.2 kB | 20.5 kB |
| ML-KEM-768 decaps | 60.8 ms | 60.5 ms | 18.2 kB | 19.3 kB |
| ML-DSA-44 keygen | 141 ms | not built | 13.3 kB | |
| ML-DSA-44 sign (fixed message) | 514 ms | not built | 14.6 kB | |
| ML-DSA-44 verify | 164 ms | not built | 11.2 kB | |

- ML-DSA-44 signing over 16 different messages: median 723 ms, shortest 307 ms, longest 2,531 ms (rejection sampling).
- Library code: ML-KEM-768 10.2 kB at -Os and 66.8 kB at -O3; ML-DSA-44 13.3 kB. Whole images are 36 to 93 kB of the 512 kB flash.
- Static RAM: 14.0 kB for the ML-KEM-768 image, 11.1 kB for ML-DSA-44 (test vectors and buffers of the harness), leaving a 22 to 25 kB stack.
- At 48 MHz the cycle counts match the LP-MSPM0G5187 (same core, 80 MHz) within 1 percent: ML-KEM-768 keygen 2,016,231 cycles here against 1,995,022 there.
- The keygen-and-verify image (`dsa-nosign`) is kept for comparison with the G5187, which could not link signing; here its keygen is 3 percent slower (6.97 M cycles) than in the full image (6.79 M) from a different code layout.

## What is not here

- The join with a Mac (`pqc_join`) was not run: it needs 8 kB more buffers and the Mac side. This image checks and times only.
- ML-DSA-44 at -O3 was not built.
- Random bytes come from a fixed-seed generator. The CC23x0 has no general-purpose TRNG; TI's RNG driver for it draws on radio noise and was not linked in this image. Keygen and encapsulation time does not depend on the bytes.

## If you see this

- **`undefined symbol RNG_init` (or `RNG_open`) when you add `/ti/drivers/RNG` to the SysConfig file.** The CC23x0 driver library has no general-purpose RNG; the RNG driver there is the radio-noise one. Use a fixed generator for timing, or start the radio.
- **`program will not fit into available memory` with the stack set to exactly the free RAM.** The linker needs a little slack for alignment: `build.sh` steps the stack down by 64 bytes until it links (22,376 B for ML-KEM-768).
