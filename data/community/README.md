# What other people built, where it broke for them, and the prompts that worked

One row per report from someone outside this bench, in `reports.csv`. Terms: [docs/vocab.md](../../docs/vocab.md). The lab's own measurements never go here; they live in `data/results/<board>.csv` with how they were checked. A report is what a reader told us, so the site shows it under the heading "Reports from others" and never as a result measured here.

How a row gets here: a reader opens one of the three issue forms on the public repository (built it, it broke here, a prompt that worked), the form asks the questions below, and `tools/community/issue_to_row.py` turns the issue into a row. When a report is reproduced on this bench, the row's `status` changes and the fix goes where the reader hits it (the board's FAQ, the project's step). The row stays, so the next reader sees that the problem was real and is closed.

## Columns

| Column | Meaning |
|---|---|
| `id` | The issue number on the public repository, `issue-<n>` |
| `date` | The day the report was filed, `YYYY-MM-DD` |
| `kind` | `built` (a timed build on their own board), `broke` (where a step failed), `prompt` (a prompt that produced the result, stored as typed in `text`, a row like any other in `reports.csv`) |
| `board` | The board page id (`mspm0g3507`, `cc2340r5`, as in `site/data/code.json`) |
| `project` | The project slug (`blink`, `run-zephyr`, as in `site/data/projects.json`), empty when the report is about the board alone |
| `os` | Their machine: `macOS 15.7 Apple Silicon`, `Windows 11`, `Ubuntu 24.04` |
| `tools` | The tool versions they used, as they wrote them |
| `hands_on_min`, `waiting_min` | The two clocks, in minutes, for a `built` report |
| `matched` | `yes` when their number matched the lab's, `no` with their number in `value`, empty when they did not compare |
| `value` | Their measured number, with its unit, when they gave one |
| `step` | The step number or name where it broke, for a `broke` report |
| `error` | The exact error text they pasted, for a `broke` report |
| `text` | The rest, in their words, one line; for a `prompt` report, the prompt itself |
| `status` | `reported` (as filed), `reproduced` (the same thing was seen here), `fixed` (the page now carries the fix, named in `link`), `not_reproduced` (a reproduction was tried and failed), `added` (a prompt kept as a row and noted on the page) |
| `link` | The issue URL, and after a fix, the page or file that carries it |
| `credit` | The GitHub handle, when they asked to be credited |

Rules: a report is never edited for content, only its `status` and `link` change. No report is invented or copied from elsewhere. `python3 tools/community/check.py` validates the file (ids unique, kinds and statuses from the lists above, boards and projects that exist).

To turn a `broke` report into a regression case and a FAQ draft: `python3 tools/community/report_to_case.py <id>` writes `data/community/cases/<id>.json` with the reporter's configuration and consent (a public issue may be quoted; `credit` decides whether they are named), with home-folder paths, IP addresses and probe serials removed. A case is not a result until it is reproduced here.
