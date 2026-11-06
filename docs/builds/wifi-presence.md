# Detect a person in a room with Wi-Fi

A board that reads the Wi-Fi in a room and says whether anyone is there.

The page: [site/project-wifi-presence.html](../../site/project-wifi-presence.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Where it stands

- **g3507**: Not for this chip. No radio.
- **cc2340r5**: Not for this chip. No Wi-Fi.
- **l1306**: Not for this chip. No radio.
- **f2800137**: Not for this chip. No radio on this chip.
- **cc35x1**: Could work. TI's presence model ran on the board: first result 7.4 s after the host asked, one every 2.0 s, and it fits in one image with a Matter thermostat. It does not separate empty from occupied in this room: 135 of 136 windows in an empty, closed room read present (2026-10-09). A second run on 2026-10-13, 6,290 results over about 4 h, read 100 percent present (cc35x1/csi/results/2026-10-13-freerun.csv). Source: `docs/experiments/2026-10-09-cc35x1-wifi-csi-presence.md`.
