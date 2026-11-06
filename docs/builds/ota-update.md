# Update a board's firmware over the air

New firmware on a board that is already installed somewhere, without plugging it in.

The page: [site/project-ota-update.html](../../site/project-ota-update.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Where it stands

- **g3507**: Not for this chip. No radio.
- **cc2340r5**: Could work. Over Bluetooth from a laptop: 43 s for the whole update, with TI's dual-image example. Over Zigbee from Home Assistant: starts, then stalls: 88.7% of a 200 kB image at best (12 transfers, none finished). Unknown (suspected: radio environment). Source: `cc2340r5/oad/README.md`.
- **l1306**: Not for this chip. No radio.
- **f2800137**: Not for this chip. No radio on this chip.
- **cc35x1**: Works. Over Wi-Fi from a Mac: download about 1 s, install and reboot 8 s, new image running (one attempt) Source: `data/results/cc35x1.csv`.
