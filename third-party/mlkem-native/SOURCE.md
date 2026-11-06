# mlkem-native 2.0.0

Unmodified copy of the ML-KEM (FIPS 203) C implementation from the PQ Code Package. Only the files a C build needs are here, plus the license. Never edit a file in this folder; a change is a patch file beside it or a copy under `ports/`. The post-quantum join (`portability/pqc-join/`) builds `mlkem/mlkem_native.c` as one compilation unit with `-DMLK_CONFIG_PARAMETER_SET=768 -DMLK_CONFIG_NO_RANDOMIZED_API`.

| | |
|---|---|
| Project | https://github.com/pq-code-package/mlkem-native |
| Release | v2.0.0 (released 2026-08-07) |
| Tag commit | `d1b2fe782888bdb761a50336012923180be7f502` |
| Archive | https://github.com/pq-code-package/mlkem-native/archive/refs/tags/v2.0.0.tar.gz |
| Archive SHA-256 | `76bf71771f09a25f30463218974ae10752d72bca34bbc470c7f6f8655f51d622` |
| Fetched | 2026-10-08 |
| License | Apache-2.0 OR ISC OR MIT. We use it under the MIT license. Every source file carries `SPDX-License-Identifier: Apache-2.0 OR ISC OR MIT`; the full text is in `LICENSE` |

## What was copied

`mlkem/mlkem_native.c`, `mlkem_native.h`, `mlkem_native_config.h`, every `.c`, `.h` and `zetas.inc` under `mlkem/src/` and `mlkem/src/fips202/`.

## What was left out

The assembly and native back ends (`mlkem/src/native/`, `mlkem/src/fips202/native/`, `mlkem/mlkem_native_asm.S`: Arm 64-bit, x86-64, RISC-V 64-bit, POWER and Arm M-profile vector code, none of which runs on our chips), the tests, proofs, examples and scripts. The portable C needs none of them; the single-file build skips them unless a back end is switched on.

## Check

`shasum -a 256` of the archive must equal the value above. Every copied file is byte-identical to the one at the same path in the archive.
