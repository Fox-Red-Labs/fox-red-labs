# BOOSTXL-SHARP128 from a TI LaunchPad: what an AI coding assistant needs to know

That the screen is a write-only 1-bit panel that wants least-significant-bit-first bytes and a VCOM flip every second, that TI ships no example for it on any current kit, and that the code in this folder drives it from five of them. Every line below cites a build, a Mac test or a run on the LP-MSPM0C1104 on 2026-10-20; the card path has not run on a board.

How to use it: copy this file into your project as `AGENTS.md` (Codex, Cursor and Copilot read that name; Claude Code reads `CLAUDE.md`, so add the line `@AGENTS.md` there). Paths below are inside the Fox Red Labs repository. Terms: `docs/vocab.md`. 1 kB = 1,024 bytes.

## The board

| Part | Fact |
|---|---|
| Panel | Sharp LS013B7DH03, 128 x 128, 1 bit, no backlight; the picture stays with the chip idle (SLAU786) |
| Screen pins | 2 power, 5 DISP, 6 select (high), 7 clock, 15 data out (`portability/boosterpacks/sharp128/README.md`) |
| Card pins | 8 select (low), 14 data in, 17 detect (low = card) (same file) |
| Pin 19 | Not used: the panel takes VCOM from a data bit (SLAU786) |
| SPI | Mode 0, panel clock 1 MHz typical and 1.1 MHz maximum; the bus carried a thousand bytes in 9.5 ms on the LP-MSPM0C1104 (`portability/boosterpacks/sharp128/README.md`) |

## Start here

| Step | Command |
|---|---|
| Test the core, no board | `portability/boosterpacks/sharp128/tests/run.sh` (prints PASS) |
| Test the player against a simulated card | `portability/boosterpacks/sharp128/tests/run_sim.sh` (three PASS lines; needs ffmpeg and macOS `hdiutil`) |
| Build | `portability/boosterpacks/sharp128/boards/build_mspm0.sh <mspm0g3519\|mspm0g3507\|mspm0l1306\|mspm0c1104> hello`; `build_mspm33.sh hello` |
| Flash and read, LP-MSPM0C1104 | `portability/boosterpacks/sharp128/boards/c1104_run.py <hello.out> <hello.map> hello_res 17 6` (backs up flash the first time) |
| Make the card's video | `python3 portability/boosterpacks/sharp128/player/video_to_frames.py in.mp4 VIDEO.BIN --fps 15`, copy `VIDEO.BIN` to the card's root |

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| Picture mirrored in each byte | The panel is LSB first; the SPI is MSB first | Leave the reversal in `sharp_lcd.c`; do not also reverse in your port | `tests/run.sh` |
| Picture fades or the glass looks stained after hours | VCOM never flipped | Call `sharp_vcom_tick(millis)` at least once a second | A 2-byte command a second on the bus |
| LP-MSPM0C1104 program never advances | SysTick does nothing on that chip | Use the TIMG14 tick in `boards/mspm0_common/port_mspm0.c` (`-DSHARP_TIMER_TIMG14`) | `hello_res.counter` rises |
| Result struct reads as stage 1, zeros | A second OpenOCD attach reset the chip | Flash, run and read in one connection | Counter rises while you wait |
| Band renderer misbehaves on the LP-MSPM0C1104 | Stack too small: the code used 352 B | Set the linker stack to 512 B | `hello_res.stack_free` above 100 |
| Player stops with error -2 | No card, a card the code cannot start, or J7 still on (LED1 shares PA22 with the card select) | Fit a card, pull J7 | `player_res.err` is 0 |
| Mount fails though the first sector reads | `sharp_sd_read` returned -1 after a good single read (fixed) | Use the current `sharp_sd.c` | `tests/run_sim.sh` |

## What fits

| Program | Flash | RAM | Source |
|---|---|---|---|
| Hello, 1-row bands, no frame buffer, LP-MSPM0C1104 | 4.9 kB | 609 B with a 512 B stack | `portability/boosterpacks/sharp128/README.md` |
| Player, Petit FatFs, 8-row reads, LP-MSPM0C1104 | 7.2 kB | 779 B with a 512 B stack | same |
| Hello with a 2 kB buffer, LP-MSPM0L1306 | 4.4 kB | 3.1 kB of 4 kB | same |
| Hello and player with FatFs, LP-MSPM0G3519 and G3507 | 12.5 kB and 10.2 kB | 5.5 kB and 5.7 kB | same |

## What ran

- Hello on the LP-MSPM0C1104: scene drawn and sent in 154 ms with 1-row bands and 41 ms with 16-row bands; one 1-row update 260 us (`portability/boosterpacks/sharp128/README.md`).
- 30 of 30 frames equal through the player on a simulated card, three library paths (`portability/boosterpacks/sharp128/tests/run_sim.sh`).
- Not run: a real card, the other four kits, power in hold mode, frames a second from a card.
