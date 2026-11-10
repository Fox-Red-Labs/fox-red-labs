# Post-quantum core on the LAUNCHXL-F2800137 (C28x, 16-bit char)

Result: ML-KEM-768 (mlkem-native) and ML-DSA-44 (mldsa-native, `-DMLD_CONFIG_REDUCE_RAM`) compile with the TI C2000 compiler 25.11.1.LTS once `uint8_t` is supplied, but ML-KEM-768 key generation overran the largest stack the 36 kB RAM allows, so no known-answer test ran and no timing exists. Rows: `pqc_*` in `data/results/f2800137.csv`. Measured with C2000Ware 26.02.00.00 LTS, `-O2`. Terms: [docs/vocab.md](../../../../docs/vocab.md). 1 kB = 1,024 bytes.

| Step | Result |
|---|---|
| Stock build (`build.sh kem`, `dsa`, `core`) | Fails on the first include: `identifier "uint8_t" is undefined` (mlkem-native `src/verify.h` line 92, mldsa-native `src/ct.h` line 98, `core/pqc_sym.h` line 9). TI's C28x `stdint.h` defines no 8-bit types |
| `uint8_t` as a 16-bit `unsigned int` (the way C2000Ware's `hw_types.h` does it; `c2000-launchpad/bench/pqc/shim.h`) | Next stop: `#error No plausibly-secure implementation of mlk_zeroize available` |
| Plus `-DMLK_CONFIG_CUSTOM_ZEROIZE` and a volatile-write zeroize in the shim | Both libraries compile with no errors. ML-KEM code 8,357 words (16,714 B), ML-DSA-44 code 10,814 words |
| Link with the harness (`c2000-launchpad/bench/pqc/main.c`, plus a copy of `core/pqc_join.c` with its buffers shared) | The unmodified core keeps 16.8k words of key and test buffers (one octet per word doubles them): it does not link in 36 kB RAM. The copy shares them (4.8k words) and links; flash 29,981 words |
| Run, code in flash, stack 7,936 / 9,712 / 10,960 words | Key generation overruns each: the CPU stops at 0x3FFF08 with SP 78 words below the top of the stack. 10,960 words (21,920 B) is the largest stack that fits beside the buffers. The Arm builds peak at 14 to 15 kB |

The copy of `pqc_join.c` changes only buffer storage (the key, ciphertext and secret buffers of the known-answer test and the timing are one shared set; the join is not run) and is `c2000-launchpad/bench/pqc/pqc_join_c28.c`. The link file `c2000-launchpad/bench/pqc/pqc_flash.cmd` gives the stack the tail of RAMLS0 plus RAMLS1, which are contiguous below 0x10000.

Not done: the vectors were never reached, so whether a 16-bit-char `uint8_t` gives correct output is unknown. A port would pack two octets per word in the byte-array interfaces, not widen them. ML-DSA-44 was compile-only: mldsa-native lists 44,704 B of allocations for signing, more than the whole 36,864 B of RAM.

Evidence: `c2000-launchpad/bench/pqc/` (`attempt1-stdint.log`, `attempt2-shim-zeroize-error.log`, `attempt3-*-compile.log`, `link-mlkem.log`, `run-mlkem-overflow.log`). Build script: `build.sh` here for the compile checks; the linked image used `c2000-launchpad/bench/build.sh` with `LNK_FLASH=c2000-launchpad/bench/pqc/pqc_flash.cmd STACK=0x2ad0` and `--c99 --preinclude=shim.h -DMLK_CONFIG_CUSTOM_ZEROIZE`.
