# frl: find the board, check the tools, back up, build, flash, verify

The Fox Red Labs first-use tool. It works with no AI assistant: `python3 tools/frl/frl.py detect` lists the probes, `doctor --board <id>` names each missing tool with its version and size, and `./start.sh mspm0l1306 blink --probe <serial>` runs the whole route for the LP-MSPM0L1306 (back up, build TI's blink, flash, read the LED pin). Routes are in `data/routes.json`; error codes in `docs/frl-errors.md`; the full guide in `docs/frl.md`.

Needs Python 3 and TI's tools (installed by you). Never writes the board's boot configuration area. Other boards get `frl identify` and a handoff, not an automatic flash.
