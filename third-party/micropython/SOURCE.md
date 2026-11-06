# Source

MicroPython, https://github.com/micropython/micropython, tag `v1.29.0`, commit `0fd6c573e` (the v1.29.0 release). MIT licence (`LICENSE`, Copyright Damien P. George and contributors). Copied 2026-10-20 from a shallow clone of the tag, unmodified: every kept file is byte-identical to the tag (checked with `diff -r` against the clone).

Kept: `py/` (core), `shared/` (readline, REPL, runtime helpers, libc pieces), `extmod/` (the `machine`, `time`, `struct` and other extension modules), `drivers/bus/` (software SPI), `lib/libm`, `lib/libm_dbl`, `lib/uzlib`, `lib/re1.5`, `lib/crypto-algorithms` (small libraries the extension modules include), `mpy-cross/` and `tools/*.py` (the build helpers), `ports/minimal/` (the reference port the layout follows), `README.md`, `LICENSE`.

Left out: every other port, `tests/`, `docs/`, `examples/`, `logo/`, the other `lib/` submodules (CMSIS, lwIP, mbedtls, BTstack, TinyUSB and the vendor SDKs; none is used by this build) and `drivers/` apart from `bus/`.

Tree digest of this folder (sha256 over the sorted per-file sha256 list, first 16 hex): see `SOURCE.sha`.

Used by: `ports/micropython/` (MSPM0G3519 and MSPM0G3507 port). No patch is applied. Nothing in this folder is edited; a change would be a patch file beside it or a copy under `ports/micropython/`.
