# Can the LP-CC1354P10-1 run post-quantum key exchange (ML-KEM-768) and signatures (ML-DSA-44), and how fast?

Yes, both, in plain C with no hardware help. On the LP-CC1354P10-1 (CC1354P10, Cortex-M33 at 48 MHz), ML-KEM-768 takes 21 to 29 ms per step and ML-DSA-44 takes 50 ms to verify and 154 ms on average to sign. The full exchange with a Mac over USB serial works end to end: the board makes its key pair from its hardware random-number generator, agrees a key with the Mac, and decrypts the Mac's encrypted line. Every step passes NIST's known-answer vectors, plus a few checks added in this repo, on the board first.

| Step | Time at 48 MHz | Stack |
|---|--:|--:|
| ML-KEM-768: make a key pair | 20.8 ms | 14.7 kB |
| ML-KEM-768: encapsulate | 24.0 ms | 20.0 kB |
| ML-KEM-768: decapsulate | 29.1 ms | 18.7 kB |
| ML-DSA-44: make a key pair | 49.5 ms | 32.0 kB |
| ML-DSA-44: sign (mean of 16 messages; 77 to 361 ms) | 154 ms | 50.1 kB (one fixed message) |
| ML-DSA-44: verify | 50.5 ms | 30.2 kB |

Code size in flash: ML-KEM-768 80.5 kB and ML-DSA-44 106.7 kB built for speed (`-O3`, the timed build); 9.4 kB and 13.3 kB built for size (`-Os`, built, not run). Libraries: mlkem-native 2.0.0 and mldsa-native 2.0.0, unmodified.

**In plain words.** A key exchange is how two devices agree a secret key over a wire anyone can read. A signature proves a message or a firmware image came from the holder of a private key. Post-quantum (also called quantum-safe) means a future quantum computer cannot break it. ML-KEM is NIST's post-quantum key exchange standard (FIPS 203, formerly Kyber); 768 is its middle security level. One side makes a key pair and sends the public half; the other side *encapsulates*: it makes a random secret, locks it with that public key and sends the result (the ciphertext); the first side *decapsulates* it with its private key. Both now hold the same 32-byte secret. ML-DSA is NIST's post-quantum signature standard (FIPS 204, formerly Dilithium); 44 is its smallest level. A known-answer test (KAT) runs the code on NIST's fixed inputs and checks the outputs byte for byte. The TRNG is the chip's hardware true random-number generator. A *load* is one flash of the image onto the board followed by one run. The *stack* is the RAM a function uses for its local variables while it runs. NoRTOS means the program runs without an operating system.

Other terms: [docs/vocab.md](../../docs/vocab.md). Board FAQ: [cc1354p10/FAQ.md](../FAQ.md). 1 kB = 1.0 kB. Measured 2026-10-08 on macOS 15.7 (Intel), CCStudio 21.0.1, TI Arm Clang 5.1.1.LTS, SimpleLink CC13xx/CC26xx SDK 8.33.00.16 (NoRTOS), OpenSSL 3.6.4.

Contents: [ML-KEM-768 numbers](#how-were-the-lp-cc1354p10-1-ml-kem-768-numbers-measured) · [ML-DSA-44 numbers](#how-were-the-lp-cc1354p10-1-ml-dsa-44-numbers-measured) · [the exchange](#what-does-the-post-quantum-exchange-between-the-lp-cc1354p10-1-and-a-mac-look-like) · [the TRNG hang](#why-did-the-exchange-stop-at-join-making-an-ml-kem-768-key-pair-on-the-lp-cc1354p10-1) · [how to run it](#how-do-i-run-the-post-quantum-tests-on-the-lp-cc1354p10-1) · [files](#which-files-make-up-the-lp-cc1354p10-1-post-quantum-tests)

## How were the LP-CC1354P10-1 ML-KEM-768 numbers measured?

| Step | Cycles (median of the 3 load medians) | Time at 48 MHz | Stack | Basis |
|---|--:|--:|--:|---|
| Key generation | 996,527 | 20.8 ms | 14.7 kB | three loads |
| Encapsulation | 1,152,209 | 24.0 ms | 20.0 kB | three loads |
| Decapsulation | 1,398,195 | 29.1 ms | 18.7 kB | three loads |

The board was loaded three times and each load ran every step three times. The three load medians agree within 0.01 percent (key generation 996,518 to 996,612). A fourth load, with the TRNG fix and ML-DSA-44 added, read 996,625, 1,152,311 and 1,398,035. Clock: the CPU's own cycle counter (SysTick, the Arm core's built-in timer, on the CPU clock). As a check, SysTick and the Cortex-M33's DWT cycle counter (a counter in the core's debug block) read the same count over a test loop on each load: 180,024 to 180,029. The fixed NIST seed was used, so the same keys come out each time. Stack is measured by filling it with a pattern before each call and finding the deepest overwritten word after, net of the calling functions; the key buffers are static and not counted. Flash is from the linker maps (not kept in the repo; the byte counts are in the CSV rows): `mlkem_native.o` (ML-KEM-768 and its SHA-3) is 80.5 kB at `-O3` and 9.4 kB at `-Os`. Rows: `data/results/cc1354p10.csv`, tests `pqc_mlkem768_*`, `pqc_stack_peak`, `pqc_flash_bytes`.

For scale: TI's MSP Crypto Library publishes 27, 31 and 36 ms for the same three steps on the 80 MHz Cortex-M0+ MSPM0G3519 (TI's figures, not measured here). On cycles, this build used 15 to 21 percent fewer than TI's published figures for the MSPM33C321A (a Cortex-M33) at 80 MHz, which are 1.26, 1.43 and 1.65 million; the two differ in library, compiler and flash wait states, so read it as the same class, not a ranking.

## How were the LP-CC1354P10-1 ML-DSA-44 numbers measured?

| Step | Cycles | Time at 48 MHz | Stack | How |
|---|--:|--:|--:|---|
| Key generation | 2,374,548 | 49.5 ms | 32.0 kB | median of 3 calls, fixed NIST seed |
| Sign, one fixed message | 4,964,477 | 103.4 ms | 50.1 kB | median of 3 calls |
| Sign, 16 different messages | mean 7,406,767; median 6,233,336 | mean 154 ms; 77 to 361 ms | | one call each |
| Verify | 2,421,949 | 50.5 ms | 30.2 kB | median of 3 calls |

One load, the same clock, stack painting and build as ML-KEM. Before timing, the board passed six checks: NIST's ACVP key generation vector (test 1), one hedged and one deterministic signing vector (tests 194 and 7, signature byte for byte), one signature NIST marks valid (test 11, accepted) and one it marks invalid (test 9, rejected), and this repo's own round trip (its signature verifies, a flipped message bit does not). The vectors and where they come from: [portability/pqc-join/kat/SOURCE.md](../../portability/pqc-join/kat/SOURCE.md).

**Why signing time varies.** ML-DSA signing makes a candidate signature and throws it away if a check fails (rejection sampling), so the number of tries depends on the message. Over 16 messages the fastest sign took 3.7 million cycles and the slowest 17.3 million. Verify always takes the same time. The timed signs use deterministic signing (the random input set to zero, which FIPS 204 allows), so a rerun gives the same times; a product would use hedged signing with fresh random bytes, which costs the same.

**Stack.** Signing needs 50 kB of stack at once, so this image sets a 64 kB stack. mldsa-native keeps its working buffers on the stack by default; its own header lists 43.7 kB of buffers for signing. Its `MLD_CONFIG_REDUCE_RAM` option trades speed for RAM; build sizes and the run left for the bench are under [ML-DSA-44 with MLD_CONFIG_REDUCE_RAM](#ml-dsa-44-with-mld_config_reduce_ram-expectation-build-sizes-and-the-run-left-for-the-bench).

Flash: `mldsa_native.o` (ML-DSA-44 with its own copy of SHA-3) is 106.7 kB at `-O3` and 13.3 kB at `-Os`. The whole `-O3` image with both algorithms and the test vectors is 266.6 kB; the `-Os` image is 84.7 kB (built, not run).

For scale: TI publishes 22, 133 and 23 ms for ML-DSA-44 key generation, sign (mean of 16) and verify on the 160 MHz MSPM33C321A, and 84, 423 and 98 ms on the 80 MHz MSPM0G3519 (TI's figures, not measured here). Neither TI part is measured here.

## What does the post-quantum exchange between the LP-CC1354P10-1 and a Mac look like?

The board offers its public key, the Mac encapsulates with OpenSSL and sends the ciphertext, both sides show a key check (the first 4 bytes of the SHA-256 of the derived AES key), and the Mac sends one encrypted line that the board decrypts and prints. From the run on 2026-10-08:

```text
key check  Mac d01a2f61  board d01a2f61  MATCH
board> DECRYPTED (76 bytes, tag ok): Hello from the Mac: this line was encrypted with a key agreed by ML-KEM-768.
```

Each step, timed on the board (cycle counter) and on the Mac (wall clock):

| Step | Where | Time | Bytes on the wire |
|---|---|--:|--:|
| 64 random bytes from the TRNG | board | 20.2 ms | |
| ML-KEM-768 key pair from them | board | 20.9 ms | |
| Board sends its 1,184-byte public key | wire | 207 ms (computed) | 2,389 |
| Mac encapsulates (OpenSSL command line, includes starting it) | Mac | 58.6 ms | |
| Mac sends the 1,088-byte ciphertext | wire | 191 ms (computed) | 2,197 |
| Board decapsulates, derives the AES key, sends the key check | board | 29.8 ms | 27 |
| Mac sends one encrypted line (104 bytes) | wire | 20 ms (computed) | 229 |
| Board decrypts it (AES-128-GCM in software) and answers | board | 2.5 ms | 22 |

On the Mac's clock, from receiving the public key to the board's answer took 346 ms. In total 4,864 characters crossed the wire for 2.3 kB of payload: the framing writes every byte as two hex characters. Wire times are computed at 115,200 baud, not measured. The UART, not the chip, is the slow part: the board's own work is about 73 ms, the wire about 420 ms. For comparison, the same exchange with X25519 (the classical key exchange in common use today, run on the Mac alone) sends 32 bytes each way, 168 bytes of payload in all.

The key derivation is HKDF-SHA256 (a standard way to turn a shared secret into a key) on the 32-byte shared secret, giving a 16-byte AES-128 key. The line is encrypted with AES-128-GCM, which also detects tampering. The shared secret itself is never printed. Neither library nor this code claims protection against power or fault attacks; this is a demonstration of the math and its cost, not a hardened product. Do not use it for real keys.

## Why did the exchange stop at "join: making an ML-KEM-768 key pair" on the LP-CC1354P10-1?

**Our mistake.** The image never called `TRNG_init()`. Adding it fixed the hang on the next load. Why it hung, from reading the SDK source (not traced on the board): in TI's SDK, `Board_init()` initializes only the power and GPIO drivers; every other driver's `_init()` is the application's job (TI's `aesKeyAgreement` example calls `TRNG_init()` itself). Without it, `TRNG_open()` returns NULL, and the code did not check. The first `TRNG_getRandomBytes()` call then used the NULL handle, so the driver read its state pointer from address 0 (the start of the vector table, which holds the initial stack address `0x20040000`, the first byte past the end of RAM), wrote there, and faulted. Everything before it worked because the known-answer tests and timing use fixed inputs and never touch the TRNG.

If you see a program stop at its first TRNG call on a CC13xx or CC26xx board, with the CPU in an exception handler (here the debugger showed PC `0x100006f4` and LR `0xfffffff9`; observed 2026-10-08, output not saved), do this: call `TRNG_init()` before `TRNG_open()`, and stop with a message if `TRNG_open()` returns NULL. That is the whole fix in `pqc_board.c`. With it, the 64 random bytes take 20 ms (polling mode) and the exchange completes.

## How do I run the post-quantum tests on the LP-CC1354P10-1?

The host test needs no board (a C compiler, Python with the `cryptography` package and OpenSSL 3.5 or later). It builds the board code for the Mac, runs both sets of known-answer tests and the whole exchange against `mac_side.py`, and prints `pqc-join host test: OK`:

```bash
portability/pqc-join/test/run.sh
```

Build the board image (touches no hardware; 19.5 s on 2026-10-08 with the SysConfig output already generated, longer the first time):

```bash
cc1354p10/pqc-join/build.sh -O3
```

Loading and running it needs the board on a USB port the Mac can power-switch with `uhubctl` (a command-line tool that turns USB hub ports off and on), because this board's debugger connects only just after power-on; see the board FAQ, [the debugger will not connect](../FAQ.md#the-debugger-will-not-connect-to-the-board-once-a-program-is-running-how-do-you-get-back-in) and [power-cycling from the Mac](../FAQ.md#can-you-power-cycle-the-lp-cc1354p10-1-from-the-mac-so-loads-work-without-a-replug). `run.sh` power-cycles the port, backs up flash and the CCFG page (the chip's boot settings) to a folder outside the repo, loads the image, runs `mac_side.py` against the UART, then loads the image you name to leave running and reads flash back against the backup. Replace `20-4:1` with your hub and port from `uhubctl`, and the path with the `.out` file you want left on the board:

```text
UHUB=20-4:1 RESTORE_IMG=<path to the .out to leave running> cc1354p10/pqc-join/run.sh -O3
```

The run on 2026-10-08 took 2 minutes 22 seconds: 22 s to back up 1 MB of flash, 18 s on the board, then the restore and read-back. The loader leaves the 8 kB flash sector at `0xFC000` (an application's saved settings) different from the backup after loading a large image; `run.sh` rewrites that sector from the backup and compares the whole 1 MB again. It prints `READBACK MATCHES BACKUP` when the board is back as it was.

## Which files make up the LP-CC1354P10-1 post-quantum tests?

`pqc_board.c` (UART, timer, TRNG, stack painting), `pqc.syscfg`, `build.sh`, `run.sh`, `ccs_pqc_session.py` (backup, load, restore, read-back). Shared code: [portability/pqc-join/](../../portability/pqc-join/README.md) (`core/pqc_join.c` for ML-KEM and the exchange, `core/pqc_sign.c` for ML-DSA). Libraries: [third-party/mlkem-native/](../../third-party/mlkem-native/SOURCE.md) and [third-party/mldsa-native/](../../third-party/mldsa-native/SOURCE.md).

## ML-DSA-44 with MLD_CONFIG_REDUCE_RAM: expectation, build sizes and the run left for the bench

**Expectation, written 2026-10-20 before any lowram build.** mldsa-native's own header (`mldsa_native.h`, `MLD_TOTAL_ALLOC_44_*`) lists the working buffers a call allocates. The option is expected to cut them like this (bytes, default to `MLD_CONFIG_REDUCE_RAM`): key generation without the pairwise test 26,912 to 11,584; key generation with it 48,480 to 16,896; sign 44,704 to 13,120; verify 24,448 to 9,120. The painted stack peaks measured at the default build exceed those figures by 5.9 to 6.6 kB (keygen 32,792 against 26,912; sign 51,320 against 44,704; verify 30,904 against 24,448), so the lowram painted peaks are expected near 17.5 kB, 19.7 kB and 15.6 kB. That is a prediction for the board run, not a result. Expected cost: more cycles, because the matrix and vectors are recomputed instead of kept; the size of the slowdown is not predicted. Expected flash: a few kB more or less than the default, since the code path is the same library. Expected fit: ML-DSA-44 in the 32 kB single bank of the LP-MSPM0G5187 only if the harness's static buffers (16.7 kB of key and test buffers) shrink as well, so signing fits the stack but the harness as written probably still does not; the 128 kB-class parts (G3519, MSPM33C321A, CC1354P10) fit either way.

Build and link only (nothing touched a probe or a board): `portability/pqc-join/lowram_build.sh`, which builds the shared core with `-DMLD_CONFIG_REDUCE_RAM` through the existing board build scripts. The CC1354P10 image in this folder could not be rebuilt on 2026-10-20 because the SimpleLink F2 SDK 8.33 and SysConfig 1.26.3 were not installed on the build machine that day; its lowram build waits on that install.

**Build result, 2026-10-20 (build and link only, TI Arm Clang 5.1.1.LTS, nothing flashed).** The default and the lowram build were both rebuilt the same day, so the pairs compare like with like. The default `mldsa_native.o` sizes match the recorded rows exactly. Flash is the `mldsa_native.o` contribution; "image" is the whole linked harness. Static RAM is the linker map's `.bss` and `.data` without the reserved stack.

| Board | Opt | ML-DSA-44 flash, default | lowram | Image, default | lowram | Static RAM | Stack reserved by the harness |
|---|---|--:|--:|--:|--:|--:|--:|
| LP-MSPM0G3519 (128 kB RAM) | -O3 | 196,764 | 118,584 | 255,656 | 197,640 | 28,680 | 61,440 |
| LP-MSPM0G3519 | -Os | 16,620 | 15,482 | 68,544 | 67,784 | 28,680 | 61,440 |
| LP-MSPM33C321A (256 kB RAM) | -O3 | 109,252 | 90,246 | 243,552 | 224,544 | 27,372 | 65,536 |
| LP-MSPM33C321A | -Os | 13,592 | 12,972 | 67,696 | 67,072 | 27,372 | 65,536 |
| LP-MSPM0G5187 (32 kB RAM, 128 kB flash) | -Os | 16,620 | 15,482 | 68,256 | 67,504 | 19,204 | 13,564 at most (see below) |
| LP-MSPM0G5187 | -O3 | 196,764 | 118,584 | does not link | does not link | | |

**Does ML-DSA-44 fit?** On the G3519 and the MSPM33C321A yes, as before: the harness already reserves 60.0 and 64.0 kB of stack, above the 50.1 kB the default build was measured to use, and lowram only lowers the need (the header's allocation for signing falls from 43.7 kB to 12.8 kB). The saving there is flash (-O3 drops 19 to 40 percent of the ML-DSA code) rather than RAM headroom, and the stack the lowram build needs is not known until it is painted on the board. On the LP-MSPM0G5187, with the ML-DSA files in and the harness's static buffers unchanged (18.8 kB including the 3 kB log), the 32 kB bank leaves at most 13.2 kB for the stack; the largest stack that links was not searched beyond that figure because it is the map's whole free RAM. The header's allocation for lowram signing alone is 12.8 kB, which leaves 444 B for every call frame around it, so signing does not fit this chip with this harness; key generation with the pairwise test (16.5 kB) does not either. Key generation without it (11.3 kB) and verify (8.9 kB) are inside 13.2 kB on the header's figures only, which are floors and not peaks. The -O3 image does not link on the G5187: its code needs 157.3 kB against 128.0 kB of flash, so use -Os. The CC1354P10 image was not rebuilt (its SDK is not installed); its harness reserves a 64 kB stack in 256 kB of RAM, the same situation as the G3519. Rows: `data/results/<board>.csv`, variants `mldsa-native-lowram-ticlang-o3` and `-os` for the sizes, `waiting` for timing and painted stack on all four boards.

**What is left for the bench.** One load on the LP-CC1354P10-1 with the SDK installed, then the numbers for the other boards on their own scripts. Build, then run, with `LOWRAM=1`:

```text
LOWRAM=1 cc1354p10/pqc-join/build.sh -O3
LOWRAM=1 UHUB=20-4:1 RESTORE_IMG=<path to the .out to leave running> cc1354p10/pqc-join/run.sh -O3
```

`build.sh` reads `LOWRAM=1` and writes `out/pqc-join-O3-lowram/`; `run.sh` loads that folder when `LOWRAM=1` is set (both edited 2026-10-20, syntax-checked, not run). Add the rows with variant `mldsa-native-lowram-ticlang-o3-flash` and replace the `waiting` rows. For the MSP boards: `portability/pqc-join/lowram_build.sh g3519|m33 -O3` builds the images and `portability/boards/mspm0g3519/run_pqc.sh` runs the G3519 image.
