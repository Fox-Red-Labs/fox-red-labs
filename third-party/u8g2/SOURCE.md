# u8g2, pinned subset

- Origin: https://github.com/olikraus/u8g2
- Tag `2.37.1`, commit `414efcdb7a908667ea461c9e3a3a31da14fc4ea5`, fetched 2026-10-08 by `fetch.sh` in this folder (shallow, sparse: `csrc/` and `LICENSE` only).
- License: BSD 2-clause (`LICENSE`, unchanged).
- Kept: the core u8x8 and u8g2 files, the SSD1306 128x64 driver (`u8x8_d_ssd1306_128x64_noname.c`) and its setup file (`u8g2_d_setup.c`, which names every display). Removed as unused: the other display drivers, the menu (`mui*`), logging (`u8log*`) and input helpers.
- Fonts: `extract/fonts_extract.c` holds `u8x8_font_chroma48medium8_r` and `u8g2_font_6x10_tr` with their comment headers, copied byte for byte from upstream `csrc/u8x8_fonts.c` (1.6 MB) and `csrc/u8g2_fonts.c` (40 MB), which are not kept. Add a font by adding a `grab()` line to `fetch.sh`.
- Checksums: `SHA256SUMS`.
- Used by: `c2000-launchpad/compile-checks/u8g2/` (experiment E52, u8g2 on the C28x).
