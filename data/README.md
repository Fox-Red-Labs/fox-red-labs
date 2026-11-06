# Results store

Every measurement in this project, one row each, in one format. Terms: [docs/vocab.md](../docs/vocab.md). Comparisons, board pages and the status deck are built from here, so a number is written once and read everywhere.

- `boards.csv`: one row per board (TI and other vendors), with the test classes that apply to it.
- `benchmark-suite.json`: the standard benchmark suite. Test ids, units, which way is better, and the variants each test takes.
- `results/<board>.csv`: the measurements for one board. One file per board, so a change to one board never touches another board's numbers.

Board ids: the id is the basename of `results/<id>.csv`, and every data file (`site/data/projects.json`, `ports.json`, `code.json`, `data/milestones.json`, `support-matrix.json`, `arrivals.json`, `boards.csv`, `docs/inventory.md` part names) uses it. The only exceptions are in `board-ids.json`: short ids and page ids that renderers still read (`g3507`, `l1306`, `c1104`, `g3519`, `g5187`, `tda4vm`, `cc3551e`, `iwrl6432aop`), each mapped to the real id, and boards with no results file. `python3 tools/board_ids.py --check` fails on any other id; `site_check.py` runs it.

Check and compare:

```bash
python3 tools/results/check.py          # every row valid, every test id known
python3 tools/results/compare.py        # writes docs/comparisons.md
```

## Columns in `results/<board>.csv`

| Column | Meaning |
|---|---|
| `board` | Id from `boards.csv` |
| `test` | Id from `benchmark-suite.json` |
| `variant` | What makes this row different from others of the same test: toolchain, OS and flags (`ticlang-bare-O3`), model and build (`occ_cnn_8k-c-O3`), stack (`zigbee-router`), job, model and docs (`zephyr-blink-sonnet5.5-ours`). Lower case, hyphens. Empty if there is only one way |
| `status` | `measured`, `computed` (arithmetic on measured numbers that predicts something no run tested; needs `source` and a `note` saying how), `not_run`, `waiting` (hardware or meter missing), `does_not_fit`, `not_supported`, `not_applicable` (the chip has no such block, for example a radio test on a C2000 or an AES speedup on a chip with no AES engine; needs a `note` naming the missing block; shown as n/a, not as a gap), `failed` |
| `value` | The number, in the test's unit: finite (no `NaN` or infinity). Empty unless `measured` or `computed` (or `does_not_fit` with the shortfall as a negative number). A negative number is fine where the test means it (a shortfall, dBm). A `yes/no` or `status` test may record `yes`, `no`, `partly` or `partial` instead |
| `value_hi` | Upper end when the result is a range (25 to 28 cycles: `value` 25, `value_hi` 28). A finite number |
| `runs` | How many runs the value comes from: a whole number, 1 or more on a `measured` or `computed` row (1 is allowed but shows as a single run); 0 only on a row where nothing ran. Leave it empty when the count is unknown; never fill in 1 |
| `clock_mhz` | CPU clock for the run, when it matters. A positive number |
| `conditions` | Anything that could move the number: supply voltage, Mac load average, RSSI, meter, code in RAM or flash |
| `date` | Day measured, a real calendar date as YYYY-MM-DD |
| `source` | Repo path of the write-up or log the number comes from |
| `checked` | How it was checked: `crc` (CoreMark self-check), `vectors` (20 of 20), `serial`, `pin-read`, `ha-state`, `scan`, `by-us` (agent report checked independently), `linker` (linker map), `build-log`, `readback` (read from the chip by the debugger), `host-clock`, `self-report` (not checked) |
| `note` | One short line. No commas inside unquoted text |

Rules:

- One row per number. A table of results is many rows.
- Measured rows need `source` and `checked`. A number nobody can trace does not go in.
- Corrections replace the row. History is in git.
- `not_applicable` is a fact about the chip; `not_supported` is a software or tool limit that a port or a vendor fix could change. Use `not_applicable` only when the silicon lacks the block, with the block named in `note`.
- A gap is a result. If a test cannot run, add the row with its status so the comparison shows the hole and the reason.
- Measured or computed. A number read off a run, or a unit change of one (cycles to cycles per byte, worst cycles to a percent of a period), is `measured`, with `conditions` saying "derived from <capture>". A formula that claims something no run tested (a "rate with 20 percent free" from one worst case) is `computed`. A test whose method was replaced keeps its rows: `benchmark-suite.json` names its successor in `superseded_by`, and `check.py` refuses new `measured` rows for it.
- Repeat runs: timings and agent jobs need 3 runs before they are quoted as a median. Store the median in `value` and the spread in `value_hi` or `note`. A median of samples inside one run ("median of 20 commands") is one run.
- An unknown run count is a provenance gap, not a 1. `python3 tools/results/check.py --gaps` lists measured rows with no `runs`; fill one only from its source.
- `check.py` rejects a row with a missing or extra cell, a non-finite number, a `runs` that is not a whole number, a `clock_mhz` that is not positive and a date that is not a real day, naming the file, line and column. Tests: `tools/tests/test_results_validation.py`.

## What a label means

Two vocabularies, kept apart. A page may use only the one that happened (`docs/content-standard.md` section 5).

**Milestone levels** (`level` in `data/milestones.json`, the highest rung a milestone reached, drawn by `tools/render_milestones.py`):

| Level | What happened |
|---|---|
| `verified` | The behavior was checked against a reference: a pin read, a serial line, vectors matched, a Home Assistant state |
| `ran` | It ran on the board with no fault; nothing read its behavior beyond that |
| `measured` | A time, size or cost was read from the run's own log |
| `built` | It compiled and linked; it was not run |
| `mapped` | Counted or predicted from the SDKs; not built |

**Proof labels on cells, experiments and results rows** (five claims, in rising order):

| Claim | What happened | Where it is recorded |
|---|---|---|
| Built | The image linked | `builds` cell; experiment `reached: link` |
| Booted | The chip started the image and did not fault | `proof: booted`; experiment `reached: boot` |
| Ran | It ran for the length of the test with no fault, and nothing read its behavior | `proof: ran` (the default for a `partial` cell); experiment `reached: run`; a sweep's "ran clean" |
| Checked | The requested behavior was read and matched: a pin read, a serial line, 20 of 20 vectors, a Home Assistant state | `yes` cell (default `proof: checked`); results row with `checked` set; experiment `reached: verified` |
| Demonstrated | Checked inside the application it is meant for, such as a light switched from Home Assistant | `proof: demonstrated`; an outcome marked `application` shows Works only with this |

The two line up like this: a milestone `verified` is Checked or Demonstrated, `ran` is Ran (or Booted), `built` is Built; `measured` and `mapped` have no cell label because they describe a number, not a run.

**Compatibility words are not result labels.** A project page's "Other boards" list (content standard 3a; it replaced the "Use another board" table) shows one of the three status chips and keeps the assessment as the chip's hover. The assessment has four words that describe an assessment, not a run: Tested with this setup (a scoped configuration ran and passed; needs a cell that ran and a source), Expected to work (an engineering basis and the required changes, not run), Needs adaptation (the main work is named), Not assessed (nobody looked; never promoted to expected). They live in the `recipe.compat` block of `site/data/projects.json`, per version, and never change a cell's `st`.

Two things that are not stages:

- **Who checked it.** A results row's `checked` says how the result was read. `self-report` means the agent said so and nobody read the board; the site shows those rows with that word, never as Works. A cell's `checked_by: self-report` does the same.
- **Reproduced.** `reproduced_by` names an independent builder who got the same result from the write-up. Three runs on the lab's own bench are three runs, not a second builder, and `runs` never implies it. No page shows a second-builder badge, time or quote until a real record of that build exists.

`tools/site_check.py` refuses a `yes` cell whose proof is only `built` or `booted`, an experiment marked `works` that did not reach `verified`, and a `reproduced_by` that names us. Whether a check was the right check is still read by a person; a valid file is not proof the measurement is right.

## Rows from the benchmark tools, private runs and contributed rows

The `benchmark` skill and `tools/benchmark/` write ordinary rows, with a few extra items in `conditions` so a row can travel between machines. Items are `key=value`, separated by semicolons like the rest of `conditions`; the columns do not change.

| Item | Meaning | Needed for |
|---|---|---|
| `compiler=` | Compiler and version, for example `compiler=ticlang-5.1.1.LTS` | every row a bundle carries |
| `sdk=` | SDK or RTOS and version, for example `sdk=mspm0_sdk_2_11_00_07` | every row a bundle carries |
| `board_rev=` | Board revision printed on the board, or `unknown` | every row a bundle carries |
| `log_sha256=` | sha256 of the log the number was read from; the log goes in the bundle | every row a bundle carries |
| `machine=` | Operating system and processor of the computer, for example `machine=macos-15.7-x86_64`. Numbers that depend on the computer (time to blink, agent minutes) are never pooled across machines | rows whose number depends on the computer |
| `private-run` | A run on a customer's board or prototype silicon. The row stays on that machine; `tools/benchmark/share.py` refuses it | private runs |
| `contributed=<name>` | Added by `tools/benchmark/import_bundle.py`: the row came from someone else's bundle. The name is shown only if the sender opted in; otherwise `anonymous` | contributed rows |
| `reproduced` | A contributed row was re-run on the lab's own bench, or checked a second way, and gave the same number | contributed rows |

A measured row in a bundle also needs `clock_mhz`. The import refuses rows without these items, flags a number more than 10% away from the existing row for the same board and test, points `source` at the log under `data/contributed/<bundle>/`, and adds `-contrib` to the variant when a row for that board, test and variant already exists, so both rows stay. Contributed rows count once reviewed and reach the site only after a rebuild.

## Radio tests

Defined in `benchmark-suite.json`: `radio_flash_used` (B), `radio_airtime` (ms per packet), `radio_energy_event` (uC per event) with `radio_energy_avg_ua` (uA at one event per 1000 ms, the second value the energy test needs), `radio_throughput` (kbps) and `radio_range` (m at packet error rate under 1%).

- Footprint rows (`radio_flash_used`, `radio_ram_left`) come from the build's linker map: TI maps from the `MEMORY CONFIGURATION` table (FLASH and SRAM `used`), GNU maps from the end of the last flash section and the top of the stack, Zephyr from the `Memory region` table in the build log. Flash does not count the NV or NVS area a stack reserves; the conditions column says so. Use the same variant name for the flash row and the RAM row of one image.
- The levers that move a number (PHY, rate, payload, TX power, MTU, interval, ack policy, height, environment, antenna) go in `variant` where short and in `conditions` otherwise. A computed value and a measured one are separate rows, with `conditions` saying which.
- Rows that cannot run are `waiting` (meter or second radio missing) or `not_run` with the reason in `note`. Boards without a radio have no rows for these tests.

## How a status reads on a board page

`tools/evaluation.py` maps each row to a state, an evidence kind and an outcome, kept apart (`docs/methodology.md`, "Evaluation rows on board pages"):

| status | state | evidence | outcome |
|---|---|---|---|
| `measured` | completed | measured | recorded |
| `computed` | completed | calculated | recorded |
| `failed` | completed | measured | failed |
| `waiting` | blocked | none | unresolved (`conditions` or `note` says what is missing) |
| `not_run` | not scheduled | none | unresolved |
| `not_applicable`, `not_supported`, `does_not_fit` | closed | specified | not_applicable (`note` is the reason) |

Recorded means a number under its row's conditions, not a pass. Text in `conditions` and `note` reaches the site, so write it for a reader: no first names, no private paths, no internal ids.
