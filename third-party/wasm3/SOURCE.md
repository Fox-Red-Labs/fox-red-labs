# wasm3

- Upstream: https://github.com/wasm3/wasm3, tag `v0.9.0`, commit `0cd38327f0c721e75172f4f1eeb55854dc0517af` (2026-08-24). A WebAssembly interpreter written in C99. A newer pre-release, `v0.9.1-beta.1` (2026-09-10), exists; this copy uses the tagged release.
- Copied unmodified: the `source/` folder (the interpreter, the optional `m3_api_*` host libraries and `extra/`), `LICENSE` and `README.md`. Left out: tests, platform build files and documentation. Hashes of every copied source file are in `SHA256SUMS`.
- Licence: MIT, Copyright (c) 2019 Steven Massey, Volodymyr Shymanskyy (`LICENSE`).
- Used by: `ports/wasm3/`. The port compiles the interpreter files as they are and sets its options on the compiler command line (fixed heap, no floats on Cortex-M0+, smaller limits).
- Known defect, patched in the build copy only: `op_CallRef` in `source/m3_exec.h` has no native-stack check (it matters only with `d_m3HasTypedRefs=1`). `ports/wasm3/patches/apply.sh` copies `source/` to `ports/wasm3/out-src/` and applies `ports/wasm3/patches/0001-callref-native-stack-check.patch` there. This folder stays byte for byte as published. Report text, not filed: `ports/wasm3/UPSTREAM-ISSUE.md`.
