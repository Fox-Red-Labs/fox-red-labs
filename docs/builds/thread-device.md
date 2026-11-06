# Join a Thread network

A Thread device on the chip itself: it forms or joins a network.

The page: [site/project-thread-device.html](../../site/project-thread-device.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Where it stands

- **g3507**: Not for this chip. No radio. To host a separate Thread radio the way the LP-CC1354P10-1 does, the host program needs about 125.9 kB of RAM and 456.7 kB of flash. The flash figure is code with its initialized data, from the image that ran with logging on; this chip has 32 kB and 128 kB, so it would not fit. UART and SPI for the radio link are there.
- **cc2340r5**: Not for this chip. Did not link with the toolchain used here, which is not the version TI tested: 7.5 to 20.7 kB of RAM short for TI's default builds, up to 23.8 kB with the NCP option. Suspected chip limit. The same builds link on the 64 kB CC2340R53. Source: `docs/experiments/2026-10-05-cc2340r5-thread-full-device.md`.
- **l1306**: Not for this chip. No radio. To host a separate Thread radio the way the LP-CC1354P10-1 does, the host program needs about 125.9 kB of RAM and 456.7 kB of flash. The flash figure is code with its initialized data, from the image that ran with logging on; this chip has 4 kB and 64 kB, so it would not fit. UART and SPI for the radio link are there.
- **f2800137**: Not for this chip. No radio. To host a separate Thread radio the way the LP-CC1354P10-1 does, the host program needs about 125.9 kB of RAM and 456.7 kB of flash. The flash figure is code with its initialized data, from the image that ran with logging on; this chip has 36 kB and 256 kB, so it would not fit. UART and SPI for the radio link are there.
- **cc1354p10**: Works. Formed its own Thread network and became leader in 6 to 10 s; full device uses 427 kB of flash and 94 kB of RAM. Source: `cc1354p10/thread/README.md`.
- **cc2755p10**: Works. Full Thread device became leader of a new network 12 s after start (one run)
