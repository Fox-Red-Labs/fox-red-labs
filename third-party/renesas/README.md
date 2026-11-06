# Renesas ra-fsp-examples, application source only

Terms: [vocab](../../docs/vocab.md). **FSP** is Renesas's Flexible Software Package; **DriverLib** is TI's C library for the MSPM0 peripherals.

Five Renesas example applications (`iic_master`, `sci_uart`, `adc`, `gpt`, `adc_gpt_periodic_sampling`), copied unmodified from https://github.com/renesas/ra-fsp-examples at commit `c74f31fde709a8b27571fee279ec584720fae4cd`, so they can be built for the LP-MSPM0G3519 by `portability/fsp-on-mspm0/build.sh`. Each folder's `SOURCE.md` names the upstream path and what was not copied. The license is BSD-3-Clause, Copyright (c) 2025 Renesas Electronics (`LICENSE`); every source file keeps its own header.

Do not edit these files. Changes live in `ports/renesas-ra-fsp/patches/`. How to build, flash and run them, and what the results were: `ports/renesas-ra-fsp/README.md`.
