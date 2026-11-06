# Crossbuild: build one TI chip's example for another chip

Scripts that build TI examples for a different TI chip family (MSPM0 on the CC2340R5, F3 on MSPM0, and the reverse), with and without the shims in `../shim/`. Each script writes one result line per build, and the matrix in `../matrix.csv` summarizes them. `build_mspm0_freertos.sh` builds the MSPM0 SDK's FreeRTOS kernel from a terminal.

Terms: [docs/vocab.md](../../docs/vocab.md). You need TI's SDKs, TI Arm Clang (versions in `../README.md`) and a Mac. Next: read `../everywhere/PORTING.md` to pick the tool for your pair of boards.
