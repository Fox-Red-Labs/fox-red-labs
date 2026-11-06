# Changing the touch games on the LP-MSPM0G3519: notes for an AI coding assistant

The games draw only through `hal_lcd_fill` and `hal_lcd_blit`, read the finger only through `hal_touch`, and keep no frame buffer: a 320 x 480 RGB565 frame is 300 kB, more than any RAM on these boards. Copy this file into your project as `AGENTS.md`. Paths in backticks are in the Fox Red Labs repository under `ports/touch-games/`. Terms: [vocab](../../docs/vocab.md). 1 kB = 1,024 bytes. **Gesture** means tap, long press or swipe.

## Start here

| Step | Command | Check |
|---|---|---|
| Host test | `bash ports/touch-games/host/run_host.sh` | prints `215 checks, 0 failed` and rewrites `proof/2048.png`, `proof/mines.png` |
| Build the board | `bash ports/touch-games/build.sh g3519` (also `g3507`, `m33`) | prints the size line; G3519 build 11.0 kB flash, 5.4 kB RAM (linker size report) |
| Flash and read | `python3 ports/touch-games/tools/g3519_lock_run.py <image.out> --uart 20 --secs 1 --mdw 0x20200c00:29` (run with the repository's bench Python) | console prints `BENCH ...` lines; `tg_stats` at 0x20200C00 holds the same numbers |
| Start in a game | `BOOT_GAME=1 BENCH=0 bash ports/touch-games/build.sh g3519` | 9.7 kB flash; the board boots into 2048 |

## Rules the code keeps

| Rule | Why | Source |
|---|---|---|
| Chip select needs a 10 us gap after it goes high | The screen drops commands without it | `src/hal_mspm0.c`, `mspm0g3507/display-test/README.md` |
| Change the SPI frame size only with the SPI block disabled | The change is ignored while enabled | `src/hal_mspm0.c` (`frames16`) |
| Touch is software I2C on the touch pins, 5 us half bit | The chip's I2C block did not get an answer from the chip | `src/hal_mspm0.c`, `mspm0g3519/RESULTS-mspm0g3519.md` |
| Scratch buffer is 1,536 pixels; a glyph run, a Minesweeper cell (34 x 34 = 1,156) and a 2048 label must each fit | One buffer is the whole RAM budget for drawing | `src/gfx.h`, `src/mines.c` |
| Gestures fire once per touch: a swipe at 36 px, a long press at 500 ms still, a tap at lift if neither fired | A lift after a long press or swipe must not also tap | `src/gesture.c`, `host/test_host.c` (9 gesture checks) |
| The first Minesweeper dig is never a mine | Mines are placed after the first tap | `src/mines.c`, `host/test_host.c` (200 seeds) |

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| OpenOCD flash ends with `Unknown Device ID[0x0]` after a build that sleeps in `WFI` | The probe read the device ID while the core slept | Flash with DSLite (`tools/g3519_lock_run.py` falls back to it) | `Verified OK` |
| Memory read right after an OpenOCD attach shows half-run data | Each attach resets the core | Read in the same OpenOCD connection after `reset run`, a sleep and `halt` | `tg_stats` magic is `0x7C6A0003` or `0x7C6A0010` |
| `BENCH` lines missing from the console | The boot measurement finished inside a 9 s capture at 40 MHz SPI and not at 20 MHz (a 22 s capture got it) | Capture 20 s | `BENCH` lines appear |
| Taps land in the wrong place | No finger has touched the screen, so the axes are unchecked | Set `TOUCH_SWAP_XY`, `TOUCH_FLIP_X` or `TOUCH_FLIP_Y` in the build flags | A tap on a button lands on that button |

## What ran, what did not

- Ran on the LP-MSPM0G3519 (2026-10-20): the scripted measurement at 40 MHz and 20 MHz SPI, three runs at 40 MHz and two at 20 MHz with identical lines; touch chip read 0 failures in 16 reads (`RESULTS-ROWS.csv`).
- Not looked at on the screen; no finger touched the board; touch axes untested (`RESULTS-ROWS.csv`).
- Built only: LP-MSPM0G3507 (10.8 kB flash, 5.3 kB RAM of 32 kB) and LP-MSPM33C321A with a screenless stub (8.1 kB, 4.3 kB).
