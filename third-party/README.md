# Third-party code, one copy for every board

Upstream projects copied here unmodified and pinned, so every board folder and every port builds against the same files. Each folder has a `SOURCE.md` or `SOURCES.txt` with the origin, the tag or commit, the license and checksums. Never edit a file in these folders: a change is a patch file kept beside the code that applies it, or a copy under `ports/`. The layout rule: `docs/repo-layout.md`.

| Folder | Project | License | Used by |
|---|---|---|---|
| `arduino/` | ArduinoCore-API 1.5.2, Arduino_GFX 1.6.8, Adafruit_BusIO 1.17.4, Adafruit_FT6206 1.1.1 | LGPL-2.1, BSD, MIT, MIT | `portability/arduino/` |
| `lvgl/` | LVGL 9.6.0 (commit `80ca777`), unused parts removed, three size patches in `mspm0g3507/lvgl-g3507/size-patch.diff` | MIT | `mspm0g3507/lvgl-g3507/` |
| `coremark/` | EEMBC CoreMark core files; each board keeps its own `core_portme.*` | CoreMark license (`LICENSE.md`) | `cc2340r5/baremetal/`, `cc1354p10/baremetal/`, `mspm0l1306/zephyr/mcu-bench-l1306/` |
| `bitnetmcu/` | BitNetMCU, commit `0715bfc` | GPL-3.0 | `ports/bitnetmcu/` |
| `u8g2/` | u8g2 2.37.1 (commit `414efcd`), core files, the SSD1306 driver and two fonts | BSD-2-Clause | `c2000-launchpad/compile-checks/u8g2/` |
| `mlkem-native/`, `mldsa-native/` | ML-KEM (FIPS 203) and ML-DSA (FIPS 204) C code, PQ Code Package 2.0.0 | Apache-2.0, ISC or MIT (your choice) | `portability/pqc-join/`, `cc2755p10/`, `mspm33c321a/` |
| `holoscan-sensor-bridge/` | NVIDIA Holoscan Sensor Bridge emulation sources, commit `4b192ae` | Apache-2.0 | `tda4/hsb/` |
| `ti-cc140xp-host/` | TI's CC140xP transceiver host driver, copied from TI's `simplelink-zephyr` fork | BSD-3-Clause (TI wrappers Apache-2.0, see `LICENSE-simplelink-zephyr`) | `mspm0g5187/radio/` |
| `fatfs/`, `pff/` | FatFs R0.16 and Petit FatFs R0.03a (ChaN) | FatFs license (BSD-style, 1-clause equivalent) | `portability/boosterpacks/sharp128/` |
| `micropython/` | MicroPython v1.29.0 (commit `0fd6c573e`), core, extension modules and helper libraries only | MIT | `ports/micropython/` |
| `arduboy-games/` | Snake, TileWorld and Crate Confusion, unmodified, one folder and licence file each | MIT, Apache-2.0, MIT | `ports/arduboy/` |
| `peanut-gb/` | Peanut-GB 1.3.0, single header, unmodified (the bounds-check patch is applied to a build copy only) | MIT | `ports/peanut-gb/` |
| `llama2c/` | llama2.c commit `350e04f` (`run.c`, `runq.c`, `export.py`), the model files are fetched, not copied | MIT | `ports/llama2c/` |
| `wasm3/` | wasm3 v0.9.0 (commit `0cd38327`), the `source/` folder | MIT | `ports/wasm3/` |
| `rp2040-doom/` | rp2040-doom commit `29a453c`, fetched into a git-ignored folder; only the fetch script and patches are kept | GPL-2.0 (Chocolate Doom parts), BSD-3 and MIT parts | `ports/doom/m33-whx/`, `ports/doom/target-cc35x1-whx/` |
| `chips/` | floooh/chips Z80 core (`z80.h` only), commit `9e88298` | zlib | `ports/zx-spectrum/` |
| `minimp3/` | minimp3, commit `ea99364` (`minimp3.h` only) | CC0-1.0 | `ports/minimp3/` |
| `ulisp-arm/` | uLisp ARM 4.9a, commit `d6427ec` (`ulisp-arm.ino`, unmodified; the build copy gets one board block) | MIT | `ports/ulisp/` |
| `liblc3/` | liblc3 v1.1.3 (commit `96a3af0`), `src/` and `include/` | Apache-2.0 | `ports/lc3/` |
| `opus/` | libopus 1.5.2 (commit `ddbe483`), library sources and headers | BSD-3-Clause | `ports/opus/` |
| `plaits/` | Plaits DSP and stmlib (pichenettes/eurorack `f4cfa76`, stmlib `e3bd7c9`), the voice, engines and tables, no module firmware | MIT | `ports/plaits/` |
| `simplefoc/` | SimpleFOC (Arduino-FOC) 2.4.0, commit `b3d8f39`, `src/` without the other chips' board code | MIT | `ports/simplefoc/` |

Every folder here has its licence file and a `SOURCE.md` or `SOURCE.txt` that names the upstream, the tag or commit and what was left out. A folder that holds several libraries (`arduino/`) keeps one licence file per library, and the ones that ship none upstream say so in `SOURCES.txt`. The legal list is the repository's `NOTICE`.
