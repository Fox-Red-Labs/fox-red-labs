# chips (Z80 core only)

- Upstream: https://github.com/floooh/chips, commit `9e88298ce56319953ac7a43213a1120359f7a3a6`.
- Copied unmodified: `chips/z80.h` (single-header, one T-state per `z80_tick`, SHA-256 `6ca70ffd91b1bdbaf00f7a41e7092b326145fd1c95dcdd7bb01183370de734f3`) and the upstream `LICENSE` (zlib/libpng, Copyright (c) 2018 Andre Weissflog; SHA-256 `0c692ece632a8ec6d2a566c9234fdabda655644e6abc70027da9ff7c35e48634`). Left out: every other chip, the systems, the UI and the code generator.
- Used by: `ports/zx-spectrum/`.
