# Measure how far a radio reaches

How far the radio reaches in your house, with signal strength and packet loss per room.

The page: [site/project-radio-range.html](../../site/project-radio-range.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Where it stands

- **g3507**: Not for this chip. No radio.
- **l1306**: Not for this chip. No radio.
- **f2800137**: Not for this chip. No radio on this chip.
- **g5187**: Could work. The sub-1 GHz radio ran, OFDM included on transmit. A CC1407P BoosterPack on this board sent 21,658 of 21,658 packets to an LP-CC1354P10-1 at 915 MHz, -14 dBm on one desk. It also sent all 23 of TI's radio modes, every OFDM mode among them, from 0.69 kbps to about 2.2 Mbps. No OFDM receiver is on the bench, so nothing received those packets. Reach through the rooms of a house is not measured. Source: `docs/experiments/2026-10-10-mspm0g5187-cc1407p-weather-station.md`.
