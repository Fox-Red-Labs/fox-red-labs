# Thread border router from two chips, no Linux

Thread devices reaching the home network through two small chips and no Linux computer.

The page: [site/project-thread-border-router.html](../../site/project-thread-border-router.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Where it stands

- **g3507**: Not for this chip. No radio.
- **cc2340r5**: Could work. As the radio: a TI RCP image on the CC2340R5, driven by a Mac running OpenThread, joined a Thread leader over the air; pings 42 to 44 ms median, 20 of 20. Source: `cc1354p10/README.md`.
- **l1306**: Not for this chip. No radio.
- **f2800137**: Not for this chip. No radio on this chip.
- **cc1354p10**: Could work. As the host with the CC2340R5 as its radio: leader in 16 s on a fresh network; joined an existing network, and an hour of pings through the network's own border router lost 0.44 percent, median 28.5 ms. Traffic from the host out to the home network did not get through. No Wi-Fi side. Radio used under this host: the CC2340R5. The CC2755P10 runs OpenThread as a full Thread device on its own (leader 6 to 8 s after start). Source: `border-router/README.md`.
