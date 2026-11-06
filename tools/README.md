# Tools: the scripts behind the pages, the checks and the bench

`render_*.py` and `site_build.py` build the site from the data. The checks are `results/check.py` and `content_lint.py`; each prints OK or the failures. `frl/` is the first-use tool (`python3 tools/frl/frl.py detect`), `benchmark/` backs up a board and runs the suite, `motor/` holds the motor console's code, and `tests/` holds the tests.

Needs Python 3 for everything and Node for the console checks; each folder's README names anything more. Take one folder with `git sparse-checkout add tools/<name>`; scripts that call another folder say so in their first lines.
