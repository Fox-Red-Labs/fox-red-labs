# MLPerf Tiny (upstream, vendored)

Files from <https://github.com/mlcommons/tiny> at commit `4addd0fa08d216e20637637874e084895f289da4` (2026-07-06), Apache-2.0 (`LICENSE.md`): the device API (`api/`, `main.cpp`), the rules (`MLPerfTiny_Rules.adoc`), the host runner (`runner/`, Python), and the keyword-spotting training scripts and reference models (`kws/`).

One change, marked in the file: `api/submitter_implemented.h` lets a port set `MAX_DB_INPUT_SIZE` (the upstream value, 27,648 bytes, does not fit the 32 kB of RAM on the LP-MSPM0G5187). Everything else is as upstream. Used by `mspm0g5187/mlperf-tiny/`.
