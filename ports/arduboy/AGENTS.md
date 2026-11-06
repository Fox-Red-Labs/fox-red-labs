# What should an AI coding assistant know before porting an Arduboy game to a MSPM0 or MSPM33 LaunchPad?

That the Arduboy2 API is already reimplemented here over a small abstract display, input and tone interface, so a game builds unchanged and the work is the board driver, not the game. Each item below comes from a run on the Mac host or a link for a board on 2026-10-20 (`ports/arduboy/README.md`).

Terms: [docs/vocab.md](../../docs/vocab.md). **EDUMKII** is the Education BoosterPack MKII; **Sharp** is the BOOSTXL-SHARP128 screen. 1 kB = 1,024 bytes.

**How to use it.** Copy this file into your project as `AGENTS.md`. Codex, Cursor, GitHub Copilot and most agents read that name; Claude Code reads `CLAUDE.md`, so copy it under that name or add `@AGENTS.md` to yours.

## What exists

| Part | Fact |
|---|---|
| Layer | `include/Arduboy2.h`, `include/Arduino.h`, `src/arduboy2.cpp`, `src/frl_core.cpp`: the Arduboy2 subset real games call, no C++ standard library, builds with Apple clang and TI Arm Clang (`ports/arduboy/build_host.sh`, `build_target.sh`) |
| Interface a driver fills in | `include/frl/abstract.h`: `frl_display_t` (`write_lines`, RGB565 or 1 bit), `frl_input_t` (stick counts plus two buttons), `frl_audio_t` (`tone(hz, ms)`), a clock and a delay, registered with `frl_set_platform()` (`ports/arduboy/host/ab_host.cpp` is the model) |
| Screen | 128 x 128 out, streamed 4 lines (RGB565) or 8 lines (1-bit) at a time from a 1 kB game buffer; placed centered or doubled vertically with `frl_set_fit()` (`ports/arduboy/src/frl_core.cpp`) |
| Games | Snake, TileWorld, Crate Confusion in `third-party/arduboy-games/`, each run with scripted input on the host (`ports/arduboy/prove_host.sh`) |
| Cost on a chip | 5.5 to 9.6 kB flash and 3.3 to 3.8 kB RAM per game, linked with a null display (`ports/arduboy/RESULTS-ROWS.csv`) |

## Start here

| Step | Command |
|---|---|
| Prove the host build | `bash ports/arduboy/prove_host.sh` (exit 0 means every check passed) |
| Size a game for a board | `bash ports/arduboy/build_target.sh <snake or tileworld or crate-confusion> <mspm0g3507 or mspm0g3519 or mspm0g5187 or mspm33c321a>` |
| Add a game | Copy it unmodified into `third-party/arduboy-games/<name>/` with its `LICENSE` and a `SOURCE.md`, check the license first, add a scripted-input file beside the others in `scripts/`, add it to `prove_host.sh` and `fit_all.sh` |
| Write a board driver | Fill `frl_platform_t` and call `frl_set_platform()` before `setup()`; set `invert_x` and `invert_y` so right and up read higher; leave `centre_x` and `centre_y` at 0 to learn the stick centre at start |

## If you see this

| You see | Cause | Fix | Evidence |
|---|---|---|---|
| `typedef redefinition ... '__darwin_mode_t'` | A game defines its own `mode_t` (Crate Confusion) | Compile with `-DFRL_RENAME_MODE_T`; `tools/ino_prelude.h` does the rename after the system headers | `build_host.sh` |
| `use of undeclared identifier` for a function defined later in a `.ino` | The Arduino IDE adds prototypes; plain C++ does not | `python3 tools/ino_protos.py in.ino out.cpp` | `build_host.sh`, Snake |
| `invalid option: --gc_sections` | TI's linker has no such flag; it drops unused code itself | Leave it out | `build_target.sh` |
| Tones never play | `Arduboy2Audio` starts on; a game's B-button toggle may have switched it off (Snake does) | Check `Arduboy2Audio::enabled()` | Snake's source in `third-party/arduboy-games/` |
| A game reads high scores back as 0xFF | EEPROM is a 1 kB RAM image that starts erased and is not saved | Expected until a flash page is wired in | `include/EEPROM.h` |
| A frame is never drawn | `nextFrame()` returns false until `millis()` has moved by the frame period; the board clock must tick | Check the driver's `millis` | `src/arduboy2.cpp` |

## Do not

- Do not edit the copied games; fix the layer. Do not copy a game without reading its license (the left-out list with reasons is in `third-party/arduboy-games/README.md`).
- Do not put GPL code here; GPL lives only in `ports/doom/`.
- Do not claim a game "works" on a board from a link: the build is linked, nothing has run on a board.

## Where these facts come from

`ports/arduboy/README.md`, `RESULTS-ROWS.csv`; Mac (Intel), macOS, Apple clang, TI Arm Clang 5.1.1.LTS, MSPM0 SDK 2.11.00.07, MSPM33 SDK 1.04.00.00; checked 2026-10-20.
