# Memory split by component

Splits a linker map's flash and RAM use into the components shown on the board pages (Your app, Radio stack, RTOS, TI drivers and device library, C library and compiler runtime, Heap and stacks, Other). `ti_map_split.py` reads TI Arm Clang maps, `gnu_map_split.py` reads GNU ld maps (Zephyr), `gen.py` writes `<board>/matrix/memory-breakdown.json` and `gen_md.py` the matching `.md`. Written 2026-10-06 for the LP-EM-CC2340R5; check the paths at the top of `gen.py` before running it for another board.
