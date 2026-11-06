<!-- experiment
id: E149
board: LP-AM13E230
date: 2026-10-09
status: works
reached: verified
line: The radar stream parser used on the Mac runs on the LP-AM13E230 as 882 bytes of code and 152 bytes of state, fed from the USB serial port: 62 of 62 frame lines match the Mac parser on a real capture and 40 of 40 on a synthetic one, with no byte lost. The wire to a live radar was not made, and the real capture holds no point cloud, so range was checked on synthetic frames only.
stop: none
-->
# Can the LP-AM13E230 run the IWRL6432BOOST radar frame parser, with no wire to the radar?

**Short answer.** Yes. The parser that reads the IWRL6432BOOST's UART stream on the Mac was rewritten as a small streaming C parser and run on the LP-AM13E230, fed from the board's USB serial port by a replay from the Mac. On a real capture of 62 frames the board's output matched the Mac parser line for line, including the detected-object count in every frame. On a synthetic frame set of 40 frames it matched the Mac parser and the generator's own record, including the nearest range. The real radar was not wired to the board.

Terms: [vocab](../vocab.md). A TLV is a type, length, value record inside a radar frame. The backchannel UART is the board's USB serial port. 1 kB = 1,024 bytes on this page. Board page: [LP-AM13E230 README](../../am13e230/README.md), [FAQ](../../am13e230/FAQ.md). Radar: [IWRL6432BOOST](../../iwrl6432boost/README.md). Files: `am13e230/radar-parser/`. Run 2026-10-09 on macOS 15.7 (Intel) with the AM13E2x SDK 26.01.00.03, TI Arm Clang 5.1.1.LTS and SysConfig 1.28.0.

**Question.** A radar that reports presence and points over a UART needs a host that reads the stream. If the host is a motor-control MCU rather than a Mac, does the parsing fit and keep up?

**Vendor position.** TI's Radar Toolbox 4.00.00.05 motion and presence demo writes a magic word `02 01 04 03 06 05 08 07`, a 40-byte header (version, total length, platform, frame number, CPU cycles, detected objects, number of TLVs, sub-frame number, all 32-bit little endian) and then TLVs of type, length and payload. The type numbers come from `motion_detect.h` in the toolbox. The shipped demo sends only the presence TLVs (315 and 319), and the object count is in the header. Checked 2026-10-09.

**What was tried.**

1. Took the Mac parser, `iwrl6432boost/tools/tlv_parse.py`, and wrote `tlv_parser.c`: the same rules (sync on the magic word, a header whose length is under 40 or over 65,536 is skipped and the scan resumes after the magic word, TLVs read while 8 header bytes fit inside the frame), one byte at a time, nothing buffered. It reads TLV 315 (presence flag and first zone byte) and TLV 1 (float points, 16 bytes each, nearest range computed as the square root of x squared plus y squared plus z squared).
2. Wrote a board program that moves bytes from the backchannel UART (UC4, 115200) into the parser and prints one line per completed frame. Nothing blocks: the main loop alternates between the receive FIFO and the transmit FIFO through a 4 kB output ring, so a long print cannot overrun the receiver.
3. Replayed the real capture, a 15 s capture of the shipped demo's UART stream (23.5 kB, 62 frames, recorded the same day), through the board's serial port at line rate. `replay.py` waits for the board's banner, sends the bytes and compares the board's lines with the Mac parser run on the same file.
4. The capture has no point cloud, so range could not be tested on it. A synthetic frame set was built from the documented format (`make_synthetic.py`): 40 frames, 0 to 12 points each, TLV 1 and 7 and 315, one unknown TLV type, one frame with no TLVs, short bursts of noise between frames and a false header with length 10 that the parser must skip. **This set is synthetic. It is not radar data.**

**How far it got.** Verified, on both sets.

| Set | Size | Frames | Board lines that match the Mac parser | Other check |
|---|--:|--:|--|---|
| Real capture | 23.5 kB | 62 | 62 of 62 | Object counts: 59 frames with 0, one with 5, one with 7, plus a malformed first frame (count 838860801) that both parsers read the same way |
| Synthetic | 7.5 kB | 40 | 40 of 40 | 40 of 40 against the generator's own record of objects, points and nearest range |

The board's counters after the synthetic run: 7.5 kB received (every byte of the file), 40 frames, 0 receiver overruns, 0 dropped output bytes. A line looks like `F2 L306 O12 T3 P1,2 N12 R2121`: frame 2, 306 bytes, 12 objects, 3 TLVs, presence byte 1 and zone byte 2, 12 points, nearest 2,121 mm (synthetic). The replay of the real capture took 2.1 s at 115,200 baud against the 15 s the recording ran, so the board kept up with a stream seven times faster than the radar sends it.

**The line.** None reached in the parser. It is 882 bytes of code and a 152-byte state struct, plus the 4 kB output ring and the 4 kB stack the project's linker file gives; the whole image uses 7.7 kB of flash and 12.2 kB of RAM. A first estimate put a streaming presence parser at under 100 bytes of state; this repo's is 152 with a result record and a point buffer in it.

**Where it stopped.** At the wire. Real radar to board was not connected, so three things are open: whether the board's UART keeps up with the radar's own timing and any gaps, the 1,250,000 baud setting the demo can use, and a stream that carries the point cloud (the shipped demo configuration does not). **Setup or handling** for the missing wire. **Unknown** for the three open items: nothing in the replay suggests a problem, but the replay is faster than the radar and has no gaps.

- Wire the radar's UART to a second AM13 UART (a different unicomm and pins) and parse the live stream, with the radar powered from its own USB port.
- Replay at 1,250,000 baud by running the board's UART faster, to see where the parser loses bytes.
- Use the trig unit or the NPU for the range and a presence rule, rather than a square root in the C library.
- Run the same C file on an MSPM0 and compare flash, RAM and cycles per byte.
- Capture a live stream with a point-cloud configuration, so range is checked on real data.

**Ask.** Would a motor-control team put the radar parsing on the same MCU as the motor loop? The useful answer is how many bytes of RAM and what share of the CPU they would give it.

**What to do next.** The host side of this radar is small. A byte-at-a-time parser in 882 bytes of code handled a real capture and a synthetic set with no lost byte, on a part that has a motor loop to run beside it. Wire the radar to a second UART and capture a point-cloud stream.

## Questions

### Can a motor-control MCU parse the IWRL6432BOOST UART stream?

Yes on a replay. The parser is 882 bytes of code and 152 bytes of state, and all 62 real frames and 40 synthetic frames matched the Mac parser. **Setup or handling.** The live radar was not wired.

### Why is the nearest range only checked on synthetic frames?

The shipped demo sends no point cloud. **TI example default.** The real capture has TLV 315 and a header object count, so range was tested on 40 synthetic frames only.

### Will it keep up at the radar's 1,250,000 baud?

Not tested. **Unknown.** The replay ran at 115,200 baud with no gaps.

### What is the first frame's object count of 838860801?

A malformed first frame in the capture. **Setup or handling.** Both parsers read it the same way.
