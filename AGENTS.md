# Notes for an AI coding assistant working in this repository

This repository holds projects, measurements and tools for Texas Instruments LaunchPad development kits, run from a Mac, with every number measured on a board and linked to its source. Start with `llms.txt` (the index of reader pages) and the `AGENTS.md` in the folder of the board you are working on; each of those files is what was learned on that one board, every line from a recorded run.

**Starting from nothing?** Follow `START.md`: it finds the board, fetches only its folder, backs it up and gets the LED doing what the person asks. `TRAPS.md` has the traps that repeat across boards, matched by exact error text; `PORTING.md` has the routes for moving code from another TI chip or another vendor's board.

**How to use it.** If you cloned this repository to work inside it, your assistant reads this file where it supports the `AGENTS.md` convention (Codex, Cursor, GitHub Copilot). Claude Code reads `CLAUDE.md`; in the published repository that file is the one line `@AGENTS.md`. If you are building your own project, copy the board folder's `AGENTS.md` into your project instead; this root file is about this repository. The person's overview is `README.md`.

## Entry commands

| Job | Command |
|---|---|
| Guided blink (kits listed in `docs/setup-coverage.md`) | `./start.sh <board id> blink --probe <serial>`, `--pattern 100,100,100,700` to change it; worked example `mspm0l1306` |
| Guided serial message (kits listed in `docs/setup-coverage.md`) | `./start.sh <board id> print-a-message --probe <serial> --message "hello"` |
| Probes on USB | `python3 tools/frl/frl.py detect` |
| Which kit is this | `python3 tools/frl/frl.py identify "<name printed on the board>"` |
| Missing tools | `python3 tools/frl/frl.py doctor --board <board id>` on a guided board; other boards `bash mac-setup/bootstrap.sh --check` |
| Back up, restore | `python3 tools/frl/frl.py backup --board <board> --probe <serial>`, `python3 tools/frl/frl.py restore <backup folder> --probe <serial>` |
| Check the results store | `python3 tools/results/check.py` |

Every other board: its `README.md` ten-minute path. Result codes `FRL-E...`: `docs/frl-errors.md`.

## A board with no folder here

`frl identify` refuses it (`FRL-E010`); do not flash it from another board's notes. Take the closest listed board in `data/boards.csv` (same `line`, then `core`) as the reference, read `TRAPS.md`, back up the flash, and prove TI's own blink example for the part builds and runs unchanged before porting anything (`PORTING.md` rule 1). Record every result, and every test that could not run, as rows in `data/results/<board>.csv`.

## Where things are

| Folder | What |
|---|---|
| `<board>/` | One folder per board, holding only what that board changes (a file a second board would copy unchanged goes in `portability/` or `third-party/`): `README.md` (the ten-minute path and the build), `AGENTS.md` (what an assistant should know), `FAQ.md` (every error hit, with the exact text and the fix) |
| `site/` | The web pages, generated from `site/data/*.json` and the data below. Never edit the HTML by hand; change the data and send a pull request |
| `data/results/<board>.csv` | Every measured number, one row each, with its source file and how it was checked (`data/README.md`). A number that is not here does not exist |
| `data/community/reports.csv` | What readers reported: builds, failures, prompts. Shown apart from the measured numbers, never merged with them |
| `data/parts.json` | Every board, kit and part a project uses: what it is, how to wire it, where to buy it |
| `docs/` | Experiments (`docs/experiments/`, one page per "would this work?" attempt), builds as text, comparisons, guides |
| `third-party/<project>/` | Upstream projects copied unmodified and pinned, each with a `SOURCE.md` (commit, license, checksums). Never edit a file here; a change is a patch file beside the code that applies it |
| `portability/<layer>/` | Board-independent layers: `core/` is the same for every chip, `boards/<board>/` holds what a board changes; `BOARD=<id> ./build.sh` picks one. The chip-to-chip tests are `portability/matrix.csv` |
| `ports/<project>/` | Code from other ecosystems on TI boards: `common/` vendor-free with a host test, one folder per TI family for the glue (ESPHome platforms in `ports/esphome-ti/`). Every port with its status: `docs/ports.md`, generated from `site/data/ports.json`; add a port there the day it is tried, failures too |
| `docs/builds/<build>.md` | One file per build: parts, pin table, steps, one section per board, each opening with its `git sparse-checkout add` line |
| `tools/` | `frl` (the first-use tool behind `start.sh`: detect, identify, back up, build, flash, verify, restore) and `results/check.py`. The page renderers, page checks and bench scripts stay with the maintainers |
| `mac-setup/` | The Mac setup check and the disk space each install takes |

## Rules that hold for every change

1. **Say what happened, not what should happen.** "Builds", "flashes", "ran", "verified": whichever was seen. A result nobody read off the board is `self-report`, never "works".
2. **Every number has a row and a source.** New measurements go in `data/results/<board>.csv` with `source` and `checked` set; three runs before a median. `python3 tools/results/check.py` must print OK.
3. **Pages come from data.** Change `site/data/*.json`, a board's Markdown or `data/`. The pages are regenerated from that data by tools that are not in this copy, so send the data change in a pull request.
4. **Write for a reader with an error message.** Exact error text in backticks, versions and dates on anything that can rot, which machine (macOS Intel, Apple Silicon, Windows, Linux), commands in fenced blocks one per block.
5. **Trademarks as adjectives** ("LaunchPad development kit"), never plural or possessive. Other vendors' products: facts from public sources or nothing.
6. **1 kB = 1,024 bytes**, and say so once on a page with memory numbers.
7. **Nothing private.** No user paths, no home network names, no keys, no customer names. Keep them out of every file.

## What a contribution looks like

A timed build on your own board (an issue with your hands-on and waiting minutes and whether the repo's number matched), a failure with its exact error text, or a prompt that worked. `CONTRIBUTING.md` has the forms; `data/community/README.md` has what becomes of a report. A pull request that adds a board folder copies the shape of an existing one.
