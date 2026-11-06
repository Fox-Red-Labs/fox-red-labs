# BOOSTXL-SHARP128: a 1-bit screen and a microSD card on a TI BoosterPack, from five LaunchPad kits

Code for the BOOSTXL-SHARP128 (a 128 x 128 black-and-white memory LCD plus a microSD slot) that builds for the LP-MSPM0G3519, LP-MSPM0G3507, LP-MSPM0L1306, LP-MSPM0C1104 and LP-MSPM33C321A. TI ships no example for this BoosterPack on any of them. Status: **Works** on the LP-MSPM0C1104 without a frame buffer; **Should work** (builds, not run on the board) on the other four kits. The panel and the player are tested on a Mac against a simulated panel and card. The microSD card path has not run on a board (see "Results").

Terms: [docs/vocab.md](../../../docs/vocab.md). 1 kB = 1,024 bytes. **VCOM** is the panel's polarity-flip bit, which has to change at least once a second. **FatFs** and **Petit FatFs** are ChaN's FAT file system libraries.

## What is in the folder

| File | What it does |
|---|---|
| `sharp_lcd.c`, `sharp_lcd.h` | The panel protocol: full update, partial update of any rows, clear, VCOM. Plain C. |
| `sharp_gfx.c`, `sharp_gfx.h`, `sharp_font5x7.h` | Text in a 5 x 7 font, lines, rectangles, circles, 1-bit pictures, drawn into a band of the screen. |
| `sharp_sd.c`, `sharp_sd.h` | microSD over SPI as a disk driver for FatFs and Petit FatFs (read only). |
| `boards/mspm0_common/port_mspm0.c` | The one port for every MSPM0 kit: pins, SPI, a millisecond clock. |
| `boards/<board>/sharp128.syscfg` | The pin map for that kit, as a SysConfig file. `boards/gen_syscfg.py` writes the four MSPM0 files from one table. |
| `boards/build_mspm0.sh`, `boards/build_mspm33.sh` | Build with TI Arm Clang. Nothing is flashed. |
| `examples/hello/hello.c` | Title, shapes, a counter, then the card's file list and the first lines of a file. |
| `player/player.c`, `player/video_to_frames.py` | Play a video from the card; turn any video into the card's file. |
| `tests/` | The Mac tests: `run.sh` (panel packing and drawing), `run_sim.sh` (the whole player against a simulated card), `test_converter.py`. |

FatFs and Petit FatFs are in `third-party/fatfs/` and `third-party/pff/`, unmodified. Only the configuration files (`ffconf.h`, `pffconf.h`) are in this folder.

## Try it in ten minutes (a Mac, no board)

```bash
portability/boosterpacks/sharp128/tests/run.sh                 # 1 s: panel bytes, VCOM, drawing, bands
python3 portability/boosterpacks/sharp128/tests/test_converter.py   # needs ffmpeg
portability/boosterpacks/sharp128/tests/run_sim.sh             # 15 s: the player against a simulated SD card
```

The simulation writes a test video with moving shapes, puts it on a FAT image, runs the real player code against a mock SPI bus and a simulated card, and checks that every frame sent to the panel equals the frame in the file (three paths: Petit FatFs with 8-row reads, Petit FatFs with 1-row reads, FatFs with whole frames).

## Build for a board

```bash
portability/boosterpacks/sharp128/boards/build_mspm0.sh mspm0g3519 hello     # also mspm0g3507, mspm0l1306, mspm0c1104
portability/boosterpacks/sharp128/boards/build_mspm0.sh mspm0c1104 player -DSHARP_CHUNK_ROWS=8
portability/boosterpacks/sharp128/boards/build_mspm33.sh hello               # LP-MSPM33C321A
```

Output is `.out` and `.map` files in the folder named at the top of each build script (set `OUT=` to change). The scripts use the SysConfig that is installed (1.28.x in these builds).

## Sizes (built 2026-10-20, TI Arm Clang 5.1.1.LTS, MSPM0 SDK 2.11.00.07, MSPM33 SDK 1.04.00.00)

Flash is code plus constants; RAM is everything in RAM including the stack. Read from each `.out` with `tiarmsize`.

| Kit | Example | Flash | RAM | Notes |
|---|---|---|---|---|
| LP-MSPM0G3519 | hello (with FatFs) | 12.5 kB | 5.5 kB | 2 kB frame buffer, 2 kB stack |
| LP-MSPM0G3519 | player (FatFs, whole frames) | 10.2 kB | 5.7 kB | |
| LP-MSPM0G3507 | hello / player | 12.5 kB / 10.1 kB | 5.5 kB / 5.7 kB | |
| LP-MSPM0L1306 | hello (screen only) | 4.4 kB | 3.1 kB | 2 kB frame buffer, 1 kB stack, in 4 kB |
| LP-MSPM0C1104 | hello (1-row bands, no frame buffer) | 4.9 kB | 609 B | 512 B of that is stack; the code used 352 B |
| LP-MSPM0C1104 | player (Petit FatFs, 8-row reads) | 7.2 kB | 779 B | 128 B read buffer, 512 B stack |
| LP-MSPM33C321A | hello / player | 29.4 kB / 17.7 kB | 3.7 kB / 3.9 kB | TI's example makefile, debug-sized flags |

## Pins

BoosterPack header pin, from SLAU786 Tables 1 to 3 and the LaunchPad board files in the MSPM0 SDK.

| Pin | Job | G3519 | G3507 | L1306 | C1104 | MSPM33C321A |
|---|---|---|---|---|---|---|
| 2 | Panel power | PA14 | PA25 | PA25 | PA25 | PA18 |
| 5 | DISP (low blanks, picture kept) | PB29 | PA26 | PA24 | PA24 | PA21 |
| 6 | Panel select (high) | PA25 | PB24 | PA21 | PA28 | PA22 |
| 7 | SPI clock | PB31 | PB9 | PA6 | PA6 | PB31 |
| 15 | SPI data out | PB8 | PB8 | PA5 | PA18 | PB8 |
| 14 | SPI data in (card) | PB7 | PB7 | PA4 | PA4 | PB14 |
| 8 | Card select (low) | PB30 | PA27 | not used | PA22 | PB30 |
| 17 | Card detect (low = card in) | PA30 | PB15 | not used | not used | PC23 |

Jumpers: LP-MSPM0C1104 J7 off (LED1 shares PA22 with the card select) when the card is used; LP-MSPM33C321A J3 off (the boot-select button shares PA18, the panel power). Pin 19 is not used: on the BoosterPack the panel takes its VCOM flip from a bit in the data, and pin 19 is not connected as shipped.

## What the panel code does

- **Byte order.** The panel wants every byte least significant bit first. The port gives plain MSB-first SPI and `sharp_lcd.c` reverses each byte, so one SPI setup serves the panel and the card. A row is 16 bytes with bit 0 of byte 0 the leftmost pixel, which is also the order of the XBM picture format.
- **Levels.** Bit 1 is white, bit 0 is black. A cleared buffer is black; `sg_fill(s, SG_WHITE)` makes a blank page.
- **Frame on the wire.** 1 mode byte, then 128 rows of (address, 16 data bytes, 1 zero byte), then 1 zero byte: 2.3 kB, 18.4 ms at the panel's 1 MHz.
- **VCOM.** Every command carries the VCOM bit and flips it. `sharp_vcom_tick(ms)` sends a 2-byte VCOM-only command once a second so a still picture keeps flipping.
- **Bands.** `sg_render_bands` runs one drawing function once per band of 1 to 128 rows and sends each band. With a 2 kB buffer a frame is one band. With a 16-byte buffer the scene is drawn 128 times, which is how the LP-MSPM0C1104 shows a full picture in 1 kB of RAM.

## Results

Ran on the LP-MSPM0C1104 with the BOOSTXL-SHARP128 stacked, 2026-10-20 (flash and read by `boards/c1104_run.py`; the debugger read the result struct). The card was not tested: no microSD card was available.

| Item | State |
|---|---|
| Panel packing, VCOM, drawing, bands | Checked on the Mac: `tests/run.sh` prints PASS |
| Whole player path against a simulated card | Checked on the Mac: 30 of 30 frames equal on three paths |
| Converter pixel order | Checked: `tests/test_converter.py` prints PASS |
| Builds for five kits | Built |
| Hello on the LP-MSPM0C1104, no frame buffer | Ran: counter at 23 after 6 s, stack 352 of 512 B used, 609 B of RAM in all; a camera frame shows the panel lit with the shapes, too small to read the text |
| Panel on the other four kits | Not run: the BoosterPack was on the LP-MSPM0C1104 only |
| Card, Petit FatFs and the video player | Not run: no microSD card was available (the player stops with error -2, card not ready) |

Measured on the LP-MSPM0C1104 (hello, SPI at 1.000 MHz read back from the chip, one run each):

| Band height | RAM | Whole picture, drawn and sent | Bytes on the wire |
|---|---|---|---|
| 1 row | 609 B | 153.7 ms | 2,560 |
| 2 rows | 625 B | 92.7 ms | 2,432 |
| 4 rows | 657 B | 63.3 ms | 2,368 |
| 8 rows | 721 B | 49.3 ms | 2,336 |
| 16 rows | 849 B | 41.1 ms | 2,320 |

One 1-row update takes 260 us. The bus moves a thousand bytes in 9.5 ms against the 8 ms the clock allows (84 percent); the rest is the CPU between bytes at 24 MHz. The time falls with taller bands because the scene is drawn once per band; the wire time is 21 to 24 ms of each figure.

Expected, not measured: a full frame from a 2 kB buffer takes 18.4 ms of wire time at 1 MHz (2.3 kB), about 19 to 22 ms with the code between rows. The simulated player paces to 30 frames a second with 8-row reads; the real card, clock and CPU time are not in that figure.

## Traps found while writing it

- The LP-MSPM0C1104's SysTick does nothing, so the port counts milliseconds with TIMG14 there (`mspm0c1104/FAQ.md`).
- A disk driver that returned an error after a good one-sector read passed every other check and failed at mount; the simulation caught it (fixed in `sharp_sd.c`).
- The LP-MSPM0C1104 needs 352 bytes of stack for the band renderer; the 64-byte default of the generated linker file is far short, and 256 is short too. The build script sets 512.
- Attaching OpenOCD to the LP-MSPM0C1104 resets it, so a result read in a second connection shows the program a few milliseconds after reset. Flash, run and read in one connection (`boards/c1104_run.py` does).
- FatFs mounts a bare FAT volume with no partition table (the simulation images) as well as an MBR card; both libraries were checked on FAT16 and FAT32.
- The card and the panel share clock and data. The card select must be high and the panel select low whenever the other one is talked to; `sharp_sd.c` forces the panel select low before every card command.

Files for the agents that build on this: [AGENTS.md](AGENTS.md). Experiment: [Sharp screen in 1 kB of RAM](../../../docs/experiments/2026-10-20-mspm0c1104-sharp128-1kb.md).
