<!-- experiment
id: E235
board: LP-MSPM33C321A
date: 2026-10-20
status: works
reached: run
line: The plain Doom engine links for the LP-MSPM33C321A but needs 164 kB of static data plus an 800 kB zone. The rp2040-doom design (m33-whx) finishes all three shareware demos on the chip's real 256 kB with a software page cache over the QSPI flash: 19.9 frames a second at an 8 kB cache and 21.9 at 16 kB, display and sound stubbed, 98.3 to 99.1 percent cache hits. On the LP-EM-CC35X1 the same design runs them at 39.2 frames a second. The LP-MSPM0G3519 (128 kB) and LP-MSPM0G3507 (32 kB) stop at static data alone.
stop: flash
-->
# What is the smallest TI chip that runs the real Doom engine? The LP-MSPM33C321A, with a smaller engine design

**Short answer.** The smallest TI chip tested that runs the real Doom engine's three shareware demos is the LP-MSPM33C321A (256 kB of SRAM), and only with the rp2040-doom design. The plain engine does not fit: it needs 164 kB of static data plus an 800 kB zone (the engine's memory pool). The rp2040-doom design needs 228 kB of the 256 kB. It reads a 1.7 MB data file that the chip's QSPI flash cannot map into the address space, so a software page cache serves the reads. On the chip, all three demos finish at 19.9 frames a second with an 8 kB cache and 21.9 with 16 kB, display and sound stubbed (section 14). Section 15 draws it on the BoosterPack's screen. The LP-EM-CC35X1 (sections 9, 11, 13 and 16) runs the same design at 39.2 frames a second. The LP-MSPM0G3519 (128 kB) and LP-MSPM0G3507 (32 kB) stop at static data alone.

Contents: 1 licensing; 2 to 8 the plain engine and its budget; 9 the same engine on the LP-EM-CC35X1; 10 to 14 the rp2040-doom design in an emulator, on the LP-EM-CC35X1 and on the LP-MSPM33C321A with a page cache; 15 the BoosterPack screen; 16 the -O2 fix.

Terms: [vocab](../vocab.md). 1 kB = 1,024 bytes; 1 MB = 1,048,576 bytes. **Zone** is the engine's own memory pool. **WAD** is the game data file and a **lump** is one item in it. **QSPI** is the serial flash interface. **Emulator** here is the Unicorn model of a Cortex-M33 running the same ARM image the board would run; it counts instructions and does not model cycles, flash wait states or the QSPI.

**Question.** Can real Doom (the shareware episode, unmodified data) run on a TI LaunchPad, and which is the smallest chip where it does?

**Expectation, written before any build.** An early estimate said the LP-MSPM33C321A would fit, "memory as the nRF52840 port", with the zone in about 150 kB (rp2040-doom reached about 58 kB), and that the LP-MSPM0G3519 would not (128 kB, no external memory bus). No finer number was recorded before the builds. The first half missed by a wide margin: see the line.

**Vendor position.** TI ships no Doom and no example that runs a large game engine. The LP-MSPM33C321A has 1 MB of flash, 256 kB of SRAM and a QSPI flash on the board that the board guide lists as 8 MB (`mspm33c321a/AGENTS.md`; the chip's own ID reads 16 MB, see [the QSPI dump page](2026-10-21-mspm33c321a-qspi-dump-doom-data.md)). Its QSPI controller, read in SDK 1.04.00.00 on 2026-10-20 (`hw_qspi.h`, `dl_qspi.h`, `mspm33c321x.h`, the two `qspi_flash_readback` example READMEs, the driverlib API guide), is a command and FIFO controller with DMA triggers: a read is a command, an address, dummy clocks and then data through the RX FIFO. A search of those files for "xip", "execute in place" and "memory-mapped" found nothing. The G3519 is 512 kB flash and 128 kB RAM, the G3507 128 kB flash and 32 kB RAM (`data/boards.csv`).

References read for technique, no code copied: rp2040-doom notes (https://kilograham.github.io/rp2040-doom/speed_and_ram.html: a straight Chocolate Doom build "requires 300K of static mutable data and a minimum of about another 700K of Doom zone memory"; their zone fell to about 45 kB with thousands of changes) and an nRF52840 port, 256 kB RAM, 1 MB flash and a 16 MB QSPI flash (https://next-hack.com/index.php/2023/12/10/multiplayer-doom-on-the-sparkfun-thing-plus-matter-board).

## 1. Licensing (checked, not a legal opinion)

Full text and sources: [ports/doom/LICENSING.md](../../ports/doom/LICENSING.md). The engine is GPL-2.0 and lives only in `ports/doom/`. The shareware WAD ships under a license that lets anyone copy it but forbids changing it, charging for it or making derivative works, so it is never committed and the build downloads it, checks its SHA-1 and uses it as it is. Freedoom is BSD-style but its phase 1 data is 27.5 MB, 3.4 times the 8 MB QSPI flash.

## 2. What was tried

Engine: doomgeneric at commit `e257a09` (a portable fork of Chocolate Doom), fetched and patched by `ports/doom/fetch.sh`; the patch is `ports/doom/patches/0001-fox-red-labs-squeezes-and-target.patch`. WAD: DOOM1.WAD 1.9, 4.0 MB, 1,264 lumps, SHA-1 `5b2e249b9c5133ec987b3ea77596381dc0d6bc1d`, not committed. Mac: clang, headless platform layer. M33: TI Arm Clang 5.1.1.LTS at `-Os`, the SysConfig output of the 160 MHz example, MSPM33 SDK 1.04.00.00, the same toolchain as `mspm33c321a/tools/build_h264.sh`. Emulator: Unicorn 2.1.4, Cortex-M33 model.

The squeeze sets (flags in `ports/doom/srclist.sh`):

| Set | What it adds |
|---|---|
| S0 | Platform change only: the engine hands its 320 x 200 8-bit frame and palette to the platform (no output buffer), no screen melt, no files, zone as a static array |
| S1 | S0 plus the state and object tables `const` (they stay in flash) |
| S2 | S1 plus render limits cut to the demos' measured maxima with margin: visplanes 128 to 64, openings 20,480 to 5,120, drawsegs 256 to 128, vissprites 128 to 100 |
| S3 | S2 plus the network tic buffer cut from 128 to 16 entries |

## 3. How far it got

Link on the M33, run in the emulator, not flashed.

**Static memory by set, measured from the linked images** (kB; the zone is not included; the stack is the 8 kB of the example and the heap is 4 to 6 kB):

| Set | `.data` | `.bss` | Stack | Static RAM | Flash (text, rodata, init image) |
|---|---|---|---|---|---|
| Stock doomgeneric | not built for the chip: its output buffer alone is 1,000 kB, and S0's static data already passes 256 kB | | | | |
| S0 | 58.9 | 228.7 | 8.0 | 295.6 | 251.0 |
| S1 | 20.2 | 228.7 | 8.0 | 256.9 | 281.5 |
| S2 | 20.2 | 149.6 | 8.0 | 177.8 | 281.5 |
| S3 | 20.2 | 131.6 | 8.0 | 159.8 (163.8 with a 4 kB heap) | 281.5 |

Flash is no problem: 281.5 kB of the chip's 1,024 kB (140.6 kB code, 135.3 kB read-only data, 5.7 kB init image). The state and object tables moved from RAM to flash, which is why flash rose 30 kB while `.data` fell 39 kB.

**The image that links.** S3 with an 80 kB zone and 4 kB heap links: flash 281.6 kB, SRAM 243.8 kB used, 12.2 kB free (`ports/doom/target/build.sh Os 80`). It cannot play: see the zone numbers next. A bigger zone does not link.

**Zone memory, measured by running the real ARM image in the emulator** (S3; every other number is exact for a 32-bit build, 24-byte block header):

| Demo | Game tics | Frames drawn | Zone floor (blocks that cannot be freed, peak) |
|---|---|---|---|
| demo1 | 5,026 | 5,025 | 532.2 kB |
| demo2 | 3,836 | 3,835 | 593.0 kB |
| demo3 | 2,134 | 2,133 | 558.3 kB |

The floor is the least the zone can hold. The zone also holds cached lumps and gets fragmented, so the smallest zone that finishes is bigger. Runs with the zone set to a fixed size:

| Zone | demo1 | demo2 | demo3 |
|---|---|---|---|
| 960 kB | finishes | finishes | not run |
| 832 kB | finishes | finishes | finishes |
| 800 kB | finishes | finishes | finishes |
| 768 kB | stops at tic 3,502: no free block of 32.0 kB | finishes | finishes |
| 736 kB | not run | stops at tic 470: no free block of 32.0 kB | not run |
| 704 kB | not run | stops at frame 79: no free block of 17.2 kB | not run |
| 640 kB and 576 kB | not run | stop during start-up | not run |

So the least zone that finished all three demos is 800 kB, and 768 kB is not enough for demo1. That is the number on the line. The render-limit maxima in the same runs were 41 visplanes, 2,106 openings, 67 drawsegs and 67 vissprites, so the S2 and S3 limits have margin on these three demos; they are not proven for every map.

**Where the 593.0 kB goes (demo2, at the peak):** held lumps 112.4 kB (the status bar graphics, the texture list, a music lump that is loaded even with the sound off, the demo itself); level data 193.3 kB (linedefs 64.1, segs 45.2, sidedefs 25.9, nodes 23.4, sectors 15.2, blockmap 12.1, vertexes 7.4); things in the level 54.0 kB (307 objects); texture tables 63.1 kB; the 320 x 200 screen 62.5 kB; the lump directory 34.6 kB; one texture being composed 32.0 kB; other 21.4 kB.

**Correctness of the squeezes and the emulator.** The first 400 frames of demo1 have the same checksum on the Mac build before the squeezes, on the Mac build with all squeezes, and in the emulator running the M33 image (`8ceeaad8`, a CRC-32 over each frame's 62.5 kB of index bytes and 768 palette bytes). After about 800 frames the Mac build differs between runs of the same build with the same demo, with or without a fixed clock, so longer checksums are not a valid comparison. The cause was not isolated; the renderer reading past a short texture column into neighboring memory is the suspect. The emulator runs themselves are repeatable: the frame and tic counts equal the Mac's in every run.

**Speed, CPU only.** Every instruction of the M33 image was counted in three 20-frame windows of demo1 (one frame is one game tic in this mode): 1.05 million instructions a frame at frames 100 to 119, 1.22 million at 1000 to 1019 and 1.15 million at 3000 to 3019, mean 1.14 million. At 160 MHz that is 7.1 ms a frame at one cycle per instruction and 10.7 ms at 1.5 cycles, which is 94 to 140 frames a second against the 35 the game needs (28.6 ms a frame). The CPU has 2.7 to 4 times to spare. Not included: flash wait states, QSPI reads for lumps, sending the frame to a panel, and the emulator's model being a model. The clock the engine reports in the emulator is fake and is not used.

**What changes at 128 x 128.** The engine always renders 320 x 200 (62.5 kB), so a smaller panel saves no engine RAM. The stock platform layer also fails below 320 x 200: at 128 x 128 it computes a scaling factor of 0 and draws nothing (0 of 16,384 pixels non-black over 1,000 frames; at 320 x 200 and 640 x 400 every pixel is drawn). Its output buffer is the panel size times 4 bytes: 1,000 kB at 640 x 400, 250 kB at 320 x 200, 64 kB at 128 x 128. In the direct mode used here the buffer does not exist; the platform scales while it sends. A 128 x 128 RGB565 frame is 32 kB if a panel driver buffers a whole frame, or a few kB if it sends in bands as the Sharp panel does. The Mac demo writes both sizes from the same frame (`ports/doom/host/dg_headless.c`).

## 4. QSPI: how the WAD would be read

There is no memory-mapped (execute or read in place) mode in SDK 1.04.00.00 for this chip's QSPI, so the WAD is read lump by lump: the engine's file layer calls `fox_wad_read(offset, buffer, length)` (`ports/doom/target/w_file_mem.c`), which becomes a QSPI fast-read command with the address, dummy clocks and a DMA transfer into the buffer. The cost of that choice, measured on the Mac with a mapped file as a stand-in: with lumps used in place the zone's non-purgeable peak is 107 kB lower and the cache holds 0.6 MB instead of 2.4 MB (64-bit sizes). Copying also means every cached lump takes zone memory, which is part of the 800 kB. A part with a mapped window (the nRF52840 reference has one) avoids this. Read speed was measured later on the chip: a miss costs 23.8 microseconds (section 14).

## 5. Budget for the three chips

All in kB, S3 static data, zone floor from demo2 (593.0), zone that finishes 800. M0+ figures for the two MSPM0 chips come from compiling the same sources for the Cortex-M0+ (`-mcpu=cortex-m0plus`, `-Os`, object files not linked: 262.0 kB of code and read-only data, `.data` 20.2 kB, `.bss` and common 135.9 kB); the zone is a property of the engine, not the core, so the same measured figures are used.

| | LP-MSPM33C321A | LP-MSPM0G3519 | LP-MSPM0G3507 |
|---|---|---|---|
| Core, clock | Cortex-M33, 160 MHz | Cortex-M0+, 80 MHz | Cortex-M0+, 80 MHz |
| SRAM | 256 | 128 | 32 |
| Static RAM needed (S3, with stack) | 163.8 | 164 | 164 |
| Left for the zone | 92.2 (link) | none: 36 over before any zone | none: 132 over before any zone |
| Zone needed: floor / finishes | 593.0 / 800 | 593.0 / 800 | 593.0 / 800 |
| Total RAM needed | 757 to 964 | 757 to 964 | 757 to 964 |
| Short by | 501 to 708 | 629 to 836 | 725 to 932 |
| Flash | 1,024 (281.6 needed, fits) | 512 (about 262 needed, fits) | 128 (about 262 needed, 134 over) |
| Where the WAD lives | QSPI on the board (16 MB read from the chip; the guide says 8 MB) | no external flash on the LaunchPad | none |
| Verdict | stops at zone memory | stops at static RAM | stops at static RAM and flash |

## 6. The line

Zone memory on the LP-MSPM33C321A: at least 800 kB to finish all three shareware demos (593.0 kB floor) plus 163.8 kB of static data, against 256 kB of SRAM. The cheap squeezes bought 132 kB of static RAM (295.6 kB to 163.8 kB) and the screen melt and output buffer. They do not touch the zone, which is level data, objects, texture tables and held lumps copied from the WAD.

The vendor shipped nothing for this, so this is a first. What existed: the SDK's QSPI examples (read 256 bytes) and the build and link recipe of the H.264 port. How it was done: the engine's platform layer re-cut to a direct frame, files removed, the zone made a static array, limits trimmed to measured maxima, and the real ARM image run in an emulator so the 32-bit numbers are exact without a board. What went wrong:

| What | Cause tag | Cost | Fix |
|---|---|---|---|
| Image would not link: `.data` 58.9 kB plus `.bss` 228.7 kB | static tables and render arrays | the first link | const tables, trimmed limits |
| "Couldn't realloc lumpinfo" at start-up | 2 kB default heap; the engine callocs its 34.6 kB lump directory | one emulator run | directory moved into the zone |
| Output stays blank at 128 x 128 | stock layer's scaling factor is 0 below 320 x 200 | found by a test, not on a panel | direct frame mode, platform scales |
| Frame checksums differ between Mac runs after about 800 frames | not isolated | a check dropped | compare the first 400 frames only |
| Zone sweep: a 32 kB composed texture fails to fit at 736 and 768 kB | fragmentation by non-purgeable blocks | five emulator runs | none; sets the 800 kB figure |

Effort: no board time and no hands. Emulator runs took 3 to 8 minutes each on a busy Mac. Verdict: not good for a chip with 256 kB; the engine's structure, not its code size or its speed, is what does not fit. The result is the number, and it says what a real attempt has to do: stop copying level data and textures into RAM.

## 8. Ask

None. A reader who has run this engine on a 256 kB part is the person to hear from: which squeeze came first.

## 9. LP-EM-CC35X1 (CC3551E): flashed and run

Added 2026-10-20. Question: does the same engine fit a Cortex-M33 with memory-mapped flash and 668 kB of data RAM? Code: `ports/doom/target-cc35x1/` ([README](../../ports/doom/target-cc35x1/README.md)).

**Expectation, written before the first flash:** the section 5 floors minus the lumps read in place (about 105 kB) are about 427 kB (demo1), 488 kB (demo2) and 453 kB (demo3) against a zone of 477 kB; demo1 and demo3 should run and demo2 should stop with a zone failure. The first 400 frame checksums should equal the Mac build (`8ceeaad8`, `6ea9fea2`, `5c59f266`). Speed: 20 to 60 frames/s at 160 MHz.

**What the chip gives an application** (SDK 10.20.00.39 linker file and `hw_memmap.h`): DRAM 507.8 kB, data TCM 128 kB, code RAM 32 kB (667.8 kB of data RAM in three separate blocks, the largest 507.8 kB); the 8 MB flash is read in place through `0x14000000`. The data sheet's 1.1 MB total includes memory the radio cores use.

**What was built.** Squeeze set S3, `-Os`, OTA switched off (application slot 6,248 kB, room for the WAD), the unchanged WAD included in the signed image and used in place, zone 477 kB placed first in DRAM, other data in TCM, code RAM and the rest of DRAM. Flash 4,399.0 kB (4,097.7 kB of it WAD), RAM outside the zone 194.7 kB including the 22 kB FreeRTOS heap. The 4.4 MB image signs with the Toolbox image builder.

**Result (2026-10-20).** The WAD hash printed at boot equals the file's. Demo 1 draws the same first 400 frames as the Mac (`8ceeaad8`) at 50 to 66 frames/s, inside the expected 20 to 60 range at the top, and stops at frame 2030 of 5,025 with `Z_Malloc: failed on allocation of 32792 bytes`. Demo 2 stopped at frame 52 and demo 3 at frame 425, both on zone failures (zone 480 kB after trimming the FreeRTOS heap from 22 to 19 kB). The expectation said demo 1 and 3 would run and demo 2 would stop; demo 1 and 3 also stopped, so the in-place lump saving (105 kB) was smaller than estimated or fragmentation costs more. Demo 3 matched the Mac checksum at frame 100 and differed from frame 200 (cause not found). The demo-select byte never reached the firmware, so each demo is its own image. The static data is already squeezed to 191.6 kB, so reaching the 593 kB floor needs level data moved out of RAM, not further trimming. Details: [target README](../../ports/doom/target-cc35x1/README.md).

**Screen.** The Waveshare panel needs an SPI controller and five header pins, bands sent from `DG_DrawFrame8`; not started.

## 10. Second build: the rp2040-doom design on the same chip (target m33-whx)

This build ports rp2040-doom (Graham Sanderson, https://github.com/kilograham/rp2040-doom, pinned commit `29a453c`), whose notes say a straight Chocolate Doom build "requires 300K of static mutable data and a minimum of about another 700K of Doom zone memory" and that theirs fits a 264 kB RP2040. Folder: [ports/doom/m33-whx](../../ports/doom/m33-whx/README.md); which of its changes are generic and which belong to the RP2040 is the table in that README.

**What was done.** The converter `whd_gen` was built on the Mac and run on the shareware WAD: 4.0 MB in, 1,758 kB out (10.7 percent of the 16 MB QSPI the chip reports; the board guide says 8 MB), the same bytes on two runs. The engine and renderer (78 upstream files, C and C++) were built with TI Arm Clang 5.1.1.LTS and linked into the chip's real 256 kB of RAM with 16-bit pointers and a zone as a static array; the SDK headers the engine includes are stand-ins, the display, sound, keys and network are stubs, and the video side takes each frame as the renderer finishes it. Three small patches to upstream (window base, no USB-controller RAM, an optional C path) and about 200 lines of platform layer. Licensing: only the converter is shipped; see [LICENSING.md](../../ports/doom/LICENSING.md) section 4.

**Result, measured in the Cortex-M33 emulator on the real memory map.**

| | Plain engine, set S3 (sections 1 to 8) | rp2040-doom design on the M33 |
|---|---|---|
| Flash used | 281.6 kB | 230.8 kB, plus a 1,758 kB data file that must be readable by address |
| Static RAM | 163.8 kB | 198.0 kB (105.0 of it two frame buffers, 46.2 a render work list) |
| Zone in use, three demos | 593.0 kB floor | 28.9 kB |
| Smallest zone that finished the three demos | 800 kB | 30 kB (29 kB and 28 kB ran out of memory in demo 2) |
| RAM to run | 964 kB | 228 kB of 256 kB, 28 kB spare |
| Instructions per frame | 1.14 million (no overlays, no decoding) | 2.55 million (decodes graphics from the data file as it draws) |
| Frame time at 160 MHz, 1.0 to 1.5 cycles per instruction | 7.1 to 10.7 ms | 15.9 to 23.9 ms (a tic is 28.6 ms) |

All three demos reach their end: the engine's game-tic counter reads 5,197, 9,235 and 11,571 at the three ends (the demos are 5,026, 3,836 and 2,134 tics; the rest is the attract-mode pages), and the run is the same at every zone size from 30 to 40 kB. The display side, the sound mixers and the software synthesizer are not built; an estimate from upstream's notes puts them at 10 to 15 kB, inside the spare 28 kB. The three demos are three of nine levels.

**Zone saved per demo** (floor in the plain engine, then high water here): demo1 532.2 kB to 26.3 kB, demo2 593.0 kB to 28.9 kB, demo3 558.3 kB to at most 28.9 kB. Measured only as a whole; the squeezes overlap, so no per-squeeze split was run. What carries to another chip, such as the LP-EM-CC35X1 of section 9: the converter, the three-file patch, the stand-in headers and the build flags; only the platform file and the linker placement are chip-specific.

**The line moved.** From RAM to data access. The engine reads the converted file through plain pointers: level data, texture and flat data, and every graphic are decoded from it as the frame is drawn. On the RP2040 the flash sits in the address space. On this chip the QSPI controller has no such window (section 4), and the file is 1.7 times the 1 MB of internal flash, which also holds 0.23 MB of code. In the emulator the file is simply mapped at an address, which is what the chip cannot do. The three ways out, none built: a software cache between the engine and the QSPI (reads in 4 kB pages; costs RAM from the 28 kB spare and time per miss), loading each level's data into RAM (34 kB on average over the nine levels, too much for the spare), or a smaller data set (Freedoom's BSD license allows cutting and converting it; its phase 1 file is 27.5 MB, so only a cut-down set would fit even the 8 MB QSPI after conversion, and 1 MB of flash only with a tiny one).

**Verdict for the question.** The smallest TI chip tested that can run real Doom is the LP-MSPM33C321A, in RAM terms and only with this design; the LP-MSPM0G3519 and LP-MSPM0G3507 still stop at static data, because the two frame buffers alone (105.0 kB) exceed the G3507 and leave the G3519 with 23 kB. At this point nothing had run on a board; sections 9 and 11 to 16 cover the board runs. The LP-EM-CC35X1 (section 9) has the piece this chip lacks: its 8 MB flash is readable through the address space, and it has 668 kB of data RAM, so the same design would have everything it needs there; not built here.

**What was tried.** Squeeze patches to the engine (direct frame output, static zone, const tables, trimmed limits), a Cortex-M33 emulator run of the real image, a build and a board run on the LP-EM-CC35X1, and a second build with the rp2040-doom data format (sections 2, 9 and 10).

**How far it got.** The emulator plays the three demos to the end. On the LP-EM-CC35X1 demo1 ran on the board: the data file hashes right, the first 400 frames equal the Mac, 49 frames per second, then a zone failure at frame 2030 (section 9).

**The line.** RAM for the zone: at least 800 kB on the LP-MSPM33C321A against 256 kB; the LP-EM-CC35X1 holds a 477 kB zone and stops just short (sections 6 and 9).

## 11. The rp2040-doom design on the LP-EM-CC35X1: all three demos finish

Added 2026-10-21. Question from sections 9 and 10: with the data file readable by address and RAM to spare, does the compressed-data engine finish the demos that the stock engine could not? Code: `ports/doom/target-cc35x1-whx/` ([README](../../ports/doom/target-cc35x1-whx/README.md)); it reuses the sources, flags, stand-in headers and patch of `m33-whx` and adds a platform layer for this chip.

**Expectation.** All three demos finish with a zone of 40 kB (the emulator needed 30 kB); frame checksums equal the emulator's; 42 to 63 frames/s if the CPU were the only cost (2.55 million instructions a frame at 160 MHz, 1.0 to 1.5 cycles each).

**What was built.** The converted file (1,758 kB) is a section of the signed image, read in place at `0x14003000`; OTA off. The 16-bit pointer window starts at the start of DRAM; the zone (40 kB) and all `.bss` are in DRAM, so the statics the engine points at (`thinkercap`, `players`) are inside it. The game clock is virtual (35 ms per read, as in the emulator run) so the frames do not depend on speed. One image plays the three demos in attract-mode order; no demo select is needed. Flash 2,017 kB, RAM 260 kB of which 40 kB zone.

**Result (2026-10-21, two images, one run each).** All three demos play to their end. Checksums at frames 100, 200, 300 and 400 (`f1b5e5d6`, `27572f0c`, `d0a2dc52`, `9ecd4100`), the demo-end frames (1,736, 3,092, 3,881) and the checksum over all 3,881 frames (`e9db8f1d`) equal the emulator run of the same engine. Zone high water 28.9 kB, the same as the emulator. Frame rate after frame 400: 22.5 frames/s (21.7, 23.7 and 23.1 per demo); with the checksum on every frame 20.3.

| | Stock engine, section 9 | rp2040-doom design |
|---|---|---|
| Zone | 480 kB, ran out | 40 kB, 28.9 kB used |
| Demos finished | none (frames 2,030, 52, 425) | three of three |
| Frames/s | 42 to 66 | 22.5 |
| Flash | 4,397 kB | 2,017 kB |
| RAM | 672 kB | 260 kB |

The frame rate is about half the expectation. The emulator counts instructions and charges nothing for reading the data file; on the board every texture, flat and sprite decode reads the external flash. Cause not isolated (no counter for cache misses was read). The reference for the checksums is the emulator because the WHX design draws different frames from stock doomgeneric and has no Mac build here. Not covered: panel, keys, sound, the other six levels.

## 12. The rp2040-doom design on the LP-MSPM33C321A with a software page cache over the QSPI

Added 2026-10-21. Question from sections 4 and 10: the engine reads its 1.7 MB data file through pointers and this chip's QSPI has no address window. Can a page cache in the 28 kB of spare RAM serve those reads, and what does it cost? Code: `ports/doom/m33-whx/` ([README section 6](../../ports/doom/m33-whx/README.md)), the board plan on the [QSPI dump page](2026-10-21-mspm33c321a-qspi-dump-doom-data.md).

**Expectation.** From a read trace of the first 740 frames (56.7 million reads, 77 thousand a frame), a cache of 256-byte pages, 2 ways, least recently used, would miss 338 times a frame at 16 kB and 554 at 8 kB: 5 to 9 ms of QSPI time at 40 MHz quad, more than 99 percent of reads served from the cache.

**What was built.** Every engine pointer into the file keeps its value; the dereferences became cache calls (`FW_U8`, `FW_U16`, `FW_U32`, `FW_COPY`, `FW_VIEW`) in 22 upstream files, about 200 changed lines (`ports/doom/m33-whx/patches/0002-whx-page-cache.patch`). One edit in `tiny_huff.h` covers every compressed stream (patches, sprites, flats, composite textures, the demo); the level data (lines, sides, segs, nodes, vertexes, blockmap, reject, things) is read through the same calls; the 8.5 kB of light tables is copied to RAM. In the emulator the data file is not mapped, so any missed site stops the run and names the function. The QSPI is modeled as a device: 40 MHz clock and quad I/O read with 6 dummy clocks (the SDK's DMA example), 400 CPU cycles of driver software a read (an assumption), 2 clocks a byte.

**Result (2026-10-21).** All three demos finish with a 30 kB zone, the same gametic at each demo end (5,197, 9,235 and 11,571) and the same zone figures as the plain build. The frame checksum over all 3,881 frames is `b22b3fde`, the plain build's is `e9db8f1d`: 11 frames differ, by 20 pixels in all. The cause is a stack array in `draw_composite_columns` that is drawn before it is fully written; zeroing it gives `fc7b6c2d` for both builds, frame for frame.

| Cache (256-byte pages) | RAM free of 256 kB | Hits | Misses a frame | QSPI wait a frame, mean (worst) |
|---|---|---|---|---|
| 8 kB | 11.4 kB | 98.30 % | 549 | 8.6 ms (18.2 ms) |
| 12 kB | 7.4 kB | 98.74 % | 407 | 6.4 ms (15.6 ms) |
| 16 kB | 3.3 kB | 98.89 % | 360 | 5.6 ms (13.3 ms) |
| 19 kB | 0.3 kB | 99.06 % | 303 | 4.8 ms (10.3 ms) |

The 16 kB build's misses per frame in demo 1 (329) match the trace's prediction (338). Smaller pages are faster in the model (128 bytes: 4.1 ms a frame at 16 kB; 64 bytes: 3.7 ms) and stay ahead if the driver costs 1k or 2k cycles a read. The plain one-bit read command, which needs no change to the flash, is 3.5 times slower: 19.5 ms a frame.

Instructions a frame rise from 2.50 million to 3.60 million (mean of 7 windows of 20 frames). At 160 MHz and 1.0, 1.5 or 2.0 cycles an instruction the cached build takes 28.2, 39.4 or 50.7 ms a frame including the QSPI wait, against 15.6, 23.4 or 31.2 ms for the plain build with no data reads; one game tic is 28.6 ms. Cycles an instruction on this chip are not measured.

**What it shows.** The data problem has a software answer that finishes the demos in the emulator with no pointer into the file left on the paths they use. The limits are RAM and speed. The cache and the light tables take 25 kB of the 28 kB spare, so the display and sound (estimated 10 to 15 kB) fit only beside an 8 kB cache, which costs 8.6 ms of QSPI wait a frame; and the wrappers cost 34 instructions for each of 32 thousand accesses a frame, which puts the frame rate under real time unless a cycle costs about one clock.

**Not covered.** The six levels the demos do not play, save and load, deathmatch respawn, the sound paths, the display, the keys. Nothing ran on the board: the QSPI holds the out-of-box video, so the board work was a separate plan (dump twice and compare, find a blank run of 28 blocks of 64 kB, write the file there, run the cache against the real read path); section 14 has the result.

## 13. Where the time goes on the LP-EM-CC35X1, and what moved it from 22.5 to 36.7 frames/s

**Question.** Section 11 measured 22.5 frames/s against an expected 42 to 63 and guessed the data file read through the external flash. Where do the cycles go, and which placement of code and data recovers them? **Expectation (written before the runs).** The WHX reads in place are the cause, so copying hot lumps to RAM should give most of the missing factor of two.

**Method.** Cycle counts from SysTick plus the RTOS tick (the cycle counter of the core reads 0 on this chip), per step of a frame, with the profiling build (`PROF=1`). The flash cache counters (instruction cache hits and misses) read at each demo end. Micro-tests of load and store cost per memory, by unrolled loops. Each layout change re-run on the same three demos; the frame checksums at frames 100 to 400 and the full-run checksum compared with the emulator reference. The board was run through the probe lock; at the end it was erased and the ad blocker programmed again.

**Result 1, the profile of the 22.5 frames/s image** (thousand cycles a frame, demo 1; demos 2 and 3 within 10 percent):

| Step | Cycles | Share |
|---|---|---|
| Whole frame | 7,415 | 100 percent |
| Game logic (`G_Ticker`) | 896 | 12 |
| Render setup and BSP (`R_RenderPlayerView`) | 2,017 | 27 |
| Frame drawing (`pd_end_frame`) | 4,381 | 59 |
| inside it: wall and sprite columns | 2,468 | 33 |
| inside it: flats and the second-core loop | 952 | 13 |
| inside it: plane setup | 574 | 8 |

Instruction cache: 2.45 million hits and 7,100 misses a frame, so code fetch is not the cost (7.4 million cycles for about 2.55 million instructions is 2.9 cycles each). The data cache counters stay 0: the data cache of this chip sits in front of the optional PSRAM only (the SDK driver flushes it only for PSRAM), and flash data is not cached.

**Result 2, the memories** (cycles per access): TCM 1.06, code RAM 1.6 load and 1.8 store, DRAM 6.2, flash 44 per word with single loads and 30 with eight-word loads, no gain on a second pass.

**Result 3, each change** (frames/s after frame 400, one run each, checksums equal to the emulator unless marked):

| Change | Frames/s |
|---|---|
| Section 11 image (everything in DRAM, data file and tables in flash) | 22.5 |
| Read-only tables (100 kB) copied to TCM at start | 23.2 |
| Lump RAM cache, 277 kB in DRAM (colormap, level, textures, sprites on first use) | 23.5 |
| Frame buffers alone in TCM | 24.1 |
| Statics, zone (40 kB), thread stack and heap in TCM, window base moved to 0x20000000; frame buffers and render list in DRAM | 34.5 |
| plus lump cache (22 kB TCM, DRAM) | 36.0 |
| plus render list in TCM, stack 4 kB (1.7 kB used), heap 8 kB, zone 32 kB, 28 kB of code RAM for lumps | 36.7 (shipped default) |
| Same code with cycle timers (layout change only) | 39.5 |
| Render files compiled -O2 on top | 39.3, but demo 3 checksum differs (engine's uninitialized stack array) |
| Hot render code in code RAM instead of lumps there | 39.4 on the timer build, no net change |

**What it shows.** The factor of two was memory placement, but DRAM, not the flash: the 512 kB DRAM answers in 6 cycles, so the engine's hot working set (statics, zone, stack, render list, about 100 kB) belongs in the 128 kB TCM, and everything with a 16-bit pointer must sit in one 256 kB window, which is why the window base moved. The lump cache is worth about 5 percent once the flash read cost (44 cycles) is paid once. The remaining gap to the estimate is the frame buffers and level data left in DRAM, and code-layout variation of up to 7 percent. **Not covered:** panel output, sound, other levels, a data-cache or flash-clock change (the flash controller registers read 0 from the application, so the read mode was not inspected).

## 14. The page-cache build on the LP-MSPM33C321A itself

Added 2026-10-21. Question from section 12: does the cache over the real QSPI finish the demos on the chip, and what do the model's guesses (400 cycles of driver software a read, one cycle an instruction) become when measured? Full account: [QSPI dump page](2026-10-21-mspm33c321a-qspi-dump-doom-data.md).

**Expectation (before the run).** Frame checksums and cache counters equal the emulator's; 41 ms a frame at 8 kB and 37 ms at 16 kB; QSPI wait about 12 ms at 8 kB.

**Result.** The data file sits in a blank run of the board's 16 MB QSPI flash (offset 0x640000; the video in the first 197 kB is unchanged, checked by three dumps). All three demos finish on the chip with the emulator's frame-checksum chain after each demo (`fe183649`, `c6686ada`, `fc7b6c2d`), the same frames, game tics and cache misses count for count. Frame time with the display and sound stubbed:

| Cache | Frame time | Frames a second | QSPI wait a frame | RAM free |
|---|---|---|---|---|
| 8 kB | 50.3 ms | 19.9 | 13.1 ms | 10.7 kB |
| 16 kB | 45.7 ms | 21.9 | 8.6 ms | 2.7 kB |

A miss costs 23.8 microseconds (3.8k cycles), not the modeled 400 cycles plus bus time; the CPU part of a frame is 37.1 ms, 1.65 cycles an instruction, not 1.0 to 2.0 assumed. The expectation's QSPI term held (12 ms against 13.1) and its CPU term was low (29 ms against 37.1). Each displayed frame holds three game tics, so the demo logic runs at 59.7 tics a second at 8 kB against the 35 the game needs; the frame rate at real-time pacing (about one tic a frame) was not measured.

**What it shows.** The software page cache works on the real chip with a read path TI ships, no write to the flash status register, and the cost is known: 13 ms of QSPI wait a frame at 8 kB, which is the size that leaves room for a display driver and sound. **Not covered:** screen, keys, sound, the six other levels, a real-time-paced frame rate.

## 15. The demos on the Educational BoosterPack's screen, with stick and buttons

Added 2026-10-21. Question from section 14: can the page-cache build on the LP-MSPM33C321A draw to the BoosterPack's 128 x 128 screen and read its stick and buttons in the RAM the 8 kB cache leaves (8.3 kB), without changing the game or slowing it much? Account and numbers: `ports/doom/m33-whx/README.md` section 8.

**Expectation (reasoned before the first run).** The 128 x 96 window takes 10 to 12 ms on the wire and hides behind the 37 ms of CPU work; the CPU pays about 1 ms a frame for the rows. A frame takes 51 to 52 ms instead of 50.3, and the frame checksums do not move.

**Result.** Doom's 320 x 200 picture goes to a 128 x 96 window (4:3, the shape the game is drawn for): the 168-row view fills 80 rows and two text lines (health, armor, ammo; mode and frames a second) take the 16 rows under it, in place of the status bar; title and other pages fill all 96 rows. No full frame buffer exists: two 256-byte row buffers, SPI DMA at 16 MHz, and the interrupt at the end of each row starts the next and converts the one after, so the engine renders the next frame while the last is on the wire. The display costs 2.4 kB of RAM.

| Run, all three demos, display on | Frames a second | Frame time |
|---|---|---|
| 8 kB cache, stepped clock (section 14's setting) | 19.3 | 51.7 ms |
| 8 kB cache, real time (35 tics a second) | 22.7 | 44.0 ms |
| 16 kB cache, real time (151 bytes of RAM left) | 24.5 | 40.9 ms |

With the stepped clock the frame-checksum chain after each demo is `fe183649`, `c6686ada`, `fc7b6c2d`, as without a display, and the display costs 1.5 ms a frame (19.9 frames a second without it). The 80-row stream takes 14.6 ms on average and the engine waited for it 2.7 ms in 7,585 frames. The expectation held for the checksums, the hidden stream and the cost (1.5 against 1 to 2 ms). Stick and buttons: in the demo loop S1 or S2 starts E1M1; then the stick walks and turns, S1 fires, S2 uses. A scripted player produced the same key events and played E1M1 for 2,328 tics (position, angle and ammo changed, 27 to 36 frames a second). The bench camera showed the color bars in order, the title logo, the text lines and a changing view; scene detail is below its resolution, so the picture was also read from the frame buffers over the probe and drawn on the host with the same scaler.

**The uninitialized stack array (section 12).** `draw_composite_columns` draws entries of `pixels[129]` it has not written; one line zeroing it (`ports/doom/m33-whx/patches/0003-pixels-init.patch`) is now part of every build. Frame checksum of all 3,881 frames of the cached build (emulator): `fc7b6c2d` at -Os, -O1 and -O2 with the patch; without it `b22b3fde` at -Os and -O1 and `53ccda64` at -O2. Same fix for the LP-EM-CC35X1 target, whose `-O2` build (39.3 frames a second) changed the demo 3 checksum: apply the patch to its tree and rebuild.

**Not covered.** The stick and buttons by hand (directions are the driver's assumption; the stick press is unusable on this LaunchPad); the menu and other overlays (not drawn); sound (compiled out for quiet hours); the zone high water in play mode; levels other than E1M1 in play.

## 16. The LP-EM-CC35X1 build at -O2 with the stack-array fix: 39.2 frames/s

**Question.** Section 13's -O2 build reached 39.3 frames/s but changed the demo 3 checksum. Is the cause the engine's uninitialized stack array (section 12), and does the fixed build keep the emulator's checksums? **Expectation (written before the runs).** With the array zeroed, all checksums equal the emulator's fixed run and the rate stays near 39.

**Method.** `ports/doom/target-cc35x1-whx/patches/0002-pixels-init.patch` (the m33-whx `0003`) is applied by `build.sh`; the render files (`pd_render.cpp`, `r_*.c`) are compiled -O2 by default. Zone 32 kB, 921,600 baud, through the probe lock. Reference: the emulator chain with the fix, `fe183649`, `c6686ada`, `fc7b6c2d` (section 12).

| Image | Frames 100 to 400 | Demo ends (frames) | Chain after demo 1, 2, 3 | Frames/s after frame 400 |
|---|---|---|---|---|
| -O2 render files, fix applied | equal to the emulator | 1,736, 3,092, 3,881: equal | `fe183649`, `c6686ada`, `fc7b6c2d`: equal | 39.2 |
| Same, checksum on every frame | | | equal | |
| -Os everywhere, fix applied, checksum on every frame | | | equal | |

**Result.** The expectation held. The old CC35X1 reference `e9db8f1d` for demo 3 was the unfixed engine at -Os; it is retired. The fix changes the last chain value at -Os too, so the CC35X1 now matches the other targets. -O2 is the default of `build.sh` (`HOTO2=""` gives -Os). One run per image; code layout moves the rate by up to 7 percent. After the runs the board was erased and the ad blocker programmed again.
