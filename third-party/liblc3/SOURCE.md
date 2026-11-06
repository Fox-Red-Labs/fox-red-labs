# liblc3

- Upstream: https://github.com/google/liblc3, tag `v1.1.3`, commit `96a3af0beb5487aca3b98a4b992a539a1f6d80d1`. The Low Complexity Communication Codec (LC3, Bluetooth LE Audio) in C.
- Copied unmodified: `src/`, `include/`, `LICENSE`, `README.md`. Left out: the Python bindings, tools, tests, conformance vectors, fuzzing, wasm and Zephyr folders, build files. Hashes are in `SHA256SUMS`.
- Licence: Apache License 2.0, `LICENSE`.
- Arithmetic: single-precision floating point. Upstream builds with `-ffast-math` (`meson.build`, `src/makefile.mk`). Arm helper paths (`src/ltpf_arm.h`, the saturating-store path in `src/common.h`) are guarded by `__GNUC__ >= 10`, which Clang does not satisfy; NEON paths need AArch64.
- Used by: `ports/lc3/`. The port compiles the files as they are and sets its options on the compiler command line.
