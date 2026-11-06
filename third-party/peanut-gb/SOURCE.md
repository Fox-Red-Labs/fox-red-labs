# Peanut-GB

- Upstream: https://github.com/deltabeard/Peanut-GB, commit `d0bcca771c83a2638c93a9ae61f3d51f226dc905` (2026-09-21), version 1.3.0.
- Copied unmodified: `peanut_gb.h` and `version.all` (hashes in `SHA256SUMS`). Left out: the SDL2, benchmark and debug examples, the test ROM headers and screenshots.
- Licence: MIT, Copyright (c) 2018-2023 Mahyar Koshkouei. The upstream repository has no separate LICENSE file; the licence text is at the top of `peanut_gb.h`, and `LICENSE` here is that text extracted so the notice travels as a file. The header says two small parts come from SameBoy (MIT, Copyright (c) 2015-2019 Lior Halphon).
- Used by: `ports/peanut-gb/`.
- ROMs are not copied. `ports/peanut-gb/fetch_roms.sh` downloads two freely licensed test ROMs and checks their hashes.

## Known defects, patched in the build copy only

A security review found two bugs where ROM content indexes a fixed array (both can be hit by a crafted ROM):

1. MBC3 real-time-clock access: `cart_ram_bank - 0x08` indexes a 5-byte array on read and on write, but any bank value of 0x08 or more is accepted, so banks 0x0D to 0xFF run past the array. The clock registers are 0x08 to 0x0C only.
2. In `gb_init`, the ROM-size byte at 0x148 and RAM-size byte at 0x149 index tables of 9 and 6 entries with no range check.

This folder stays byte for byte as published. `ports/peanut-gb/patches/0001-bounds-checks.patch` is applied to a copy (`ports/peanut-gb/out/peanut-gb-patched/`) by `ports/peanut-gb/patches/apply.sh`, which every build script calls. The front end also checks both header bytes itself and refuses the ROM with a message. Tests: `bash ports/peanut-gb/tests/run_tests.sh`. Upstream issue text, not filed: `ports/peanut-gb/UPSTREAM-ISSUE.md`.
