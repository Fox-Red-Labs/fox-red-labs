# Portable code and fast first demos on TI development kits

Source code, build scripts and measured results for 21 development boards: 16 TI development kits and EVMs, and 5 boards from other makers on the same tests. Application code stays apart from board code, so a demo moves to another board by changing a board folder, and every number is a CSV row with its source.

Terms: [docs/vocab.md](docs/vocab.md). Errors seen on more than one board: [TRAPS.md](TRAPS.md). 1 kB = 1,024 bytes.

**Built it on your kit?** Post it in [Show and tell](https://github.com/Fox-Red-Labs/fox-red-labs/discussions/categories/show-and-tell) with its four parts: what you built or ported, what it runs on, a photo or a clip, and anything that did not work. [How to post](#how-to-post-what-you-built).

Last activity: 2026-11-11.

## Quick start: a changed blink in about ten minutes

The worked example below uses the [LP-MSPM0L1306](https://www.ti.com/tool/LP-MSPM0L1306) and a Mac (tested on macOS 15.7, Intel) with 6 GB free, `make` and Python 3.9 or later (`xcode-select --install`). Install four free tools, about 0.9 GB; CCStudio is not needed, and the SDK download needs a free myTI account. The same command drives the other kits listed under **Other boards** below, each with its own tools:

| Tool | Version tested | Where |
|---|---|---|
| MSPM0 SDK | 2.11.00.07 | [ti.com/tool/MSPM0-SDK](https://www.ti.com/tool/MSPM0-SDK) |
| SysConfig | 1.26.3 | [ti.com/tool/SYSCONFIG](https://www.ti.com/tool/SYSCONFIG) |
| TI Arm Clang | 5.1.1.LTS | [ti.com/tool/ARM-CGT](https://www.ti.com/tool/ARM-CGT) |
| xPack OpenOCD | 0.12.0-7 | [GitHub releases](https://github.com/xpack-dev-tools/openocd-xpack/releases), unpacked to `~/xpack-openocd-0.12.0-7` |

**Your computer.** Python 3.9 or later on each; per-OS folders and serial ports are in [docs/install-routes.md](docs/install-routes.md#your-computer).

| | First command | Python | Once only |
|---|---|---|---|
| macOS | `./start.sh mspm0l1306 blink` | `xcode-select --install` | nothing |
| Linux | `./start.sh mspm0l1306 blink` | `sudo apt install python3 make git` | the udev rule below |
| Windows (PowerShell) | `py start.py mspm0l1306 blink` | python.org installer | UniFlash or CCStudio, for the probe driver |

Linux udev rule, so your user can open the probe: `sudo cp ~/xpack-openocd-0.12.0-7/openocd/contrib/60-openocd.rules /etc/udev/rules.d/ && sudo udevadm control --reload && sudo udevadm trigger`. Tested: the whole route on macOS; on Linux the dry run, checks and tests in a fresh Ubuntu 24.04; Windows only in CI, no board.

Coming from STM32 HAL, Arduino, Zephyr or ESPHome: [PORTING.md](PORTING.md) has the route. Otherwise fetch the top of the repository (a few MB) and run the board's blink job:

```bash
git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git
```

```bash
cd fox-red-labs
```

```bash
./start.sh mspm0l1306 blink
```

`start.sh` adds the folders it needs (about 25 MB), names any missing tool, backs up the board's flash, builds TI's blink example, flashes it and times the LED pin through the debugger. Success is a blinking LED and a line starting `OK:`. With two boards plugged in, add `--probe <serial>` (`python3 tools/frl/frl.py detect` lists them); `--dry-run` checks the arguments without touching the board. Then change the pattern (on, off, on, off, in ms):

```bash
./start.sh mspm0l1306 blink --pattern 100,100,100,100,100,700
```

The second job prints your own text over the USB cable: `./start.sh mspm0l1306 print-a-message --message "hello"`. If no `OK:` line appears, the last line carries a code (`FRL-E...`) explained in [docs/frl-errors.md](docs/frl-errors.md).

The commands below need more than the sparse clone has, and Python 3.12 or newer (`.python-version` names the tested 3.14; `python3 -m pip install -r requirements.txt`). Add it with `git sparse-checkout add tools data mac-setup`, or clone without `--sparse` (about 500 MB).

<!-- routes:other-boards (written by tools/start/route_summary.py; do not edit by hand) -->
**Other boards:** `./start.sh <board id> <job>` drives 13 kits. With a recorded hardware run: LP-AM13E230 (`am13e230`: `blink`); LP-CC1354P10-1 (`cc1354p10`: `blink`); LP-EM-CC2340R5 (`cc2340r5`: `blink`, `print-a-message`); LP-EM-CC2755P10 (`cc2755p10`: `blink`); LP-EM-CC35X1 (`cc35x1`: `print-a-message`); LAUNCHXL-F2800137 (`f2800137`: `blink`, `print-a-message`); LP-MSPM0C1104 (`mspm0c1104`: `blink`); LP-MSPM0G3507 (`mspm0g3507`: `blink`); LP-MSPM0G3519 (`mspm0g3519`: `blink`); LP-MSPM0G5187 (`mspm0g5187`: `blink`); LP-MSPM0L1306 (`mspm0l1306`: `blink`, `print-a-message`); LP-MSPM0L2228 (`mspm0l2228`: `blink`); LP-MSPM33C321A (`mspm33c321a`: `blink`, `print-a-message`).

13 other boards with a folder have a manual path: `bash mac-setup/bootstrap.sh --check` lists the missing tools and their disk space, then follow the ten-minute path in the board folder's `README.md`. Each board's tools, probe, check, backup and recovery, and what is not yet tested: [docs/setup-coverage.md](docs/setup-coverage.md). For a kit not in the folder map, `python3 tools/frl/frl.py identify "<name printed on the board>"` returns TI's page for it. Board cores, memory and prices are in `data/boards.csv`; `site/hardware.html` compares them.
<!-- /routes:other-boards -->

Other boards on Linux and Windows: expected, not run ([docs/portability-of-the-recipes.md](docs/portability-of-the-recipes.md)).

An AI coding assistant reads `AGENTS.md` here and in the board folder; [START.md](START.md) is a prompt that runs the same steps.

## How the code moves between boards

Shared code lives once at the top level; a board folder holds only what that board changes. The rule, and the few older copies still to move: docs/repo-layout.md.

- **`third-party/<project>/`**: upstream projects copied unmodified and pinned (LVGL, CoreMark, ArduinoCore-API and others), each with a `SOURCE.md` naming the commit, license and checksums. A change is a patch file beside the code, never an edit.
- **`portability/<layer>/`**: this repository's board-independent layers. `core/` is the same for every chip; `boards/<board>/` holds the pin map, SysConfig file and generated files. `BOARD=mspm0g3507 ./build.sh` in `portability/arduino/` builds unmodified Arduino libraries and sketches for that board.
- **`ports/<project>/`**: code from another ecosystem running on TI boards. `common/` is vendor-free with a host test; one folder per TI family holds the glue.
- **`<board>/`**: the board's SysConfig, linker and startup files, build script, README, `AGENTS.md` and `FAQ.md`.

Keep the application in a file with no device header. Put the board in a thin glue file (`board_init`, `uart_write`, `led_toggle`, `sleep_ms`). [PORTING.md](PORTING.md) has the route from another TI chip, STM32 HAL, Zephyr, ESPHome and Arduino.

## Run a demo on your board

Each build is one file in `docs/builds/` (start with [docs/builds/blink.md](docs/builds/blink.md); the list is [docs/builds/README.md](docs/builds/README.md)): parts, pin table and steps, one section per board, each opening with the `git sparse-checkout add` line its steps need. A board is marked tested with this setup, expected to work, needs adaptation or not assessed. The web pages are `site/projects.html`; which build ran where is `site/portability-proven.html#builds`.

## How to post what you built

Open [Show and tell](https://github.com/Fox-Red-Labs/fox-red-labs/discussions/categories/show-and-tell) and start a new discussion. A post has four parts:

1. **What you built or ported.** A line or two, and the project it started from.
2. **What it runs on.** The kit (its board id is the folder name), and the SDK or tool version.
3. **A photo, a clip or a log.** Something that shows it running.
4. **Anything that did not work.** Optional, and the most useful part.

This repository is a copy built from a private lab notebook (`.export-source` names the commit); a merged pull request is applied to the notebook under your name and comes back in the next copy. The board id is the folder name, such as `mspm0g3507`. A post with all four parts can be repeated by the next person. A step that failed goes in the [Report a problem](https://github.com/Fox-Red-Labs/fox-red-labs/issues/new?template=broke-here.yml) form, so the exact error text lands on that board's page. Questions go in [Q&A](https://github.com/Fox-Red-Labs/fox-red-labs/discussions/categories/q-a). When a post leads to a fix, the fix goes into the board's `FAQ.md` or `AGENTS.md` under your GitHub handle, and your handle joins the Built-by list. The full guide is [docs/how-to-contribute.md](docs/how-to-contribute.md).

## Measured numbers, and how to read them

Every measured number is a row in `data/results/<board>.csv`: test, conditions, run count, date, the `source` file and how it was `checked`. A test that could not run is a row too, with a status (`not_run`, `does_not_fit`, `not_supported`, `waiting`, `failed`) and the reason. Timings are medians of three runs. Columns: [data/README.md](data/README.md). The suite of 385 tests: `data/benchmark-suite.json`. Method: docs/methodology.md.

```bash
python3 tools/results/check.py
```

```bash
python3 tools/results/compare.py
```

The first validates every row; the second writes [docs/comparisons.md](docs/comparisons.md), the boards side by side, and `site/benchmarks.html` shows them.

## What is here today

Counted from the data files on 2026-10-22:

| What | Count | Source |
|---|---|---|
| Boards listed (TI kits and other makers' boards) | 22 (17 TI, 5 other); 21 have a folder in the published copy | `data/boards.csv` |
| Result rows | 6.8k, of which 5.8k measured on a board | `data/results/*.csv` |
| Tests in the standard suite | 385 | `data/benchmark-suite.json` |
| Builds | 38, of which 20 have step-by-step walkthroughs | `site/data/projects.json` |
| Tools and code bases from elsewhere that ran | 61 on 16 development kits, each with its files, command and what you see | `site/data/ports.json` |

## A TI board that is not listed

The guided tools refuse a board they have no notes for (`frl identify` returns `FRL-E010`), so nothing is flashed by guesswork. The manual path:

1. Find the closest listed board in `data/boards.csv`: same `line`, then same `core`. Its folder's `AGENTS.md` and `FAQ.md` are the best starting notes.
2. Read [TRAPS.md](TRAPS.md), then back up the new board's flash before the first write.
3. Build and flash TI's own blink example for the new part, unchanged, from its SDK. A port only counts once that works ([PORTING.md](PORTING.md) rule 1).
4. Move code with the matching route in [PORTING.md](PORTING.md). Most code needs only the board layer under `portability/<layer>/boards/` or the board's `.syscfg` to change.
5. Record what ran as rows in `data/results/<board>.csv`, including the tests that could not run, and give the board its folder as in the next section.

## Add a board or a demo

**A board:** a folder named by its id in `data/boards.csv`, with a `README.md` in the order of docs/readme-standard.md, an `AGENTS.md` where every line cites a run, a `FAQ.md` with exact error text, and rows in `data/results/<id>.csv`. A file a second board would copy unchanged goes in `portability/` or `third-party/`.

**A board drawing:** the 3D picture on a board page is a table of parts in `data/boardviz/<id>.json`. `python3 tools/boardviz/check.py` checks it, and `tools/boardviz/dev.html?board=<id>` shows it from a local server ([docs/how-to-contribute.md](docs/how-to-contribute.md#add-a-board-drawing)).

**A demo or port:** upstream code into `third-party/<project>/` with its `SOURCE.md`, your code into `ports/<project>/`, numbers as result rows, and an entry in `site/data/ports.json` or `site/data/projects.json`. The pages under `site/` are rebuilt from those sources when the change is taken in. Reports from your own board go through the issue forms in [docs/how-to-contribute.md](docs/how-to-contribute.md).

## Traps

Read [TRAPS.md](TRAPS.md) before the first flash. Its six rules keep a board recoverable: name the probe by serial number, never write the boot configuration area (NONMAIN, CCFG, SCFG), back up flash first, never mass-erase, recheck pins after changing the device in SysConfig, and read RAM use from the linker map. When a command fails, search its exact error text in the board's `FAQ.md`, then the table in [TRAPS.md](TRAPS.md#if-you-see-this); `FRL-E...` codes are in [docs/frl-errors.md](docs/frl-errors.md). The backup:

```bash
python3 tools/benchmark/backup.py save <board> --probe <serial>
```

## Folder map

| Folder | What is in it |
|---|---|
| `mspm0l1306/`, `mspm0c1104/`, `mspm0g3519/`, `mspm0g5187/`, `mspm0g3507/` | MSPM0 development kits (`mspm0g3507/` is the LP-MSPM0G3507 and the shared benchmark suite in `mcu-bench/`) |
| `mspm33c321a/`, `am13e230/`, `c2000-launchpad/` | LP-MSPM33C321A, LP-AM13E230 (motor control), LAUNCHXL-F2800137 and LAUNCHXL-F28P65X |
| `cc2340r5/`, `cc1354p10/`, `cc2755p10/`, `cc35x1/`, `cc3200-launchxl/` | Wireless kits: LP-EM-CC2340R5, LP-CC1354P10-1, LP-EM-CC2755P10, LP-EM-CC35X1, CC3200-LAUNCHXL (parked: its debugger does not answer) |
| `iwrl6432boost/`, `tda4/` | IWRL6432BOOST 60 GHz radar board and the SK-TDA4VM Linux Edge AI kit |
| `nucleo-g071rb/`, `nucleo-u083rc/`, `frdm-mcxa153/`, `pico2/`, `esp32c6/` | NUCLEO-G071RB, NUCLEO-U083RC, FRDM-MCXA153, Raspberry Pi Pico 2 H, ESP32-C6-DevKitC-1, on the same tests |
| `border-router/`, `board-to-board/`, `thermostat-system/`, `tool-telemetry/` | Builds that use more than one board |
| `portability/`, `ports/`, `third-party/` | Shared layers, ports of other ecosystems, unmodified upstream code |
| `data/` | Results, benchmark suite, board list, parts catalog, community reports |
| `docs/` | Methodology, vocabulary, builds as text, experiments, guides |
| `tools/` | Probe detection and kit lookup (`tools/frl/`), benchmark and backup scripts, renderers, checks |
| `mac-setup/` | Mac setup check and the disk space each install takes |
| `site/` | Generated web pages and the data they come from |

Top-level files: `start.sh`, `START.md`, `AGENTS.md`, `TRAPS.md`, `PORTING.md`, `llms.txt` (the index for assistants), `CONTRIBUTING.md`.

## License

Code BSD-3-Clause, written content CC BY 4.0, brand all rights reserved; third-party files keep their own licenses. Full terms: `LICENSE` and `NOTICE` in the published repository.

Fox Red Labs is a lab run by a TI employee on personal time. Contributors' projects are their own. Not affiliated with, sponsored by, or endorsed by Texas Instruments or any other manufacturer.
