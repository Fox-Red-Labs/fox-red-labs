# The data API, version 1

Static JSON files under `site/api/v1/`, one per data set, written by `python3 tools/api_v1.py` from the same files the site is drawn from. Read them with any HTTP client or from a clone; nothing is served by a program. An assistant connected over MCP gets the same files through the `site_api` tool and the list tools beside it (`docs/use-with-your-tools.md`).

Terms: [vocab](vocab.md). Part of [docs/](README.md).

## The files

| File | What a row is | Source |
|---|---|---|
| `index.json` | the list below with row counts and a sha256 per file | this script |
| `boards.json` | one board or kit on hand: id, name, vendor, line, core, clock, flash, RAM, test classes, status | `data/boards.csv` |
| `results.json` | one measured number, or one test that could not run with its reason | `data/results/*.csv` (`data/README.md`) |
| `parts.json` | one board, kit, module or part a build uses: what it is, hook-up, where to buy, which builds use it | `data/parts.json` |
| `ports.json` | one port of another ecosystem's code onto a TI chip, with what ran and its limits | `site/data/ports.json` |
| `milestones.json` | one thing that first ran, built or was ported, with its level, limits and evidence | `data/milestones.json` |
| `support-matrix.json` | the works-with grid per TI family | `data/support-matrix.json` |
| `questions.json` | one question the site answers: the question as a reader types it, the files that hold the answer, the strings a correct answer contains, and whether a model is needed (`tokens`, never required on this repo's side) | `data/questions.json` |

Every file carries `schema_version`, `generated` (the UTC date it was written), `source` and `row_fields` (the top-level fields its rows carry). Fields inside a row are the source file's own fields, documented where that file is (`data/README.md` for results, the `note` key of each JSON source).

## What is promised

- The file names, the top-level keys and the row fields of version 1 stay as they are. A field may be added. A field is removed or renamed only in a new version under a new folder (`v2/`), and this page lists the change.
- `python3 tools/site_check.py` fails when a file is older than its source, so a release never ships a stale snapshot.
- `data/api-v1-fields.json` records every row field each file has carried. `tools/api_v1.py` adds new fields to it and fails, on a write and on `--check`, when a recorded field is no longer in the rows, so a removal cannot ship inside v1.
- The renderer-internal files (`site/data/projects.json` and the others) are not promised and may change without notice.

## Changes

- 1.0, 2026-10-20: `row_fields` added to every file (a key added within the version), and the field record that holds the promise.
- 1.0, 2026-10-17: first version, seven data sets.
- 1.0, 2026-10-18: `questions.json` added (a field or file may be added within a version). A model can use it to check that it finds each answer.
