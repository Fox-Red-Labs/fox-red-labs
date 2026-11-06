# m33-whx: the rp2040-doom engine on the LP-MSPM33C321A (emulator stage)

**Short answer.** Doom plays all three shareware demos on the LP-MSPM33C321A: 22.7 frames/s in real time on the Educational BoosterPack screen (24.5 with a 16 kB cache). rp2040-doom's design solves the memory problem that stopped the plain engine (164 kB of static data and an 800 kB zone). Built with TI Arm Clang and linked into the chip's real 256 kB of RAM, it leaves 58.0 kB for the zone. The converted game file (1.72 MB) does not fit the chip's 1 MB of flash and its QSPI has no memory-mapped mode, so section 6 reads it through a software page cache.

- **Emulator** (Cortex-M33): all three demos play to the end. The zone's in-use high water is 28.9 kB; the smallest zone that finished all three is 30 kB.
- **Page cache** (section 6): the demos still finish. Frames match the plain build except for 20 pixels in 11 frames. The engine reads 98.3 to 99.1 percent of its accesses from the cache (8 to 19 kB), and the QSPI wait is 8.6 to 4.8 ms a frame at 40 MHz quad. The price is RAM (8.7 kB for the light tables plus the cache, leaving 0.3 to 11 kB of the 256) and about 44 percent more instructions.
- **On the chip** (2026-10-21, section 7): the data file sits in the board's QSPI flash and all three demos finish with the emulator's frame checksums. A frame takes 50.3 ms with an 8 kB cache and 45.7 ms with 16 kB (display and sound stubbed).
- **With the Educational BoosterPack** (section 8): the demos play on its screen, the stick and two buttons play E1M1, and the display adds about 1.5 ms to a frame.

Contents: 1 which rp2040-doom changes carry over, 2 files, 3 build and run, 4 numbers, 5 what is left, 6 the page cache, 7 on the board, 8 with the Educational BoosterPack.

Terms: [vocab](../../../docs/vocab.md). 1 kB = 1,024 bytes; 1 MB = 1,048,576 bytes. **WHX** is rp2040-doom's compressed game-data format; **zone** is the engine's memory pool; **XIP** is reading flash through the address space.

## 1. Which rp2040-doom changes carry over

Read from the pinned source (`third-party/rp2040-doom/SOURCE.md`) and its notes (https://kilograham.github.io/rp2040-doom/speed_and_ram.html, https://kilograham.github.io/rp2040-doom/flash.html).

| Change | Where | Depends on the RP2040? | Needed here | State on the M33 |
|---|---|---|---|---|
| WHD and WHX converter | `src/whd_gen` (5.3k lines of C++) | No: a host tool | Yes | Builds on the Mac in 2 s (`convert.sh`); 4.0 MB WAD becomes 1.72 MB WHX |
| WHD and WHX reader, game-data decoders | `doom/r_data_whd.c`, `whddata.h`, `tiny_huff.c`, `image_decoder.c`, `musx_decoder.c` | No: plain C | Yes | Built, runs in the emulator |
| Level data, textures, patches and flats kept in the data file, not in RAM | `p_setup.c` raw map types, `r_data_whd.c`, `pd_render.cpp` | Needs the file to be readable by address | Yes | Runs in the emulator with the file mapped at an address; no such mapping exists on this chip (section 4) |
| Zone: 16-bit pointers in a 256 kB window, 8-byte block header, no purgeable blocks, thinker pool | `z_zone.c`, `doomtype.h`, `p_tick.c` | Assumes a 256 kB RAM window at a fixed base | Yes | Works; the window base is a build option (patch). The M33's SRAM is exactly 256 kB, so the window is the whole RAM |
| Const state tables, small integer types, trimmed structures | `info.c`, `doomtype.h`, many files | No | Yes | Built |
| New renderer: columns and spans kept as lists, drawn into two partial frame buffers | `pd_render.cpp` (3.1k lines of C++) | Partly: hardware interpolators, a second core, spin locks, the USB controller's spare RAM | Yes | Built with a C span loop in place of the interpolator (upstream has one), one core, spin locks as no-ops; two small patches |
| Frame buffers of 320 x 168 bytes, two of them | `pd_render.cpp`, `pico/i_video.c` | No | Yes | 105 kB of the static RAM |
| Video output with overlays: status bar, menu, HUD drawn at scan-out | `pico/i_video.c` (1.5k lines) | Yes: PIO VGA signal, scanline DMA, scratch RAM | Needs a panel driver instead | Not built; replaced by a stub that takes each frame |
| Sound: I2S output, software OPL synthesizer (emu8950), ADPCM sound effects | `pico/i_picosound.c`, `opl/`, `musx_decoder.c` | Output side yes (PIO I2S); synthesizer and decoders no | Optional | Not built; stubs |
| Keyboard | `pico/i_input.c` (TinyUSB) | Yes | Needs the BoosterPack joystick and buttons | Not built; stub |
| Multi-player over I2C, saved games into flash | `pico/piconet.c`, `pico/picoflash.c` | Yes | No | Stubs and `NO_USE_SAVE`, `NO_USE_LOAD` |
| System, timer, console | `pico/i_system.c`, `i_timer.c`, `stubs.c` | Yes (small) | Yes | Rewritten in `plat/platform.c` (about 200 lines) |
| Start-up and clocks, voltage, SDK calls | `i_main.c`, pico-sdk | Yes | Different here | `shim/` stands in for the headers the engine includes |

## 2. What is in this folder

| File | What |
|---|---|
| `defs.txt` | The build flags of upstream's `doom_tiny` target, copied from its CMake files, plus the M33 ones |
| `srclist.txt` | The upstream files built (78) |
| `shim/` | Stand-in headers for the pico-sdk (`pico.h`, `pico/sem.h`, `hardware/interp.h` and others), a `config.h` |
| `plat/platform.c` | The platform layer: zone, console, clock, frame sink, stubs for sound, keys and network |
| `build.sh` | Fetch, patch, compile with TI Arm Clang, link into the real 256 kB RAM; prints sizes |
| `convert.sh` | Builds `whd_gen` and converts the reader's own `DOOM1.WAD` into `wad-cache/DOOM1.whx` (git-ignored) |
| `../../../third-party/rp2040-doom/` | Pinned fetch of upstream and the patches (`patches/0001-m33-whx.patch`, 3 files) |
| `patches/0002-whx-page-cache.patch` | Section 6: 22 upstream files, every dereference of the data file goes through the cache (about 200 changed lines). Applied only when `WHXCACHE` is set |
| `patches/0003-pixels-init.patch` | One line, always applied: zero the `pixels` array in the composite-column drawer, so the frames no longer depend on what the stack held (section 6, "Correctness"). `NOPXINIT=1` leaves it out |
| `shim/fox_whx.h`, `plat/fox_whx.c` | The cache: 256-byte pages, 2 ways, least recently used, a one-page fast path; the miss path is one `qspi_read` call |
| `emu_qspi.py` | The emulator harness of `../host/emu_m33.py` plus a QSPI device model, a trap on any direct read of the data file, per-frame checksums and cache counters |
| `tools/whx_classes.py`, `tools/cachesim.c` | Maps each 128-byte unit of the data file to a class (flats, sprites, level segs and so on); a trace-driven cache simulator used to choose the design before the firmware existed |
| `plat/edu.c`, `board/doom_board_edu.syscfg`, `tools/edu_dump.py` | The Educational BoosterPack side (section 8): screen, stick and buttons, and a reader of the image's state over the probe |
| `board/`, `plat/board.c`, `tools/qspi_host.py`, `tools/run_board.py` | Board side: read-only dump and guarded write firmware, the engine's platform glue (SDK quad read, DWT timing, frame checksums), the host scripts. `BOARD=1 build.sh` (section 7) |

## 3. Build and run

```bash
cd ports/doom
./fetch.sh                                   # the WAD (the converter reads it)
m33-whx/convert.sh                           # wad-cache/DOOM1.whx
m33-whx/build.sh 44                          # zone of 44 kB; needs the SysConfig output used by the other M33 builds
python3 -I host/emu_m33.py <doom.out> wad-cache/DOOM1.whx --nm <tiarmnm> --ram-kb 256 --ms-per-read 35 --stop-demos 3
```

`ZSTATS=1 m33-whx/build.sh 44` adds the zone high-water counter the run prints.

With the cache (section 6): `WHXCACHE=16 WHXSTATS=1 m33-whx/build.sh 30` builds the cached image (16 kB of pages; `WHXSTATS` adds the hit and miss counters, leave it off to count instructions); run it with `python3 -I m33-whx/emu_qspi.py <doom.out> wad-cache/DOOM1.whx --nm <tiarmnm> --ram-kb 256 --ms-per-read 35 --qspi --stop-demos 3`. The `--qspi` run does not map the data file at all: a direct read stops the run and names the function.

## 4. Numbers

Measured on the linked image. kB as above.

| What | Value |
|---|---|
| Code | 134.7 kB |
| Read-only data | 95.0 kB (plus 1.1 kB init image) |
| Flash total | 230.8 kB of the chip's 1,024 kB |
| Converted data file (WHX) | 1,758 kB (4.0 MB WAD in; 10.7 percent of the board's 16 MB QSPI (21 percent of the 8 MB the board guide lists), 1.7 times the chip's flash) |
| Static RAM (data 2.7, bss 183.2, stack 8.0, heap 4.0) | 198.0 kB |
| Largest static items | frame buffers 105.0 kB, render work list 46.2 kB, visplane flags 6.6 kB, patch decoder 4.5 kB, patch lists 3.0 kB |
| RAM left for the zone in 256 kB | 58.0 kB |
| Zone in use, high water over the three demos | 28.9 kB, counted inside the engine |
| Smallest zone that finished all three demos | 30 kB (29 kB and 28 kB stopped in demo 2 with "out of memory") |
| RAM needed to run the demos | 198.0 + 30 = 228 kB of 256 kB, 28 kB spare |
| Same measure for the plain engine (experiment page, set S3) | 163.8 kB static plus 800 kB zone = 964 kB, 4.2 times more |
| Instructions per displayed frame, demo1 | 2.48, 2.43 and 2.75 million in three 20-frame windows (mean 2.55 million; three game tics per frame in this run) |
| Frame time at 160 MHz from that | 15.9 ms at one cycle per instruction, 23.9 ms at 1.5; one tic is 28.6 ms |
| Demo lengths seen by the engine | gametic 5,197, 9,235 and 11,571 at the three demo ends (the demos are 5,026, 3,836 and 2,134 tics; the rest is attract-mode pages, as in the Mac run) |

What the number does not include. The display side (scanline buffers, the overlay palettes), the sound mixers and the software OPL synthesizer are not built, and their RAM is not in the 198.0 kB. From upstream's notes and source they are small, a few kB each (its own zone-plus-heap figure of 58 kB minus 52 kB is about 6 kB for audio and scanline buffers); together an estimated 10 to 15 kB, which the 28 kB of spare covers. That is an estimate, not a build. The frame time is the CPU alone: no reads of the data file, no panel.

The three demos are three of the nine levels; the zone high water on the others is not measured.

Zone in use per demo, plain engine against this design (the first number is the floor of blocks that cannot be freed, from the experiment page; the second is the high water of bytes in use):

| Demo | Plain engine, set S3 | m33-whx | Saved |
|---|---|---|---|
| demo1 | 532.2 kB | 26.3 kB | 505.9 kB |
| demo2 | 593.0 kB | 28.9 kB | 564.1 kB |
| demo3 | 558.3 kB | at most 28.9 kB (the high water did not rise in demo 3) | at least 529.4 kB |

The saving was measured only as a whole. A per-squeeze breakdown was not run, because the squeezes are not independent in this code (level data read in place, no purgeable blocks and 16-bit pointers all change the same allocations). The single-squeeze figures that exist are from the plain engine on the Mac (experiment page, 64-bit sizes): lumps used in place 107 kB, no screen melt 156 kB. For another target, such as the LP-EM-CC35X1 (`../target-cc35x1`), the reusable parts are `shim/`, `defs.txt`, the patch in `third-party/rp2040-doom/patches/` and the converter; `plat/platform.c` and the linker placement are the only chip-specific files.

## 5. What is left

1. **Reading the data on the board.** Solved in the emulator (section 6) and on the chip (section 7). The QSPI controller in SDK 1.04.00.00 has no memory-mapped window, and 1.72 MB does not fit the 1 MB of internal flash beside 0.23 MB of code, so the cache reads the QSPI. Measured on the chip: a miss costs 23.8 microseconds (section 7). Open: sound next to the cache.
2. **A panel driver.** Done for the Educational BoosterPack's screen (section 8): the view and the title pages, with a text line in place of the status bar. The menu and the other overlays are not drawn.
3. **Sound** (compiled out) and **joystick input** (done, section 8; played by hand, 2026-11-04).
4. **More levels.** The three demos cover three of the nine; other levels may need a bigger zone.
5. **QSPI dump first.** Done (section 7): the flash is 16 MB, the video in the first 197 kB is unchanged, the data file is at 0x640000.

## 6. Reading the data file through a software page cache

Added 2026-10-21 (experiment page, section 12). The chip's QSPI has no memory-mapped window, so the engine cannot follow a pointer into the 1.72 MB file. Question: can a software page cache in the 28 kB of spare RAM serve every read, and what does it cost in time?

**Expectation (written from a read trace, before the firmware existed).** The first 740 frames of demo 1 read the file 56.7 million times, 77 thousand times a frame. A least-recently-used cache of 256-byte pages, 2 ways, simulated on that trace (`tools/cachesim.c`) misses 338 times a frame at 16 kB and 554 at 8 kB, which is 5 to 9 ms of QSPI time a frame at 40 MHz quad. More than half of the misses are level geometry (lines, vertexes, nodes, segs); two fifths are sprite, wall-patch and status-bar data.

**What was built.** Every engine pointer into the file keeps its value (`TINY_WAD_ADDR` plus file offset); only the dereference changes. `shim/fox_whx.h` gives `FW_U8`, `FW_U16`, `FW_U32`, `FW_COPY` and `FW_VIEW`; each checks the pointer against the file's address range, tries a one-page fast path and falls back to a 2-way lookup, and a miss is one `qspi_read(offset, dst, n)` (`plat/fox_whx.c`). Patch `0002-whx-page-cache.patch` changes 22 upstream files:

| Access | Where it was wrapped |
|---|---|
| Lump directory, names, sizes, header | `w_wad.c`, `w_wad.h`, `w_file_memory.c`, `d_main.c` |
| Every bit stream (wall patches, sprites, flats, composite textures, the demo) | the five byte fetches in `tiny_huff.h`, so one place covers every decoder |
| Table walks: texture table, composite-column metadata, sprite tables, flat translation, status-bar patch numbers | `r_state.h`, `r_data.h`, `r_data_whd.c`, `r_segs.c`, `pd_render.cpp`, `r_things.c` |
| Level data kept in place: lines, sides, segs, subsectors, vertexes, nodes, blockmap, reject, things, sectors | `r_state.h` macros, `p_setup.c`, `p_maputl.c`, `p_sight.c`, `r_bsp.c`, `r_main.c`, `p_mobj.c` |
| Status-bar and menu patches | `v_patch.h`, `v_video.c`: palette copied per patch, pixel data by `FW_U8` |
| Light tables (8.5 kB, read once per drawn pixel) | copied to RAM at start-up (`r_data_whd.c`), not cached; wrapping every lookup was not built |

The emulator (`emu_qspi.py --qspi`) does not map the file at all. A read of its address range stops the run and names the function, so a missed site shows up at once. The QSPI is a four-register device model: each read costs `400 + (14 + 2 x bytes) x 4` CPU cycles. The 40 MHz clock and quad I/O read with 6 dummy clocks are the SDK's DMA example (`qspi_flash_readback_DMA`, CPUCLK 160 MHz divided by 4); the 400 cycles for command set-up, DMA start and completion were an assumption until the board run (section 7: a miss took 23.8 microseconds, 3.8k cycles). The MSPM33C321A data sheet was not read for a higher QSPI clock.

**Correctness.** All three demos (3,881 frames, three of the nine levels) finish with the same gametic and zone figures as the plain build and no direct read of the file. Frame checksum of all frames: `b22b3fde`, against `e9db8f1d` for the plain build. They differ in 11 frames and 20 pixels (frame 3362: one pixel, row 167, column 40, 83 in the plain build and 2 in the cached one). The cause is not the cache: `draw_composite_columns` draws entries of its stack array `pixels[129]` that it has not written in that call, so the pixel is whatever the stack held. With that array zeroed (`patches/0003-pixels-init.patch`, called `PXINIT=1` when it was optional) the plain and cached builds draw identical frames, checksum `fc7b6c2d` over all 3,881, and the plain build alone changes in 9 of those 11 frames. Since 2026-10-21 the patch is always applied; the cached build gives `fc7b6c2d` at -Os, -O1 and -O2 (section 8, "The stack array"). A heap size change on the plain build alone changes nothing, and cache sizes of 8, 12, 16 and 19 kB all give `b22b3fde`.

**Result (one run per row; the cache counters and QSPI time are exact, the instruction counts are exact in 7 windows of 20 frames).**

Cache size, 256-byte pages, 2 ways, 40 MHz quad, all three demos:

| Cache | RAM used of 256 kB | Free | Hits | Misses a frame | QSPI wait a frame, mean (worst) |
|---|---|---|---|---|---|
| 8 kB | 244.6 kB | 11.4 kB | 98.30 % | 549 | 8.6 ms (18.2 ms) |
| 12 kB | 248.6 kB | 7.4 kB | 98.74 % | 407 | 6.4 ms (15.6 ms) |
| 16 kB | 252.7 kB | 3.3 kB | 98.89 % | 360 | 5.6 ms (13.3 ms) |
| 19 kB | 255.7 kB | 0.3 kB | 99.06 % | 303 | 4.8 ms (10.3 ms) |

The RAM figure includes the 30 kB zone, the 8.7 kB of light tables and the cache with its tags. The plain build needs 228.0 kB. Display, sound and keys are not built and were estimated at 10 to 15 kB (section 4), so the cache that fits next to them is about 8 kB. Hits are accesses served from a cached page, counting one access for every value read or page chunk copied (125.7 million over the three demos).

Per demo (16 kB): hits 99.00, 98.82 and 98.76 percent; 329, 359 and 432 misses a frame; 5.1, 5.6 and 6.8 ms. The busiest demo is demo 3.

Page size, 16 kB cache:

| Page | Misses a frame | QSPI wait a frame | Same, 1k cycles of driver software a read | Same, 2k cycles |
|---|---|---|---|---|
| 256 B | 360 | 5.6 ms | 7.0 ms | 9.2 ms |
| 128 B | 448 | 4.1 ms | 5.8 ms | 8.6 ms |
| 64 B | 616 | 3.7 ms | 6.0 ms | 9.9 ms |

Smaller pages win in the model because 8 CPU cycles a byte dominates; 128 bytes is the best or near-best at every driver overhead tried (400, 1k and 2k cycles a read). The first row is what is built by default; `WHXPAGE=7` builds 128 bytes. A plain one-bit fast read (the safe command when the quad-enable bit is clear) is 3.5 times slower per page: 19.5 ms a frame at 16 kB and 256 bytes, arithmetic on the measured read counts.

Frame time at 160 MHz, mean of 7 windows of 20 frames across the three demos (3 game tics per displayed frame in this mode; one tic is 28.6 ms):

| | Instructions a frame | 1.0 cycle each | 1.5 | 2.0 |
|---|---|---|---|---|
| Plain, CPU only | 2.50 million | 15.6 ms (64 frames/s) | 23.4 ms (43) | 31.2 ms (32) |
| Cached, 16 kB, QSPI wait included (5.7 ms in these windows) | 3.60 million | 28.2 ms (36) | 39.4 ms (25) | 50.7 ms (20) |

The wrappers add 44 percent to the instruction count (1.36 to 1.48 times in the seven windows); the 125.7 million accesses are about 32 thousand a frame at roughly 34 extra instructions each, and 27 percent of them miss the one-page fast path and take the 2-way lookup. Cycles per instruction on this chip are not measured; the LP-EM-CC35X1 build of this engine ran at 22.5 frames/s on 2.55 million instructions a frame, which is about 2.8 cycles each with its own flash. At 1.0 cycle the cached build is at the 35 frames/s the game needs; at 1.5 or more it is not.

**What it means.** The data problem is closed in the emulator: no direct read remains on the paths the demos use, the hit rate is above 98 percent, and the QSPI wait is smaller than the CPU time. The RAM is the limit: the cache and the light tables take 25 kB of the 28 kB spare, so the display and sound have 3 kB at the 16 kB size or about 11 kB at 8 kB, and the extra instructions put the frame rate under the real-time rate unless a cycle costs about one clock. Cheaper accessors (a two-page fast path, copying a whole node or line record at once) and a smaller zone are the next squeezes.

**Not covered.** The six levels the demos do not play; save and load (`p_saveg.c`), respawn in deathmatch 2 (`P_RespawnSpecials`), the automap and the sound and music paths (the sound files are read by code that is stubbed here) were wrapped where they read the file or left alone and were not run. A site that was missed would show only when a run reaches it. The board has not run any of this.

## 7. On the board (LP-MSPM33C321A, 2026-10-21)

Account, changes from the plan and reproduction: `docs/experiments/2026-10-21-mspm33c321a-qspi-dump-doom-data.md`. Build: `WHXCACHE=8 BOARD=1 WHX_CRC32=0xe9018e36 m33-whx/build.sh 30` (the pixels patch is part of every build since section 8) (SysConfig from `board/doom_board.syscfg`: 160 MHz, QSPI pins; `plat/board.c`; TI's `QSPI_FLASH.c`). The clock steps 35 ms a read as in the emulator, so the demos play the same tics and the frame checksums are comparable.

**Expectation (before the run):** same checksums and cache counters as the emulator; 41 ms a frame at 8 kB, 37 ms at 16 kB; QSPI wait about 12 ms at 8 kB.

| | Result |
|---|---|
| QSPI part | ID C2 20 18, 16 MB; video in the first 197 kB; data file at 0x640000; dump before and after identical outside the file |
| Frame checksums after demo 1, 2, 3 | `fe183649`, `c6686ada`, `fc7b6c2d`, equal to the emulator; cache misses equal count for count (2,131,318) |
| Frame time, 8 kB cache | 50.3 ms (19.9 frames a second), QSPI wait 13.1 ms, 10.7 kB RAM free (without the screen) |
| Frame time, 16 kB cache | 45.7 ms (21.9 frames a second), QSPI wait 8.6 ms, 2.7 kB RAM free |
| One miss | 23.8 microseconds (3.8k cycles); the model assumed 400 cycles of software |
| CPU part of a frame | 37.1 ms, 1.65 cycles an instruction |

Three game tics a frame in this mode: 59.7 tics a second at 8 kB against 35 needed. The display, keys and sound are still stubs; real-time-paced frame rate not measured.


## 8. With the Educational BoosterPack: the demos on its screen, and a play mode (LP-MSPM33C321A, 2026-10-21)

Question: can the page-cache build draw its frames on the BoosterPack's 128 x 128 ST7735S in the 8.3 kB of RAM the 8 kB cache leaves, read the stick and two buttons, and keep the frame rate and the game's behavior? Account: `docs/experiments/2026-10-20-doom-smallest-ti-chip.md` section 15.

**Expectation (reasoned before the first run, not written to a file).** The 128 x 96 window takes 10 to 12 ms on the wire at 16 MHz, hidden behind the 37 ms of CPU work because the DMA runs on its own; the CPU pays the row conversion (about 1 ms a frame) and the text line (rare). So a frame should take 51 to 52 ms instead of 50.3, and the frame checksums should not move.

**What was built.** `EDU=1` adds `plat/edu.c` and `board/doom_board_edu.syscfg` (the board image's clock tree and QSPI plus the BoosterPack's GPIO, SPI with DMA and HSADC pins from `portability/boosterpacks/edumkii/boards/mspm33c321a`; the SPI block's unused chip-select pin lands on PA26, away from the QSPI and the BoosterPack pins). The BSD-3 panel core of `portability/boosterpacks/edumkii` does the reset, the init sequence, the windows and the text.

| Part | Choice | Why |
|---|---|---|
| Picture | 320 x 200 scaled to a 128 x 96 window in the middle of the 128 x 128 panel, black above and below | 4:3 is the shape Doom is drawn for (its pixels are 1.2 times taller than wide). Stretched to 128 x 128 the picture would be 33 percent too tall and the wire time 25 percent longer |
| Game frame | The 320 x 168 view (the status bar is not in the frame buffer) fills the top 80 rows of the window; 168 source rows over 80 | 96 / 200 of the height; the 16 rows left under it carry two text lines: health, armor, ammo, then DEMO or PLAY and the frames a second |
| Page | Title and other full pages (the 200 rows are spread over both frame buffers) fill all 96 rows | Drawn while the engine waits, since both buffers are in use |
| Across | Two source pixels averaged at every 2.5 source pixels; a brightness lift of 255 * (v / 255)^0.75 on the palette | The panel is dim next to a monitor; the lift is one 256-byte table in flash |
| Streaming | No full RGB565 frame. Two 256-byte row buffers; the SPI DMA sends one row and the interrupt raised by its end starts the next row and converts the one after (palette table, 512 bytes) | The engine renders the next frame into the other 320 x 168 buffer while the previous one is on the wire. The buffer that was shown is handed back only when the new one has started |
| Palette | The engine's palette number (damage and pickup tints included) rebuilt from the data file's PLAYPAL lump when it changes, as upstream's `i_video.c` does | Tints are computed because the converted file keeps one palette |
| Keys | Stick up and down: forward and back; left and right: turn; S1 fires; S2 uses. In the demo loop a press of S1 or S2 starts E1M1 (medium); after that the keys go in as keyboard events | The stick polarity (right is the high X reading, down the high Y reading) is the driver's assumption (`edumkii.h`); the stick press is not usable on this LaunchPad (PA21 is VREF- and R10 ties it to ground), so no menu key |
| Sound | Compiled out; the buzzer pin is held low | Keeps the board silent and the RAM free |

RAM: the display adds 2.4 kB (palette 0.5, row buffers 0.5, driver state and its line buffer 0.6, the rest in small variables), leaving 8.3 kB of the 256 with the 8 kB cache and 151 bytes with the 16 kB cache. Flash: 306 kB of the chip's 1,024 kB (code 206 kB, read-only data 98 kB).

**Result (all three demos, display on, one run per row; frame time is the DWT cycle count between frames).**

| Run | Frames a second (demo 1, 2, 3) | Frame time | Notes |
|---|---|---|---|
| 8 kB cache, stepped clock, checksums on | 19.3 (18.6, 19.8, 17.5) | 51.7 ms | Three game tics a frame as in section 7. Frame-checksum chain after each demo `fe183649`, `c6686ada`, `fc7b6c2d`: equal to the run without a display. Section 7 without the display: 19.9, 50.3 ms, so the display costs 1.5 ms a frame |
| 8 kB cache, real time, no checksums | 22.7 (22.4, 23.5, 21.1) | 44.0 ms | The game runs at its own 35 tics a second (gametic 5,197, 9,235, 11,571 at the demo ends, as before), about 1.5 tics a frame |
| 16 kB cache, real time, no checksums | 24.5 (24.1, 24.7, 22.8) | 40.9 ms | 151 bytes of RAM left; ran all three demos |

Display figures from the image's own counters (8 kB, real time, 7,585 frames): the 80-row stream takes 14.6 ms on average (17.6 ms at most; the wire time at 16 MHz is 10.2 ms, the rest is the gap the SPI block leaves between rows and the restart in the interrupt); the row conversion costs 1.46 ms a frame; the engine waited for the display 2.7 ms in all (0.4 microseconds a frame), so the stream is hidden; the text lines were redrawn 1,149 times. The expectation held for the checksums, the stream and the wait; the cost was 1.5 ms a frame against 1 to 2 expected.

**Play mode.** A scripted player (`SCRIPT=1`: S1 pressed at 8 s, then walk, turn and fire in a 6 s loop; the same events the buttons and stick produce) started E1M1 and played it: 79 key events, the player's position and angle changed, ammo 47 to 22, tics 641 to 2,328, 27 to 36 frames a second in E1M1 on the on-screen counter. The stick at rest read X 1787 and Y 2141 counts and S1 and S2 read released. Not measured: the zone high water in play (30 kB zone; the demos' high water was 28.9 kB).

**What a camera showed (the panel is about 100 pixels across in its frame).** Four color bars in the order red, green, blue, white (red reads magenta on this camera); the title page as the DOOM logo and the marine; text lines at the bottom; the view region changing from frame to frame in demos and in play. Scene detail is not resolved and the camera tints grays light blue. For the picture itself the two 320 x 168 buffers and the palette were read over the probe and drawn on the host with the same scaler (`tools/edu_dump.py`): a green marble room, E1M1 outdoors, the title page. The check images are `HALTPAGE=1` (stops on the title page), `SCRIPT=1` and `TESTMS=<ms>` (color bars).

**The stack array.** `patches/0003-pixels-init.patch` is now part of every build. Frame-checksum of all 3,881 frames of the cached build (8 kB, emulator): `fc7b6c2d` at -Os, -O1 and -O2 with the patch; without it `b22b3fde` at -Os and -O1 and `53ccda64` at -O2, so the -O2 build drew different frames from the same sources (emulator, 3,881 frames, one run each). For the LP-EM-CC35X1 target (not edited here): its `-O2` build that reached 39.3 frames a second changed the checksum of demo 3 for the same reason; applying this patch to its tree (one line after `uint8_t pixels[129];` in `draw_composite_columns`, `pd_render.cpp`) and rebuilding at -O2 should make the three checksums equal the -Os ones.

**Build and run.**

```bash
# real time, no checksums (the frame-rate image)
EDU=1 BOARD=1 WHXCACHE=8 RT=1 NOCRC=1 WHX_CRC32=0xe9018e36 m33-whx/build.sh 30
# stepped clock with checksums (compares with section 7)
EDU=1 BOARD=1 WHXCACHE=8 WHX_CRC32=0xe9018e36 m33-whx/build.sh 30
python3 -I ports/doom/m33-whx/tools/run_board.py <doom.out> <label>
python3 -I ports/doom/m33-whx/tools/edu_dump.py <doom.out> <prefix> --frames
```

