# mldsa-native 2.0.0

Unmodified copy of the ML-DSA (FIPS 204) C implementation from the PQ Code Package. Only the files a C build needs are here, plus the license. Never edit a file in this folder; a change is a patch file beside it or a copy under `ports/`. The post-quantum join (`portability/pqc-join/`) builds `mldsa/mldsa_native.c` as one compilation unit with `-DMLD_CONFIG_PARAMETER_SET=44 -DMLD_CONFIG_NO_RANDOMIZED_API`.

| | |
|---|---|
| Project | https://github.com/pq-code-package/mldsa-native |
| Release | v2.0.0 (released 2026-08-07) |
| Tag commit | `834a90d5e846ffa1e1611bd24e160bb2e9b86d35` |
| Archive | https://github.com/pq-code-package/mldsa-native/archive/refs/tags/v2.0.0.tar.gz |
| Archive SHA-256 | `97a7305c32b62cbcae97891823176e18ca96d9eaafc3728587d75bb113862a40` |
| Fetched | 2026-10-08 |
| License | Apache-2.0 OR ISC OR MIT. We use it under the MIT license. Every source file carries `SPDX-License-Identifier: Apache-2.0 OR ISC OR MIT`; the full text is in `LICENSE` |

## What was copied

`mldsa/mldsa_native.c`, `mldsa_native.h`, `mldsa_native_config.h`, every `.c`, `.h` and `zetas.inc` under `mldsa/src/`, and every `.c` and `.h` under `mldsa/src/fips202/`.

## What was left out

The assembly and native back ends (`mldsa/src/native/`, `mldsa/src/fips202/native/`, `mldsa/mldsa_native_asm.S`: Arm 64-bit, x86-64 and other vector code, none of which runs on our chips), the tests, proofs, examples and scripts. The portable C needs none of them; the single-file build skips them unless a back end is switched on.

## Check

`shasum -a 256` of the archive must equal the value above. Every copied file is byte-identical to the one at the same path in the archive (checked with `cmp` on 2026-10-08).
