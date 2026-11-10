# MicroPython (ports/zephyr) on the LP-EM-CC2755P10 through TI's Zephyr fork: did not build

**Short answer.** No. MicroPython v1.29.0's Zephyr port stopped at `mpconfigport.h:26: fatal error: alloca.h: No such file or directory` in three builds on TI's fork (tag `v3.7.0-ti-1.00.01_ga`, Zephyr 3.7.0, GCC 14.3 from the Zephyr SDK as a cross compiler): the first with the CC2340R5's trimmed `prj_cc2340r5.conf` minus ADC and `CONFIG_DEVICE_DT_METADATA` (`prj_cc2755p10.conf`), the second with `CONFIG_PICOLIBC=y`, the third with `CONFIG_NEWLIB_LIBC=y`. The build's `.config` kept `CONFIG_MINIMAL_LIBC=y` each time, so the fork's Kconfig did not take either libc from the conf file; why was not traced. Status: **Did not build**. Checked 2026-11-09.

Terms: [vocab](../../../docs/vocab.md). The CC2340R5 port that runs, on upstream Zephyr 4.4.2, is in [`../zephyr-cc2340r5/`](../zephyr-cc2340r5/README.md); upstream Zephyr 4.4.2 has no board for the CC2755P10.

| File | What it is |
|---|---|
| `build.sh` | The `west build -b lp_em_cc2755p10` line; variables `MP`, `ZP`, `TC`, `VENV`, `OUT` as in the CC2340R5 script |
| `prj_cc2755p10.conf` | The basic-features configuration with a 64 kB heap (the chip has 162 kB of RAM) |

First errors, per build: `cc2755p10/bench/zephyr-fork/micropython-errors.txt`. A Lisp prompt does run on this board without Zephyr: [`../../ulisp/`](../../ulisp/README.md).
