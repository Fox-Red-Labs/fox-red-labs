# Results tools: check the numbers and build the comparison

`check.py` validates every row of `data/results/*.csv` against `data/benchmark-suite.json` and `data/boards.csv`, and must print OK. `compare.py` writes `docs/comparisons.md` from the same rows. The other scripts derive memory sizes and figures of merit from build records.

Needs Python 3 only. A number that is not a row in `data/results/` does not exist for the site; a test that could not run is a row with its reason (`data/README.md`).
