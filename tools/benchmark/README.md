# Benchmark helpers: back up a board, run the suite, put it back

`detect.py` finds the board, `backup.py save|restore` reads the whole flash before a run and proves the read-back matches after, `doctor.py` checks the tools, `plan.py` shows what a run would do before anything touches the board, and `share.py` and `import_bundle.py` pack result rows into a bundle (after a privacy check; it sends nothing) and check one. The suite itself is `data/benchmark-suite.json`. Start with `python3 tools/benchmark/detect.py --no-ccs`.

Needs Python 3 and TI's debug tools. Results go to `data/results/<board>.csv`; checked by `python3 tools/results/check.py`. Not here: any board's saved image, which stays on the machine that made it.
