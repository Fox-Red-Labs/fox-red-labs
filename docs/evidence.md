# Evidence levels: claim, computed, measured

Every number or status in this project is a claim, computed or measured. The level says how the fact is known. The levels never blend: a page prints only the level that happened. Terms: [vocabulary](vocab.md).

| Level | Means | A record of it needs |
|---|---|---|
| `claim` | A source says so. Nobody here calculated it or read it off hardware. Data sheet values, a vendor's published measurements, a prediction nobody has run, someone saying "done". | `source` (a repo path or URL), `date` |
| `computed` | Calculated or simulated. The arithmetic or the model is this repo's and can be rerun, but no hardware was read. Includes a build that linked and a footprint compared with a drawing by script. | `source` (the script, file or netlist), `method` (one line), the inputs, `date` |
| `measured` | Read from hardware, or from a check that a person or an independent tool made of the hardware's behavior. | `source` (the log or write-up), `checked` (how: crc, serial, readback, pin-read, by-us ...), `date`, and what it was measured on |

Rules:

- Rising order is claim, computed, measured. A number never rises on its own; a new run replaces the row.
- `self-report` is not a check. A number whose only support is someone's say-so is a `claim`.
- A measured number carries what it was measured on. A radar result from the IWRL6432BOOST is measured for that board and a claim for the carrier that uses the same chip, until the carrier is measured.
- A prediction compared with a capture is two numbers with two levels (computed or claim, and measured), and the comparison is a third thing that has no level of its own.
- A row that asserts nothing (waiting, not run, open) has no level. It is pending, not low.
- Reader pages keep their plain wording. This page is the vocabulary underneath: a page never says more than its level allows.

## How each existing store maps

The stores keep their own words.

| Store | Its field | claim | computed | measured | pending | Gap today |
|---|---|---|---|---|---|---|
| `data/results/*.csv` (`data/README.md`) | `status`, `checked` | `not_supported`, `not_applicable`; and `measured` rows whose `checked` is `self-report` or empty, or with no `source` | `computed` | `measured`, `failed`, `does_not_fit` with a real `checked` | `not_run`, `waiting` | The column holds the level already, but no row says it in the vocabulary's words, and a self-report row still reads `measured`. |
| `data/milestones.json` | `level` | `ran`, `mapped` (proposed) | `built` (proposed) | `verified`, `measured` | none | The ladder is a different scale (built, ran, verified). The mapping of `ran`, `built` and `mapped` is a proposal, not confirmed. |
| Live panel (`tools/live/live_panel.js`) | `compare(pred, meas, tol)` result | the prediction series, if it came from a datasheet | the prediction series, if it was simulated | the measured series | none | The function has no level field on its inputs or its output. The page has to say which series is which. |
| `site/data/projects.json` cells (`evidence()` in `tools/site_ui.py`, used by `tools/render_projects.py`) | `st`, `proof`, `checked_by` | kind `part` (Ran or Boots, not checked; Reported, not checked) and `no` | kind `build` (Builds only) | kind `run` (Works: checked or demonstrated, by us) | kind `wait` (Waiting) | The renderer computes the word from `st` and `proof`; the export and the search index carry the word, not a level. |

## Open points

These are not settled.

1. Whether `ran` is a claim or a measured observation. The ladder says "ran, behavior not checked", which fits `claim`, but a hardware run did happen.
2. Whether `built` is `computed` (a tool judged it) or its own kind.
3. The search index carries the levels a section marks with `data-evidence`, and the search page filters on them (Evidence facet). Few pages mark their numbers, so the facet fills as they are marked.
