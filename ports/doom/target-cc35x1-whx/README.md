# rp2040-doom design on the LP-EM-CC35X1 (CC3551E): all three demos finish, 39.2 frames/s

**Short answer.** The engine that fits the LP-MSPM33C321A's RAM on paper runs on this board. The converted game data (WHX, 1,758 kB) is linked into the image and read in place from the external flash. One image plays the three shareware demos to the end, using 28.9 kB of zone (the zone is 40 kB), at 39.2 frames/s (22.5 before the memory placement and 36.7 before -O2 for the render files, sections 13 and 16) with no panel and no sound. The frame checksums at frames 100, 200, 300 and 400 and over all 3,881 frames equal the Cortex-M33 emulator run of the same engine (with the one-line stack-array fix, `patches/0002-pixels-init.patch`). The stock engine on this board (target [`target-cc35x1`](../target-cc35x1/README.md)) stops on zone memory in all three demos. Experiment page: [Doom, smallest TI chip, sections 11 and 13](../../../docs/experiments/2026-10-20-doom-smallest-ti-chip.md). License terms: [LICENSING.md](../LICENSING.md).

Terms: [vocab](../../../docs/vocab.md). 1 kB = 1,024 bytes. **WHX** is rp2040-doom's compressed game-data format; **zone** is the engine's memory pool; **XIP** is reading flash through the address space.

## What is here

| File | What it does |
|---|---|
| `build.sh` | Memory placement (section 13) and -O2 for the render files (section 16) are the default; `HOTO2=""` gives -Os everywhere. Placement: statics, zone, render list and thread stack in the 128 kB fast RAM, the lump cache in fast RAM, code RAM and DRAM. Options: `TCMSTAT=0 LUMPCACHE=0` gives the 22.5 frames/s layout, `PROF=1` adds the per-step cycle report. Copies the SDK `uart2echo` example, switches OTA off, applies the m33-whx patch and `patches/0001-demo-end-hook.patch` to the pinned upstream, compiles the 78 engine files with the build flags of `../m33-whx/` (window base and data address changed), links with the SDK linker file changed to put the zone and all `.bss` in DRAM, signs with the Toolbox. `./build.sh 40` sets a 40 kB zone (default). `CRCALL=1` checksums every frame. Checks that the zone and the statics the engine points at are inside the 256 kB window. |
| `platform_core.c` | The engine-side platform layer, from `../m33-whx/plat/platform.c`: virtual game clock, frame checksum, demo-end report, stubs for sound, keys and network. |
| `panel_cc35x1.c`, `PANEL-WIRING.md` | Optional Waveshare 3.5 inch panel driver (`PANEL=1 ./build.sh 31`), written for the same panel as the LP-MSPM0G3519 record; the wire list and picture are in `PANEL-WIRING.md`. Built, not run on a panel; the default image is unchanged |
| `hal_cc35x1.c` | FreeRTOS side: 921,600 baud console on the probe's port, tick, the thread that starts the engine. |
| `lumpcache_patch.py`, `prof_patch.py` | Build-time edits of the pinned upstream copy: the lump RAM cache hook, and (with `PROF=1`) cycle timers around the big steps of a frame |
| `whx_blob.S`, `fox_whx.h` | Include the converted file into the image (page aligned) and name it for the engine (`TINY_WAD_ADDR=fox_whx`). |
| `patches/0001-demo-end-hook.patch` | One call at the top of `G_CheckDemoStatus` so the firmware reports each demo end. |
| `patches/0002-pixels-init.patch` | Zeroes the stack array `draw_composite_columns` draws before writing (same as `../m33-whx/patches/0003`). |

Shared with `m33-whx`: sources list, build flags, stand-in headers, the three-file upstream patch, the converter. Chip-specific: the platform layer, the memory placement and the data blob.

## Build and run

```bash
cd ports/doom && ./fetch.sh && m33-whx/convert.sh          # WAD, then wad-cache/DOOM1.whx (git-ignored)
target-cc35x1-whx/build.sh                                  # image and signed Toolbox files in the build folder the script prints
# flash <build>/ex/freertos/ticlang/toolbox/tool_settings.json with the Toolbox command in step 4 of ../../dns-sinkhole/README.md, reset, read the console at 921,600 baud
```

The console prints `CRC n xxxxxxxx` every 100 frames, `DEMOEND k frames= ms= zonehw= crc=` at each demo end, then `EXIT`, `FPS after400` and `DONE`. The restore is a full flash erase, then program the ad-blocker build ([DNS sinkhole page](../../dns-sinkhole/README.md), step 4).

## Numbers (2026-10-21, 160 MHz, serial 921,600 baud)

Reference for the checksums is the emulator run of the same engine and data : the WHX design draws different frames from the stock engine, and has no Mac build here. The game clock is virtual (35 ms per read, as in the emulator), so the frames drawn do not depend on the chip's speed.

| What | Value |
|---|---|
| Frame checksum at 100, 200, 300, 400 | `f1b5e5d6`, `27572f0c`, `d0a2dc52`, `9ecd4100`: all equal to the emulator |
| Chain over all frames, after demo 1, 2, 3 (CRCALL image) | `fe183649`, `c6686ada`, `fc7b6c2d`: equal to the emulator with the stack-array fix, at -Os and at -O2. (`e9db8f1d` was the chain of the engine before the fix.) |
| Demo ends (frames) | 1,736, 3,092, 3,881: equal to the emulator |
| Zone high water | 27.0 kB after demo 1, 28.9 kB after demos 2 and 3: equal to the emulator; zone 40 kB |
| Frame rate after frame 400 | 39.2 frames/s over 3,481 frames at -O2 (36.7 at -Os, 22.5 with the first layout) |
| Frame rate with the checksum on every frame | 20.3 |
| Flash | 2,017 kB: 1,758 kB data file and 259 kB code, read-only data and init image |
| RAM | 260 kB: 220 kB static and RTOS (engine statics 183 kB, FreeRTOS heap 22 kB, libc heap 4 kB, stacks), 40 kB zone; 248.7 kB of it inside the 256 kB pointer window |

Expectation (from the experiment page, section 10: an instruction count of 2.55 million per frame): 42 to 63 frames/s if the CPU were the only cost. First measurement 22.5. The cause is not the flash cache: instruction fetches hit the flash cache 99.7 percent of the time. It is where the data sits. A load or store costs 1.06 cycles in the 128 kB fast RAM (TCM), 1.6 in the 32 kB code RAM, 6.2 in the 512 kB DRAM and 44 from the external flash; the first layout kept every static, the zone and the thread stack in DRAM. Moving them to TCM took the rate from 22.5 to 34.5. The final layout reaches 36.7 at -Os and 39.2 with the render files at -O2, 93 percent of the low end of the estimate. Per-step cycles are in section 13 of the experiment page. The demos hold 10,996 game tics in 3,881 frames, about 2.8 tics per rendered frame, because the game clock is stepped by the firmware per frame; the original draws one frame per tic (28.6 ms) at 35 tics/s. The rate here is frames rendered per second, not the pace of play, and the pace against real time was not measured.

## Limits

No panel, keys or sound; the data file is the whole shareware game, but only the three demo levels (three of nine) were run. The window rule: the 16-bit pointers reach 256 kB from `0x28000000`, so the engine's statics and the zone must all sit in it, which caps the zone near 43 kB with this layout. The frame rate is one run per image. Code layout moves it by up to 7 percent: the same code with cycle timers added ran at 39.5, and compiling the render files with -O2 gave 39.3. Without the stack-array fix, -O2 changes the demo 3 checksum (an uninitialized stack array in the engine, see the m33-whx README); the build applies the fix.
