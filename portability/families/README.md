# Porting guides by TI chip family

One folder per family. Each guide says which parts are held, how they differ in ways that stop a port, which driver layers have run on them with the file that proves each, and how to move code into, across and out of the family. MSPM0, MSPM33, the radio family, CC35xx Wi-Fi, AM13, C2000, AM62L and AM67A have guides; every family has a generated evidence file.

Terms: [docs/vocab.md](../../docs/vocab.md). A board folder (`mspm0l1306/`, `am13e230/` and the rest) says how to bring up one board; these folders say how to support a family. The cross-chip method and the full test record are [`portability/README.md`](../README.md) and [`portability/matrix.csv`](../matrix.csv).

| Family | Boards held | Guide | Evidence (generated) |
|---|---|---|---|
| MSPM0 | C1104, L1306, G3507, G3519, G5187 | [README.md](mspm0/README.md), [AGENTS.md](mspm0/AGENTS.md), [FAQ.md](mspm0/FAQ.md) | [EVIDENCE.md](mspm0/EVIDENCE.md) |
| MSPM33 | MSPM33C321A | [README.md](mspm33/README.md), [AGENTS.md](mspm33/AGENTS.md), [FAQ.md](mspm33/FAQ.md) | [EVIDENCE.md](mspm33/EVIDENCE.md) |
| CC23xx, CC27xx, CC13xx | CC2340R5, CC2755P10, CC1354P10 | [README.md](radio/README.md), [AGENTS.md](radio/AGENTS.md), [FAQ.md](radio/FAQ.md) | [EVIDENCE.md](radio/EVIDENCE.md) |
| CC35xx Wi-Fi | CC35X1 | [README.md](wifi/README.md), [AGENTS.md](wifi/AGENTS.md), [FAQ.md](wifi/FAQ.md) | [EVIDENCE.md](wifi/EVIDENCE.md) |
| AM13 | AM13E230 | [README.md](am13/README.md), [AGENTS.md](am13/AGENTS.md), [FAQ.md](am13/FAQ.md) | [EVIDENCE.md](am13/EVIDENCE.md) |
| AM26x | AM263P | _guide not written yet_ | [EVIDENCE.md](am26/EVIDENCE.md) |
| C2000 | F2800137, F28P65X | [README.md](c2000/README.md), [AGENTS.md](c2000/AGENTS.md), [FAQ.md](c2000/FAQ.md) | [EVIDENCE.md](c2000/EVIDENCE.md) |
| mmWave radar | IWRL6432BOOST | _guide not written yet_ | [EVIDENCE.md](radar/EVIDENCE.md) |
| TDA4 Linux | SK-TDA4VM | _guide not written yet_ | [EVIDENCE.md](linux/EVIDENCE.md) |
| AM62L | TMDS62LEVM | [README.md](am62l/README.md), [AGENTS.md](am62l/AGENTS.md), [FAQ.md](am62l/FAQ.md) | [EVIDENCE.md](am62l/EVIDENCE.md) |
| AM67A | BeagleY-AI | [README.md](am67a/README.md), [AGENTS.md](am67a/AGENTS.md), [FAQ.md](am67a/FAQ.md) | [EVIDENCE.md](am67a/EVIDENCE.md) |

Families and boards are the ids of `data/support-matrix.json`. Each `EVIDENCE.md` is written by `python3 tools/render_families.py` from `portability/matrix.csv`, `site/data/ports.json`, `data/support-matrix.json`, `data/board-ids.json` and `data/results/`; `python3 tools/render_families.py --check` fails when one is stale, and `tools/tests/test_render_families.py` runs that check. Never edit an `EVIDENCE.md` by hand: change its source and rerun.

To add a guide for another family, copy the shape of `mspm0/` (README.md, AGENTS.md and FAQ.md beside the generated EVIDENCE.md), cite a file or a matrix row for every claim, and add the folder to `llms.txt`.
