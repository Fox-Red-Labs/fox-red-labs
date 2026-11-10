# Send a message to a satellite

Goal: a message from an LP-EM-CC2755P10 received by a satellite about 550 km up. Not achieved.

The page: [site/project-hubble-satellite.html](../../site/project-hubble-satellite.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Where it stands

- **g3507**: Not for this chip. No radio.
- **cc2340r5**: Not for this chip. The chip's 8 dBm output is below the 20 dBm the satellite link needs; it works for Hubble's ground network only.
- **l1306**: Not for this chip. No radio.
- **f2800137**: Not for this chip. No radio on this chip.
- **cc2755p10**: Could work. Hubble's satellite sample builds, flashes and transmits every 10 s with a 256-bit key; the matching Bluetooth beacon image was heard and decoded on a Mac, 4 of 4 packets in 40 s at -54 to -57 dBm. The board went outside for one pass on 2026-10-18; nothing has reached the dashboard.
