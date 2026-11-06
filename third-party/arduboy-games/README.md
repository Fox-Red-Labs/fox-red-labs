# Which open-source Arduboy games are copied here, and which were left out?

Three games, copied unmodified, each with its licence file and a `SOURCE.md`. They build against the Arduboy2-compatible layer in `ports/arduboy/` and run on the Mac with scripted input. Terms: [vocab](../../docs/vocab.md). Checked 2026-10-20.

| Folder | Game | Licence | Upstream and commit | Size |
|---|---|---|---|---|
| `crate-confusion/` | Crate Confusion, an arcade fuel-collecting game | MIT | `phoboslab/arduboy-games` at `de5e858` (see the note in its `SOURCE.md`) | 7 source files, 3 images |
| `snake/` | Snake with a start menu and a sound switch | MIT | `BennettB123/Arduboy-Snake` at `0b8dc02` | one sketch |
| `tileworld/` | TileWorld, a scrolling tile platformer demo | Apache-2.0 | `Pharap/Arduboy2DPlatformerDemo` at `b1089ee` | 8 headers and one sketch |

Each file stays as published. The build adds only what the Arduino IDE adds: a block of function prototypes in front of the first function of a `.ino` sketch (`ports/arduboy/tools/ino_protos.py`), and for Crate Confusion one compiler flag that renames the game's own `mode_t` type so it does not collide with the system type (`-DFRL_RENAME_MODE_T`).

## Left out, and why

| Candidate | Licence | Reason |
|---|---|---|
| `Nautili/ArduboyTetris`, `hackwin/arduboy-tetris`, `devvid/arduboy-snake` | none on the repository (the licence API returns not found) | Without a licence nobody may redistribute the code. |
| `thebitstick/Arduboy2-Clock` | GPL-3.0 | GPL code lives only in `ports/doom/` in this repository. |
| `jhhoward/Arduboy3D` (Catacombs of the Damned) | MIT | Allowed, deferred: it has its own `Platform.h` layer, so it is a separate port to the abstract interface rather than a game on the Arduboy2 layer. |
| `ModusCreateOrg/evade-arduboy-game` | MIT | Allowed, deferred: it includes the legacy `Arduboy.h` and plays music through its own tune code. |
| the games in `obono/ArduboyWorks` | MIT | Allowed, deferred: each one subclasses Arduboy2 (`MyArduboy`) and reaches into the AVR timers for sound, so each needs a small audio shim first. |
| Mystic Balloon | not checked | The repository was not found under the names tried. |

A licence is not enough alone: the notice must stay with the code, which is why each folder keeps its `LICENSE`.
