# minimp3

- Upstream: https://github.com/lieff/minimp3, `master` at commit `ea99364f61c14656440e8d77e9c233ccf3124633` (2022-06-25). A single-header MP3 (MPEG-1/2/2.5 Layer III) decoder in C, about 1,900 lines.
- Copied unmodified: `minimp3.h` and `LICENSE`. Left out: `minimp3_ex.h` (file-reading helpers that need `stdio`), the player, test vectors, fuzzing and scripts. Hashes are in `SHA256SUMS`.
- Licence: CC0 1.0 Universal (public-domain dedication), `LICENSE`.
- Arithmetic: floating point only. The decoder has no fixed-point path; `MINIMP3_FLOAT_OUTPUT` changes the output format, not the arithmetic. SSE and NEON paths exist and are off with `MINIMP3_NO_SIMD`; on a Cortex-M the plain C path runs, plus one `ssat` instruction where the compiler reports an Arm architecture of 6 or higher except Armv6-M.
- Used by: `ports/minimp3/`. The port includes the header as it is and sets its options on the compiler command line.
