# Contributing

Three routes: post a build in [Show and tell](https://github.com/Fox-Red-Labs/fox-red-labs/discussions/categories/show-and-tell), report a failure with one of the [report forms](https://github.com/Fox-Red-Labs/fox-red-labs/issues/new/choose), or send a change by pull request.

To post a build, give all four parts:

1. **What it is.** One or two lines, and the project it started from.
2. **Where it runs.** The board id and the SDK or tool version.
3. **One proof.** A photo, a log or a measurement with its unit.
4. **What did not work.** Or “nothing” if all of it worked.

A post with all four parts can be repeated by the next person. An accepted fix goes into the board's `FAQ.md` or `AGENTS.md` under your GitHub handle, and your handle joins its Built-by list. Questions go in [Q&A](https://github.com/Fox-Red-Labs/fox-red-labs/discussions/categories/q-a).

The guide is [docs/how-to-contribute.md](docs/how-to-contribute.md): how to post, the three report forms (I built it, it broke here, a prompt that worked), what becomes of a report, how to send a pull request, add a demo or a board, fix a page, add a number, and the rules this repo holds itself to.

This repository is a copy built from a private lab notebook; `.export-source` names the notebook commit it came from, and each update lands here as one commit. A pull request opened here is applied to the notebook under your name and sign-off, and the next copy carries it, so it is never lost in an update. Fixes you send to the projects the ports came from (Zephyr, ESPHome, MicroPython and the rest) are welcome too: each port's page says whether its change has been sent upstream.

Every commit is signed off (`git commit -s`, the Developer Certificate of Origin); if your employer may own your work, get permission first and say so in the pull request. Details in the guide.

To run the tests of the `frl` tool: `python3 -m pip install -r requirements.txt` (Python 3.12 or newer), then `bash tools/run_tests.sh`.
