# rp2040-doom

Doom for the Raspberry Pi RP2040 by Graham Sanderson, a port of Chocolate Doom with a rewritten renderer, a compressed game-data format (WHD and WHX) and a converter, `whd_gen`.

| Item | Value |
|---|---|
| URL | https://github.com/kilograham/rp2040-doom |
| Commit | `29a453c980918a03e40fc8b69b024e7a3bdb5dc2` (default branch, read 2026-10-20) |
| License | Mixed, from its README: code derived from Chocolate Doom keeps GPLv2; new RP2040 Doom code is BSD-3; ADPCM-XQ is BSD-3; `emu8950` derived code is MIT |
| How it is kept here | Not copied into the repository. `fetch.sh` clones it into `upstream/` (git-ignored) and checks out the commit. `patches/` holds the only changes, applied to a copy by `ports/doom/m33-whx/build.sh` |
| Not used | `doom1.whx`, a converted shareware WAD that the upstream repository itself contains. Nothing here uses or copies that file; the reader runs `whd_gen` on their own `DOOM1.WAD` |

Why the converter and not the data: see `ports/doom/LICENSING.md`, section 4.
