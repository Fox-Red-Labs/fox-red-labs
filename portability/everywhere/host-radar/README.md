# IWRL6432 radar frame parser on the SK-TDA4VM Linux processor

**It runs.** The radar frame parser written for the LP-AM13E230 (`am13e230/radar-parser/tlv_parser.c`, plain C, 882 bytes of code) built with the SK-TDA4VM's own gcc on its Linux (Cortex-A72) on 2026-10-11 and parsed a 60-second recorded capture from the IWRL6432BOOST running TI's Motion and Presence 2D demo: 231 frames, every line identical to the Mac reference parser (`iwrl6432boost/tools/tlv_parse.py`) and to this Mac's build of the same C. Matrix id X34 in `portability/matrix.csv`. The radar's own firmware (Cortex-M4F plus the hardware accelerator) cannot run on an A72; what a Linux processor takes from the radar is its output, and this is that path, proven.

Terms: [docs/vocab.md](../../../docs/vocab.md). 1 kB = 1,024 bytes.

| File | Role |
|---|---|
| `tlv_parser.c`, `tlv_parser.h` | the parser, unchanged from the AM13 version |
| `host_main.c` | 20 lines: read a capture, feed bytes, print one line per frame in the board's format `F L O T P N R` |
| Check | the 231 lines this Mac's build printed for the 60 s capture matched the SK-TDA4VM's output exactly (`cmp`) |

How it got there: the five files as a tar.gz (76 kB), base64 through the serial console in 13 s, `gcc -O2 -std=c99 host_main.c tlv_parser.c`, run, `cmp`. Build and run on any host: `cc -O2 -std=c99 host_main.c tlv_parser.c -o radar_host && ./radar_host <capture.bin>`.

Next for the TDA4 and the radar: the live path, the radar's UART into one of the TDA4's serial ports with this parser behind it, which needs a cable between the two boards.
