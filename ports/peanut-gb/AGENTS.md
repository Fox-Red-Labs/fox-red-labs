# What an AI coding assistant should know before putting a Game Boy emulator on a MSPM0 or MSPM33 LaunchPad

That Peanut-GB needs 30.8 kB of RAM as built here, so it fits the 128 kB and 256 kB chips and only just links on the 32 kB ones, and that the ROM, not the emulator, decides flash. Each item comes from a run on the Mac or a link for a board on 2026-10-20 (`ports/peanut-gb/README.md`).

Terms: [docs/vocab.md](../../docs/vocab.md). **ROM** is a game's image file. 1 kB = 1,024 bytes.

**How to use it.** Copy this file into your project as `AGENTS.md`. Codex, Cursor, GitHub Copilot and most agents read that name; Claude Code reads `CLAUDE.md`, so copy it under that name or add `@AGENTS.md` to yours.

## What exists

| Part | Fact |
|---|---|
| Emulator | `third-party/peanut-gb/peanut_gb.h` 1.3.0, MIT, never edited (`SHA256SUMS`); builds use a patched copy in `ports/peanut-gb/out/peanut-gb-patched/` that fixes two ROM-controlled out-of-bounds indexes (`patches/0001-bounds-checks.patch`); `pgb_init` also refuses bad header bytes |
| Front end | `ports/peanut-gb/src/pgb_frontend.c`: `pgb_init(rom, len, cart_ram, cap)`, `pgb_run_frame()`, `pgb_present()`; reads buttons through `frl_read_buttons()` from `ports/arduboy/include/frl/abstract.h` |
| RAM | 30.8 kB `.data` plus `.bss`: console state about 17 kB, 8 kB cartridge RAM buffer, 5.6 kB packed picture, 1 kB of line buffers (`ports/arduboy/FIT.md`) |
| Flash | 12.5 kB of code at `-Os`, plus the ROM (`ports/arduboy/FIT.md`) |
| Controls | Stick is the D-pad, button 1 is A, button 2 is B, both together is Start, both with the stick down is Select (`src/pgb_frontend.c`) |
| Sound | Off (`ENABLE_SOUND 0`) |
| Speed switches | `PGB_SCALER_AVG`, `PGB_HIGH_LCD_ACCURACY=1`, `PGB_RAMFUNC`, `PGB_FRAME_SKIP` (`src/pgb_frontend.c`); instruction counts per step in `README.md` ("Speed on the M33") |
| Mac speed | About 4,180 frames per second alone and 1,680 with scaling, process CPU time (`ports/peanut-gb/README.md`) |

## Start here

| Step | Command |
|---|---|
| Get the test ROMs | `bash ports/peanut-gb/fetch_roms.sh` (never commit a ROM; only freely licensed ones are used) |
| Prove it on the host | `bash ports/peanut-gb/prove_host.sh` |
| Size it for a board | `bash ports/peanut-gb/build_target.sh <board> [rom or acid]` |
| Change the cartridge RAM size | `cart_ram` in `target/main_pgb.cpp`; the front end refuses a ROM whose save size is larger (`pgb_init` returns -3) |

## If you see this

| You see | Cause | Fix |
|---|---|---|
| `pgb_init failed: -3` | The ROM wants more cartridge RAM than the buffer holds | Raise the buffer (up to 32 kB) |
| `program will not fit ... .rodata` on a 128 kB chip | The ROM is larger than flash | A ROM under about 115 kB, or a chip with more flash |
| `ROM refused: ROM size code in the header ...` | The ROM header holds a size code outside Peanut-GB's tables | Not a valid ROM for this emulator |
| Black picture for 20 seconds from Tobu Tobu Girl | Its logos and intro play first; the title is near frame 1,700 | Wait, or press A and B together |
| Picture is blocky and speckled on a Sharp panel | One-bit output uses a 4 x 4 dither of 4 gray levels | Expected; a different threshold is a later change |

## Do not

- Do not commit a ROM. Do not edit `third-party/peanut-gb/peanut_gb.h`; add to `patches/` instead, and keep `bash tests/run_tests.sh` passing.
- Quote a board frame rate only from a board read: the first image read 4.7 to 7.9 frames a second at 32 MHz; the faster images are expectations (`README.md`, "Speed on the M33") until `snk.frame_cyc_*` has been read.
- Do not run a speed step at 160 MHz without checking `CPUCLK_FREQ` in the generated header; the default clock is 32 MHz.

## Where these facts come from

`ports/peanut-gb/README.md`; Mac (Intel), macOS, Apple clang, TI Arm Clang 5.1.1.LTS, MSPM0 SDK 2.11.00.07, MSPM33 SDK 1.04.00.00; checked 2026-10-20.
