# Doom on the LP-EM-CC35X1 and the LP-MSPM33C321A

Doom plays all three shareware demos to the end on two TI chips: 39.2 frames/s on the CC35X1 Wi-Fi MCU and 22.7 to 24.5 frames/s on the MSPM33C321A with its own 128 x 96 screen, frame checksums equal to the reference on the stepped-clock runs (the real-time runs were not checksummed). The reference is a Cortex-M33 emulator running the same image, with a one-line stack-array fix (see the last section). Tested on macOS with the host tools; the board runs need a debug probe.

Contents: [What it took](#what-it-took), [Every number and where it comes from](#every-number-and-where-it-comes-from), [Build it yourself](#build-it-yourself), [Questions](#questions), [Limits](#limits), [Upstream bug](#upstream-bug-an-uninitialized-stack-array).

Terms: [vocab](../../docs/vocab.md). **Demo** here is one of the three recorded attract-mode games in the shareware data, replayed by the engine. A **tic** is one step of game logic; the game needs 35 a second. 1 kB = 1,024 bytes; 1 MB = 1,048,576 bytes. **Zone** is the engine's memory pool. A **stepped clock** advances the game clock by a fixed 35 ms each time the engine reads it, so the frames drawn do not depend on chip speed. **Squeeze** means a change that cuts RAM use. **High water** is the most of the zone ever in use. **Lump cache** holds recently used pieces of the game data. **Render list** is the list of wall and floor pieces to draw. **doomgeneric** is the plain, portable Doom engine; **rp2040-doom** is the small-memory engine that fits. **WAD** is the game data file. **WHX** is the compressed data format that rp2040-doom's converter writes from the WAD. **QSPI** is the serial flash interface. **TCM** is the core's fast RAM. Checked 2026-10-21; TI Arm Clang 5.1.1.LTS; MSPM33 SDK 1.04.00.00; CC35X1 SDK 10.20.00.39.

| | LP-EM-CC35X1 (CC3551E) | LP-MSPM33C321A |
|---|---|---|
| Core, clock | Cortex-M33, 160 MHz | Cortex-M33, 160 MHz |
| RAM used | 260 kB, with a 40 kB zone | 256 kB chip; free with screen: 8.3 kB at 8 kB cache, 151 B at 16 kB (QSPI dump page: 10.7 and 2.7 kB without it) |
| Game data | read in place from the 8 MB flash | read from the board's 16 MB QSPI flash through a software cache |
| Game clock | stepped: the clock advances 35 ms each read, about 3 tics per frame, so the demo runs faster than real time | real time, 35 tics a second, about 1.5 tics per frame |
| Frames/s (frames/s means frames drawn per second) | 39.2, stepped clock | 22.7 (8 kB cache), 24.5 (16 kB cache), real time with screen; 19.9 and 21.9 stepped, no screen |
| Frame checksums equal to the reference | yes, on the stepped run | yes, on the stepped run (19.3 frames/s with screen) |
| Screen, keys, sound | none | Educational BoosterPack (EDUMKII) screen (128 x 96 picture in the middle of its 128 x 128 panel), stick and buttons; no sound |
| Demos finished | 3 of 3 | 3 of 3 |

The demos are three of the game's nine levels. The CC35X1 rate is on a stepped game clock, so each frame holds about 3 tics and the demo logic runs faster than real time. It was not paced to real time. The closest pair is 39.2 against 19.9 and 21.9 (both stepped, no screen). It is not exact: both use about 3 tics a frame, but the boards differ in memory and cache. The real-time MSPM33C321A frames (22.7 and 24.5) are cheaper than the stepped ones (19.9, 21.9) because a real-time frame holds about 1.5 tics, so there is less to compute; the 1.5 ms the screen adds is on top of that.

## What it took

**Stock engine: does not fit.** The doomgeneric engine, with every cheap squeeze applied, links for the MSPM33C321A with 164 kB of static data. Its zone needs 593 kB at the least and 800 kB to finish all three demos. The chip has 256 kB of RAM. For the stock engine RAM was the limit, not CPU speed: it needs about 1.1 million instructions a frame. The rp2040-doom build needs 2.5 to 3.6 million instructions a frame, plus memory wait, so there the CPU matters.

**The rp2040-doom design: fits.** rp2040-doom keeps level data, textures and graphics compressed in the WHX file and decodes them while a frame is drawn. It also uses 16-bit pointers into a 256 kB window and a new renderer. Built for the M33, it needs 198 kB of static data and a 30 kB zone, 228 kB in all. The zone's high water over the three demos is 28.9 kB, against 593 kB for the stock engine.

**The data file has to be reachable.** The WHX file is 1,758 kB, 1.7 times the MSPM33C321A's 1 MB of flash. Its QSPI controller sends commands and fills a FIFO. SDK 1.04.00.00 has no memory-mapped mode for it. A software page cache replaces every read of the file: 256-byte pages, 2 ways, in the RAM that was spare. About 200 lines changed in 22 upstream files. Hit rates are 98.3 percent at 8 kB and 99.1 percent at 19 kB. The wrappers add 44 percent to the instruction count. On the chip the 8 kB cache leaves 10.7 kB of RAM free, which is where the screen driver goes.

**The CC35X1 started slow.** It reads the file in place from its flash, so the same engine ran with no cache. The first layout, with everything in the 512 kB DRAM, gave 22.5 frames/s. A load costs 1.06 cycles in TCM, 1.6 in code RAM (1.8 for a store), 6.2 in DRAM and 44 from the flash. Moving the statics, zone, stack and heap into the 128 kB TCM gave 34.5 frames/s. The render list in TCM, a 4 kB stack and a 28 kB lump cache in code RAM took it to 36.7. Compiling the render files at -O2 gave 39.2, once an engine bug was fixed (below).

| Step on the CC35X1 | Frames/s |
|---|---|
| Everything in DRAM, data in flash | 22.5 |
| Statics, zone, stack in TCM | 34.5 |
| Plus lump cache, render list in TCM | 36.7 |
| Render files at -O2, stack-array fix applied | 39.2 |

## Every number and where it comes from

Experiment page: [Doom, smallest TI chip](../../docs/experiments/2026-10-20-doom-smallest-ti-chip.md) (sections 3, 10 to 16). Board run: [QSPI dump and data load](../../docs/experiments/2026-10-21-mspm33c321a-qspi-dump-doom-data.md).

| Number | Value | Source |
|---|---|---|
| Error when the stock engine's zone runs out on the CC35X1 | `Z_Malloc: failed on allocation of 32792 bytes` (demo 1, frame 2030, zone 477 kB of the 480 kB it was given) | experiment page, section 9 |
| Stock engine, static data | 163.8 kB | experiment page, section 3 |
| Stock engine, zone floor / least zone that finished | 593.0 kB / 800 kB | experiment page, section 3 |
| rp2040-doom design, static data | 198.0 kB | `m33-whx/README.md`, section 4 |
| Same, zone high water / least zone that finished | 28.9 kB / 30 kB | `m33-whx/README.md`, section 4 |
| WHX file from a 4.0 MB WAD | 1,758 kB | `m33-whx/README.md`, section 4 |
| Page cache hit rate, 8 / 16 / 19 kB | 98.30 / 98.89 / 99.06 percent | `m33-whx/README.md`, section 6 (emulator) |
| One QSPI miss on the chip | 23.8 microseconds | QSPI dump page |
| MSPM33C321A frame time, 8 kB / 16 kB cache, no screen | 50.3 / 45.7 ms | QSPI dump page |
| QSPI wait a frame, 8 kB / 16 kB | 13.1 / 8.6 ms | QSPI dump page |
| CPU part of a frame on the M33 | 37.1 ms, 1.65 cycles an instruction | QSPI dump page |
| MSPM33C321A with screen, real time, 8 kB / 16 kB | 22.7 / 24.5 frames/s | `m33-whx/README.md`, section 8; row in `m33-whx/RESULTS-ROWS-wave5.csv` |
| Screen cost | 1.5 ms a frame, 2.4 kB of RAM | `m33-whx/README.md`, section 8 |
| CC35X1 memory access cost: TCM / code RAM / DRAM / flash | 1.06 / 1.6 / 6.2 / 44 cycles | `target-cc35x1-whx/README.md`, memory cost table |
| CC35X1 frame rate, final | 39.2 frames/s | row in `target-cc35x1-whx/RESULTS-ROWS-wave5.csv` |
| CC35X1 flash / RAM | 2,017 kB / 260 kB (40 kB zone) | `target-cc35x1-whx/README.md` |

Every measured number is a row in a `RESULTS-ROWS*.csv` file in the target's folder (the older `RESULTS-ROWS.csv` files are superseded by the `wave5` files: they still hold a waiting row for a board run since made, the pre-fix checksum chain `e9db8f1d` and a 22.53 frames/s row). Correctness is checked by frame checksum. The reference is the Cortex-M33 emulator running the same image. The chain value after each demo is `fe183649`, `c6686ada`, `fc7b6c2d` on both chips, with the same demo lengths (1,736, 3,092 and 3,881 frames) and the same cache miss counts on the MSPM33C321A. The frame rates are one run per image. Code layout moves the CC35X1 rate by up to 7 percent.

## Build it yourself

You need TI Arm Clang 5.1.1.LTS, SysConfig and the SDK for your board (MSPM33 SDK 1.04.00.00; CC35X1 SDK 10.20.00.39 with its image Toolbox), a C++ compiler on the Mac for the converter, a debug probe, and for the emulator `pip install unicorn`.

You bring the game data. The shareware `DOOM1.WAD` may be copied but not changed or sold, so this repository never contains it, or the WHX file made from it. The converter is the only thing shipped. [LICENSING.md](LICENSING.md) has the terms. The engine is GPL-2.0 and `fetch.sh` downloads it at a pinned commit.

```bash
cd ports/doom
./fetch.sh
```

`fetch.sh` downloads the engine, applies the patches and checks the WAD against SHA-1 `5b2e249b9c5133ec987b3ea77596381dc0d6bc1d`. It then converts the WAD with the pinned converter:

```bash
m33-whx/convert.sh
```

The output is `wad-cache/DOOM1.whx`, git-ignored. Its SHA-1 should be `066413a38303c3d36471ad887b798bc91275793b`.

LP-EM-CC35X1 (data linked into the signed image, read in place):

```bash
target-cc35x1-whx/build.sh
```

The signed image files land in the build folder the script prints (`<build>` below). Flash them with the Wi-Fi Toolbox command in step 4 of [the DNS sinkhole page](../dns-sinkhole/README.md), then reset the board and read the console. Use one command at a time on a debug probe.

The stock-engine target has its own script, `target-cc35x1/flash.sh`. The console runs at 921,600 baud and prints a checksum every 100 frames, then `DEMOEND` for each demo and `DONE`.

LP-MSPM33C321A with the Educational BoosterPack (data written to the QSPI flash first):

```bash
EDU=1 BOARD=1 WHXCACHE=8 RT=1 NOCRC=1 WHX_CRC32=0xe9018e36 m33-whx/build.sh 30
```

Run these steps in this order, as the QSPI dump page shows:

1. Read the board's QSPI flash twice with `qspi_host.py` and compare the two dumps. This board's flash holds its out-of-box video in the first 197 kB, and the dumps prove the read is stable.
2. Write `DOOM1.whx` at offset 0x640000 with the guarded write. It refuses to touch the first 197 kB.
3. Run the image with `run_board.py` and watch for `whx in flash: crc ... (match)`.

```bash
python3 -I ports/doom/m33-whx/tools/qspi_host.py write ports/doom/wad-cache/DOOM1.whx 0x640000 <dumpA.bin>
python3 -I ports/doom/m33-whx/tools/run_board.py <doom.out> <label>
```

`<dumpA.bin>` is the first dump from step 1. The steps, guards and scripts are in the [QSPI dump page](../../docs/experiments/2026-10-21-mspm33c321a-qspi-dump-doom-data.md). Its flash is a 16 MB part (JEDEC `C2 20 18`), not the 8 MB the board guide lists.

No emulator or Mac build is needed to run on a board. To check frames without hardware, `host/emu_m33.py` and `m33-whx/emu_qspi.py` run the same ARM image (`pip install unicorn`).

## Questions

**The MSPM33C321A boots but the demos never start.** Check the line `whx in flash: crc ... (match)` that `run_board.py` prints. `MISMATCH` means the data file is not at 0x640000 in the QSPI flash, or the write did not finish. Rewrite it with the guarded write script.

**The CC35X1 stops with `Z_Malloc: failed on allocation of 32792 bytes`.** That is the stock engine, which runs out of zone. Build the `target-cc35x1-whx` image instead.

**Frames differ from the reference at -O2, or in demo 3.** Only if you built without `m33-whx/patches/0003-pixels-init.patch` (`target-cc35x1-whx/patches/0002-pixels-init.patch` for the CC35X1 target). Apply the stack-array fix below.

**Why does the stock engine not fit?** The stock (doomgeneric) engine needs at least 593 kB for its zone and the chip has 256 kB of RAM. That is a measured fact for that engine only; the rp2040-doom design fits.

**The board guide says the QSPI flash is 8 MB.** The chip's ID (`C2 20 18`) and SFDP density say 16 MB.

**Does writing the data file erase the board's video?** Not with the guarded write: the video sits in the first 197 kB, the file goes at 0x640000, and a dump after the write matched the dump before outside the file.

**Why 22.7 frames/s and not 35?** Each frame takes 44 ms with the screen at 8 kB cache. The game still advances at 35 tics a second, so the picture updates less often than the logic.

**Why does the CC35X1 have no screen?** No screen driver has been written for it.

**Will it run on the LP-MSPM0G3519 or LP-MSPM0G3507?** No. The two frame buffers alone are 105 kB, over the 32 kB chip and 128 kB with the rest of the static data.

**Is this the whole game?** No. Only the three recorded demos ran, The MSPM33C321A has also been played by hand.

**Can I use Freedoom instead of the shareware WAD?** Not tested. Only the shareware `DOOM1.WAD` was converted.

**Will it run on the LP-MSPM33C321A without the BoosterPack?** The engine runs with the display stubbed: 21.9 frames/s (16 kB cache, stepped clock, no screen), and the QSPI data file is still needed.

**Why is the CC35X1 faster than the MSPM33C321A at the same 160 MHz?** Its data sits in 1-cycle fast RAM and flash is read in place, while the MSPM33C321A pays 23.8 microseconds per cache miss on QSPI. The frame rates are also on different clocks (stepped against real time).

**How long does a build take?** Not timed here.

**Where do I get DOOM1.WAD?** `fetch.sh` downloads it and checks the SHA-1.

## Limits

- Screen, stick and buttons exist only on the MSPM33C321A. Scripted key events played E1M1 from tic 641 to tic 2,328. It has since been played by hand with the stick and buttons; the frame rate during hand play was not timed.
- No sound on either chip. It is compiled out.
- The CC35X1 build has no screen or keys: no driver has been written for it.
- Only the three demo levels were run. The six other levels, save and load, the menu and the automap were not.
- The MSPM33C321A real-time rates are 22.7 and 24.5 frames/s. With a stepped clock and no screen they were 19.9 (8 kB) and 21.9 (16 kB); with the screen at 8 kB, 19.3.
- The two smaller MSPM0 chips (LP-MSPM0G3519, 128 kB; LP-MSPM0G3507, 32 kB) do not hold the static data of this design (RAM).
- The checksum chain applies to the engine with the stack-array fix below. Without it, demo 3 differs at -O2.
- Only the unchanged shareware demos were played, not the whole game. No Freedoom data was converted.

## Upstream bug: an uninitialized stack array

`draw_composite_columns` in rp2040-doom's renderer (`pd_render.cpp`) draws entries of its stack array `pixels[129]` that it has not written in that call. The pixel it draws is whatever the stack held. In the 3,881 demo frames this changed 11 frames by 20 pixels in total. At -O2 it changed the frames more, and the CC35X1 demo 3 checksum with them. One line after `uint8_t pixels[129];` that zeroes the array makes -Os, -O1 and -O2 draw identical frames (`fc7b6c2d` over all 3,881). The fix is `m33-whx/patches/0003-pixels-init.patch`, applied by every build here.

The name Doom is used only to say what runs. This repository has no Doom logo, artwork or captured frames.

Sources: files named above; experiment pages in `docs/experiments/`; <https://github.com/kilograham/rp2040-doom>.
