# Run Bluetooth and Zigbee on one chip at once

Bluetooth and Zigbee on one chip at the same time.

The page: [site/project-two-radios.html](../../site/project-two-radios.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Where it stands

- **g3507**: Not for this chip. No radio on this chip, so nothing to combine.
- **cc2340r5**: Not for this chip. TI's release notes say CC23xx chips do not support two radio stacks at once. TI's CC2755P10 two-radio example, moved here, also did not link: 2.1 kB of RAM short before its heap. Source: `docs/experiments/2026-10-05-cc2340r5-dmm.md`.
- **l1306**: Not for this chip. No radio on this chip, so nothing to combine.
- **f2800137**: Not for this chip. No radio on this chip, so nothing to combine.
- **cc1354p10**: Could work. Zigbee light joined Home Assistant in about 5 s and a Mac switched it over Bluetooth 4 of 4 times; Home Assistant switching was not tested. Source: `cc1354p10/dmm-zigbee-ble/README.md`.
