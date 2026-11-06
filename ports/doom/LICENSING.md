# What license terms apply to the Doom port, and what may ship with it?

**Short answer.** Three separate things, three separate terms. The engine is GPL-2.0, so a port is GPL-2.0 and its source goes with any binary. The shareware game data (DOOM1.WAD, 4.0 MB) may be copied freely but not changed, sold or repackaged, so it is never committed here and the reader supplies it. Freedoom, the free replacement data, has a permissive license, but its phase 1 file is 27.5 MB, 3.4 times the 8 MB flash on the LP-MSPM33C321A. Read 2026-10-20 from the files linked below. This is a reading of the texts, not legal advice.

Terms: [vocab](../../docs/vocab.md). 1 kB = 1,024 bytes and 1 MB = 1,048,576 bytes throughout.

## 1. The engine

| Item | Text | Source |
|---|---|---|
| id Software's 1997 source release | "The DOOM source code is released for your non-profit use. You still need real DOOM data to work with this code." The same README ends: "Copyright (c) ZeniMax Media Inc. Licensed under the GNU General Public License 2.0." The repository carries the GPL v2 text as `LICENSE.TXT` | https://github.com/id-Software/DOOM/blob/master/README.TXT and https://github.com/id-Software/DOOM/blob/master/LICENSE.TXT |
| doomgeneric (the portable engine used here, based on Chocolate Doom) | Repository `LICENSE` is the GNU GPL version 2. Each engine file keeps the id Software 1993-1996 and Simon Howard 2005-2014 copyright headers. Pinned commit `e257a09527062bfb986b6e5e0780839b6cbd5b71` | https://github.com/ozkl/doomgeneric |
| rp2040-doom (read for its RAM techniques, no code copied) | Documents the squeezes used on a 264 KB part: "A straight RP2040 compile of the Chocolate Doom source code ... requires 300K of static mutable data and a minimum of about another 700K of Doom zone memory" | https://kilograham.github.io/rp2040-doom/speed_and_ram.html |

What follows for this repository: everything under `ports/doom/` is GPL-2.0 (the `LICENSE` file beside this one). Source is published with any binary built from it. No GPL engine code is placed outside `ports/doom/`. The engine source is not copied into the repository: `fetch.sh` downloads the pinned commit and applies `patches/`, so the patch files are the only engine-derived text kept here, and they carry the same license.

## 2. The shareware data

The file is `DOOM1.WAD` version 1.9. Identification: SHA-1 `5b2e249b9c5133ec987b3ea77596381dc0d6bc1d`, MD5 `f0cefca49926d00903cf57551d901abe`, 4.0 MB, 1,264 lumps (hashes: https://www.wad-archive.com/wad/5b2e249b9c5133ec987b3ea77596381dc0d6bc1d; size and lump count: https://doomwiki.org/wiki/DOOM1.WAD).

The terms that ship with it are the "Limited Use Software License Agreement" of the shareware release (copy at https://github.com/wasm3/pywasm3-doom-demo/blob/main/doom1.wad-LICENSE). The clauses that matter here, quoted:

- Clause 1: "You may not: modify, translate, disassemble, decompile, reverse engineer, or create derivative works based upon the Software."
- Clause 2: "You must treat the Software like any other copyrighted material, except that you may make copies of the Software to give to other persons. You may not charge or receive any consideration from any other person for the receipt or use of the Software without receiving ID's prior written consent as specified in the VENDOR.DOC file."
- The game's own text screen: "DOOM, Knee-Deep in the Dead can be freely distributed. Disk vendors should refer to the vendor information text file (vendor.doc) in your DOOM directory." (https://www.wad-archive.com/wad/5b2e249b9c5133ec987b3ea77596381dc0d6bc1d)

VENDOR.DOC was not found in this pass, so the conditions for vendors and bundlers are unknown.

What follows:

1. The WAD is never committed. `ports/doom/.gitignore` excludes `*.wad`, `*.WAD` and the `wad-cache/` folder; `fetch.sh` downloads the file for the reader's own machine and checks the SHA-1 above.
2. The port reads the file as shipped. A converted or repacked WAD (rp2040-doom ships a converter, not the converted file) can be read as a derivative work under clause 1, so this port does not convert it.
3. The reader copies the file onto the board. A kit or board sold with the WAD in its flash would be "consideration" under clause 2. It needs id Software's written consent, so this repository does not do it.
4. The demo a reader runs is the unchanged game.

## 3. Freedoom

| Item | Fact | Source |
|---|---|---|
| License | BSD 3-clause style: copyright "2001-2024 Contributors to the Freedoom project", redistribution allowed with the notice and disclaimer, no use of the project name to endorse derived products | https://raw.githubusercontent.com/freedoom/freedoom/master/COPYING.adoc (the same text is `COPYING.txt` inside the release) |
| Release | v0.13.0, `freedoom-0.13.0.zip` 23.0 MB, SHA-256 `3f9b264f3e3ce503b4fb7f6bdcb1f419d93c7b546f4df3e874dd878db9688f59` | https://github.com/freedoom/freedoom/releases/tag/v0.13.0 |
| Phase 1 data | `freedoom1.wad` 27.5 MB, SHA-256 `7323bcc168c5a45ff10749b339960e98314740a734c30d4b9f3337001f9e703d` (unzipped from the release) | same release |
| Phase 2 data | `freedoom2.wad` 27.5 MB | same release |

The phase 1 file is 3.4 times the 8 MB QSPI flash, so it does not fit the board without cutting it down. The license allows modified copies, so a trimmed Freedoom data set is the legal route if a shipped kit is ever wanted; building it is not part of this wave.

## 4. The compressed data format (m33-whx target): converter only

Source: the rp2040-doom README (https://github.com/kilograham/rp2040-doom). It says "`doom1.whx` is included in this repository, otherwise you need to build `whd_gen`" and "To generate a WHX file (you must use this to convert DOOM1.WAD to run on a 2M Raspberry Pi Pico): whd_gen <wad_file> <whx_file>". So upstream ships the converter and, in its repository, one converted shareware file. Its code is mixed: Chocolate Doom derived files keep GPLv2, new RP2040 files are BSD-3, ADPCM-XQ is BSD-3, emu8950 derived code is MIT (same README, "RP2040 Doom Licenses").

Rule for this repository: ship only the converter. The pinned fetch script and build recipe are here (`third-party/rp2040-doom/`, `m33-whx/convert.sh`); the reader runs `whd_gen` on their own `DOOM1.WAD`, locally, into the git-ignored `wad-cache/DOOM1.whx`. No original and no converted WAD is committed, and the `doom1.whx` inside the upstream repository is not used or copied. The reason is clause 1 of the shareware license (section 2): a WHX file is a re-encoding of the game's data (compressed, textures renumbered, sounds and music re-encoded), which can be read as a derivative work. The converter is code, not data, and carries no such term. `.gitignore` excludes `*.whx`, `*.whd`, `*.wad` and `wad-cache/`.

Checksum of the converter's output for the shareware 1.9 file with this pinned converter, so a reader can confirm theirs: SHA-1 `066413a38303c3d36471ad887b798bc91275793b` (deterministic over two runs; it differs from the file upstream ships, which was built at another time).

## 5. The name

The shareware README writes the title as DOOM(TM) (https://www.classicdoom.com/doominfo.htm, copy of the v1.8 README). Rules for this repository: the text says the board "runs Doom", there is no Doom logo or artwork in any file, and Doom is not part of any project, package or repo name beyond the folder `ports/doom/`. No frame captured from the game is committed.
