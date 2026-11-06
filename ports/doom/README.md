# Doom engine port for the LP-MSPM33C321A (host and emulator stage)

**Short answer.** Doom plays all three shareware demos to the end on the LP-EM-CC35X1 (39.2 frames/s) and on the LP-MSPM33C321A with the Educational BoosterPack screen (22.7 to 24.5 frames/s), with the frame checksums equal to the reference. The reader-facing account is [WRITEUP.md](WRITEUP.md). This README covers the first stage: the plain engine, which builds and links for the LP-MSPM33C321A and plays the demos in a Cortex-M33 emulator but does not fit its 256 kB of RAM (about 164 kB of static data and a zone of at least 800 kB). The rp2040-doom design in [`m33-whx/`](m33-whx/README.md) is what fits. The experiment page is [Doom, smallest TI chip](../../docs/experiments/2026-10-20-doom-smallest-ti-chip.md); license terms are in [LICENSING.md](LICENSING.md).

Terms: [vocab](../../docs/vocab.md). 1 kB = 1,024 bytes; 1 MB = 1,048,576 bytes. **Zone** is the engine's own memory pool; **WAD** is the game data file; **lump** is one item inside it; **QSPI** is the chip's serial flash interface.

## What exists

| Part | Where | State |
|---|---|---|
| Engine source | `fetch.sh` downloads doomgeneric at one pinned commit into `src-upstream/` (git-ignored) and applies `patches/` | GPL-2.0; no engine file is copied into the repository |
| Patch | `patches/0001-fox-red-labs-squeezes-and-target.patch` | direct frame output, no screen melt, file-less start-up, zone as a static array, const tables, trimmed render limits, zone and limit counters |
| Mac build and run | `host/build.sh`, `host/run_demo.sh`, `host/dg_headless.c`, `host/ppm2png.py` | works: frames to PNG every N, zone peak, checksum |
| M33 build | `target/build.sh`, `target/main.c`, `target/dg_mspm33.c`, `target/w_file_mem.c`, `target/fox_target.h`, `target/include/` | links with TI Arm Clang 5.1.1.LTS and the SysConfig output the other M33 builds use |
| Emulator | `host/emu_m33.py` (needs `pip install unicorn`) | runs the same ARM image; counts instructions; no timing |
| Data | the shareware WAD in `wad-cache/` (git-ignored) | never committed |

## Build and run

```bash
cd ports/doom
./fetch.sh                                   # engine at the pinned commit, patched; WAD checked by SHA-1
. ./srclist.sh                               # defines SQ_S0 to SQ_S3, the squeeze sets
host/build.sh s3 "$SQ_S3"                    # Mac build with all squeezes
host/run_demo.sh s3 demo1 35                 # demo1, a frame every 35 as PNG in build/frames/, zone and limit report
SRC=$PWD/src-upstream/doomgeneric target/build.sh Os 80 $SQ_S3     # M33 image; 80 kB zone is all that fits
python3 -I target/sizes.py <doom.out> <tiarmobjdump>               # flash and RAM by section
```

M33 image against the emulator (builds with no SysConfig start-up and two registers for time and console):

```bash
EMU=1 BIGRAM=1 SRC=$PWD/src-upstream/doomgeneric target/build.sh Os 1024 $SQ_S3 -DFOX_ZSTATS
python3 -I host/emu_m33.py <doom.out> wad-cache/DOOM1.WAD --nm <tiarmnm> --demo 2
```

`BIGRAM=1` gives the linker 4 MB of RAM so a build that does not fit the chip can still be sized and run.

## Numbers

See the [experiment page](../../docs/experiments/2026-10-20-doom-smallest-ti-chip.md) for the method and every row. Machine-readable rows: `RESULTS-ROWS.csv`.

## What is left

1. **QSPI dump.** Done for the rp2040-doom design (`m33-whx`): the board's flash is a 16 MB part, read twice with identical results, and the data file sits in a blank run of it.
2. **WAD load (plain engine).** `fox_wad_read` in `target/dg_mspm33.c` is a stub in this stage. It would become a QSPI command read (fast-read command, FIFO, DMA). The chip's QSPI has no memory-mapped window in SDK 1.04.00.00, so lumps are copied, not used in place; see section 4 of the experiment page. The reader copies DOOM1.WAD onto the flash; this repository never ships it.
3. **Memory.** Solved by the rp2040-doom design in `m33-whx/`. The plain engine in this folder links only with an 80 kB zone and cannot start the game.
4. **Screen.** `DG_DrawFrame8` receives the 320 x 200 index frame and the palette; a panel driver scales and sends it. Written for the Educational BoosterPack in `m33-whx/` (section 8); not in this plain-engine stage.
5. **Joystick and buttons.** `DG_GetKey` returns nothing. The Educational BoosterPack stick and buttons feed it in `m33-whx/` (played by hand, 2026-11-04).

The command to run when hardware work starts: `target/build.sh Os <zone kB> $SQ_S3` after step 3 has a zone size that fits, then flash the image with the board's programmer (see [WRITEUP.md](WRITEUP.md), Build it yourself).

Other targets: [`target-cc35x1/`](target-cc35x1/README.md) is the LP-EM-CC35X1 (CC3551E), WAD read in place from external flash. [`target-cc35x1-whx/`](target-cc35x1-whx/README.md) is the same board with the rp2040-doom design (`m33-whx` sources, WHX data in the image): all three demos finish, 39.2 frames/s (22.5 with the first memory layout).

## Porting lessons

- RAM decides, not speed. Static data is 164 kB and the zone needs at least 800 kB, against 256 kB of SRAM. The CPU has 2.7 to 4 times the speed the game needs (1.14M instructions a frame).
- The cheap squeezes cut static RAM from 295.6 kB to 159.8 kB: state tables made `const` (`.data` fell from 58.9 kB to 20.2 kB), render limits cut to measured maxima, no screen melt, no output buffer. None of them touch the zone.
- The stock platform layer allocates an output buffer of panel size times 4 bytes and draws nothing below 320 x 200 (its scaling factor becomes 0). Hand the 320 x 200 frame to the platform and scale while sending.
- This chip's QSPI has no memory-mapped mode in SDK 1.04.00.00, so level data is copied into the zone. A part with mapped flash and more data RAM (the CC35X1 target) reads lumps in place.
- Running the real ARM image in a Unicorn emulator gives exact 32-bit memory numbers with no board. Compare frame checksums over the first 400 frames only: later frames differ between runs on the Mac.
- On the CC35X1 (480 kB zone, lumps read in place) demo 1 runs to frame 2030 and demos 2 and 3 stop at frames 52 and 425; the sources of a 593 kB floor are level data and tables, not the engine's static data, so a bigger zone needs those out of RAM. A UART byte sent after boot did not reach the SDK driver, so build one image per demo.
- The rp2040-doom design runs on the CC35X1 with a 40 kB zone and finishes all three demos with frame checksums equal to the emulator. Its 16-bit pointers reach 256 kB, and static objects are pointed at too (`thinkercap`, `players`), so check the addresses of the zone and those statics against the window at link time. Use a virtual game clock when comparing frames between a slow and a fast host. Same-file calls cannot be wrapped by the linker: hook `G_CheckDemoStatus` with a one-line patch.
- No address window for the data file (LP-MSPM33C321A): a software page cache fixes it without moving a pointer, but every dereference of the file must change (22 upstream files, about 200 lines in `m33-whx/patches/0002-whx-page-cache.patch`). Leave the file unmapped in the emulator and trap every direct read: the run names the next site to fix. With 256-byte pages the cache hit 98.3 to 99.1 percent at 8 to 19 kB; the wrappers cost 44 percent more instructions, and the cache plus 8.5 kB of light tables took 25 kB of the 28 kB spare. Keep per-pixel tables in RAM, not behind the cache. Do not use a memory-read hook on code that reads uninitialized stack; use an MMIO region (a no-op hook changed the frames). Compare builds by one checksum per frame, not one for the run: 11 of 3,881 frames differed by 20 pixels, from a stack array the renderer draws before it is written (`m33-whx/README.md` section 6).
