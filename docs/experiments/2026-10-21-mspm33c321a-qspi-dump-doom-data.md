<!-- experiment
id: E27
board: LP-MSPM33C321A
date: 2026-10-21
status: in progress
reached: run
line: The board's QSPI flash was read twice with the SDK read path (identical, 16 MB part), the Doom data file was written into a blank run of it without touching the out-of-box video, and the rp2040-doom engine ran all three demos from it on the chip with the same frame checksums as the emulator. Frame time is 45.7 ms with a 16 kB cache and 50.3 ms with 8 kB (21.9 and 19.9 frames a second) with the display and sound stubbed; the QSPI wait is 8.6 and 13.1 ms of it.
stop: speed
-->
# Can the LP-MSPM33C321A's QSPI flash be dumped safely and hold the Doom data file?

**Short answer.** Yes. The 16 MB QSPI flash was read twice (identical, SHA-256 `7928a618...acd1a5`), the video that came with the board occupies only its first 197 kB, and the 1,758 kB Doom data file went into a blank run at offset 0x640000. A third dump showed every byte outside that run unchanged. The engine then played all three demos from the chip with the emulator's frame checksums.

Terms: [vocab](../vocab.md). 1 kB = 1,024 bytes; 1 MB = 1,048,576 bytes. **WHX** is the converted game-data file; **QE** is the flash's quad-enable bit.

**Expectation (written before the first board run).** The file reads back with CRC-32 `e9018e36` in about 175 ms; the three frame-checksum chain values equal the emulator's (`fe183649`, `c6686ada`, `fc7b6c2d`) and so do the cache counters; a frame takes about 41 ms with an 8 kB cache (3.6 million instructions at 1.3 cycles plus 549 misses at 22 microseconds) and about 37 ms with 16 kB; QSPI share about 12 ms.

**Question.** Can the LP-MSPM33C321A's on-board QSPI flash be read in full without losing the video that shipped in it, and can it hold the 1,758 kB Doom data file so the engine runs on the chip?

**Vendor position.** The board guide lists an 8 MB NOR flash. The chip's own identification and SFDP density say 16 MB (JEDEC C2 20 18); the guide's figure is wrong for this board.

**What was tried.** A read-only identify and dump (twice), a guarded write of the data file into a blank run, a third dump to prove nothing else moved, then the engine on the chip with a software page cache over the QSPI. The steps, guards and results are in the next section.

**How far it got.** Run: all three demos play to the end on the chip with the emulator's frame checksums, 19.9 frames a second with an 8 kB cache and 21.9 with 16 kB.

**The line.** Speed with the display and sound stubbed: 50.3 ms a frame at 8 kB and 45.7 ms at 16 kB, of which the QSPI wait is 13.1 and 8.6 ms. A 16 kB cache leaves 2.7 kB of RAM free. The panel, keys and sound are not on.

**Ask.** Has anyone read the same flash part with a different tool and found a different density than the board guide's 8 MB? The display and keys work needs the Educational BoosterPack stacked on this board.

## What was done

Plan: dump twice and compare, write only into a blank run behind guards, dump a third time and compare. Code: `ports/doom/m33-whx/board/` (`qspi_dump.c`, `qspi_write.c`, `doom_board.syscfg`, build scripts) and `ports/doom/m33-whx/tools/` (`qspi_host.py`, `run_board.py`). Dumps and logs stay outside the repository (the dump is TI's video).

| Step | Result |
|---|---|
| Identify (read-only) | ID C2 20 18; SFDP density 0x07FFFFFF bits, so 16 MB, not the 8 MB the board guide says; status 0x40 (QE already set, no block-protect bit); config register 0x07; normal read and fast read agree on the first 64 bytes |
| Dump A and B | 128 blocks of 128 kB each, every block checked against a CRC-32 computed on the chip; the two files are identical; 12 to 13 minutes each over the probe |
| Where the data is | Blocks 0 to 3 of 64 kB hold the video; the last non-0xFF byte is at 0x302FD; everything above is 0xFF. Highest blank run of 28 blocks: 0xE40000 to the end of the 16 MB. Offset 0x640000 (the plan's figure for an 8 MB part) is blank too and was used |
| Write | 14 chunks, 7,033 pages of 256 bytes, 0.2 to 0.4 s of chip time per 128 kB. Guards on the chip: ID, no protect bit, window, staged-data CRC, target all 0xFF, read-back compare. Each chunk: blank_ok 1, verify_bad 0 |
| Dump C | Outside 0x640000..0x7F7877 equal to dump A; the run equals `DOOM1.whx` byte for byte |
| Run on the chip | Section below |

Changes from the plan, each forced by what the chip or the board showed:
1. The size is 16 MB, not 8 MB. A larger part only adds room; the dump covers all of it.
2. Command 0x35 is not sent. On this Macronix part it enters QPI mode; the config register is read with 0x15.
3. The data does not leave over the application UART. The Education BoosterPack covers those pins, so each 128 kB block goes to chip RAM and the host reads it with DSLite. A DSLite `load` of 4 bytes is the request; it restarts the program, so the mailbox sits outside every linked section. DSLite counts the length of a memory range in 32-bit words.
4. The QE bit was already set, so the quad read (0xEB, 1-4-4, 6 dummy clocks, 40 MHz) needed no write to the flash status register.

## Run on the chip (Doom, all three demos)

Image: rp2040-doom with the page cache, 30 kB zone, `PXINIT=1`, 160 MHz, clock stepped 35 ms a read as in the emulator so the demos play the same tics per frame. Frame time is DWT cycles in the frame loop with the checksum time left out; the display and sound are stubs. The file in flash is checked against its CRC-32 before the demos start.

| | Chip | Emulator |
|---|---|---|
| File in flash | CRC-32 `e9018e36`, read in 339 ms (7,033 reads) | n/a |
| Frame-checksum chain after demo 1, 2, 3 | `fe183649`, `c6686ada`, `fc7b6c2d` | same |
| Frames, game tics at the end | 1,736 / 3,092 / 3,881; 5,197 / 9,235 / 11,571 | same |
| Cache reads, all three demos (8 kB) | 2,131,318 misses in 125.7 million accesses (98.33, 98.36, 98.17 % per demo) | same, count for count |

| Cache | Frame time, mean (longest) | Frames a second | QSPI wait a frame | Misses a frame | RAM free |
|---|---|---|---|---|---|
| 8 kB | 50.3 ms (104.8) | 19.9 | 13.1 ms (worst 27.6) | 549 | 10.7 kB |
| 16 kB | 45.7 ms (97.8) | 21.9 | 8.6 ms (worst 20.3) | 360 | 2.7 kB |
| 8 kB with hit counters | 61.6 ms | 16.2 | 13.1 ms | 549 | |

Per demo at 8 kB: 51.0, 47.4 and 53.6 ms a frame; demo 3 is the busiest, as in the emulator.

What the numbers say:
- A miss costs 23.8 microseconds, 3.8k cycles. The bus time is 526 clocks at 40 MHz (2.1k CPU cycles); the SDK's polled read adds about 1.7k. The model assumed 400 cycles of software, so the model's QSPI wait (8.6 ms at 8 kB) was 1.5 times too low; the model's preference for 128-byte pages may not hold, since the software cost per read is higher than it assumed (not run).
- The CPU part of a frame is 37.1 ms at either cache size, which is about 5.9 million cycles for the 3.6 million instructions the emulator counts: 1.65 cycles an instruction (flash wait states, branches, byte accesses).
- The expectation said 41 ms (8 kB) and 37 ms (16 kB); the chip took 50.3 and 45.7 ms. The QSPI term was right (12 ms expected, 13.1 measured); the CPU term was low (29 ms expected, 37.1 measured, 1.3 against 1.65 cycles an instruction).
- Each displayed frame here holds three game tics. The three demos advance at 59.7 tics a second at 8 kB and 65.7 at 16 kB, against the 35 the game needs, so the engine runs the demo logic 1.7 to 1.9 times faster than real time. At real-time pacing a frame holds about one tic and costs less than these figures; that case was not measured.
- Counting cache hits costs 11 ms a frame (61.6 against 50.3): the counters are in the hottest path, so the timing builds leave them out.

## What this does not cover

The screen, the keys and the sound are stubs. The six levels the demos do not play, save and load and the automap were not run. One chip, one flash. The video was not played after the write (nobody at the board); the dump comparison shows its 197 kB unchanged.

## Reproduce

```bash
# read-only dump (flash the firmware, then the host script, both under the probe lock)
ports/doom/m33-whx/board/build_qspi_dump.sh
python3 tools/frl/lock.py exec --serial <probe> -- python3 -I ports/doom/m33-whx/tools/qspi_host.py dump <out.bin>
python3 -I ports/doom/m33-whx/tools/qspi_host.py analyze <out.bin> 28
# write the data file (guarded), then run the engine
ports/doom/m33-whx/board/build_qspi_write.sh
python3 tools/frl/lock.py exec --serial <probe> -- python3 -I ports/doom/m33-whx/tools/qspi_host.py write ports/doom/wad-cache/DOOM1.whx 0x640000 <dumpA.bin>
WHXCACHE=8 PXINIT=1 BOARD=1 WHX_CRC32=0xe9018e36 ports/doom/m33-whx/build.sh 30
python3 tools/frl/lock.py exec --serial <probe> -- python3 -I ports/doom/m33-whx/tools/run_board.py <doom.out> c8
```
The figures are in the tables above.
