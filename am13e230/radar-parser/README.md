# IWRL6432BOOST radar frame parser on the TI LP-AM13E230, replayed from a capture with no wire to the radar

Short answer: `./build.sh --flash`, then `python3 replay.py <capture.bin>` sends a recorded radar capture over the board's USB serial port, and the board prints one line per frame it parsed. Both checks passed on 2026-10-09: 62 of 62 frame lines match the Mac parser on a real capture, and 40 of 40 on a synthetic one. Write-up: [the experiment page](../../docs/experiments/2026-10-09-am13e230-radar-host-parser.md).

Board: [LP-AM13E230 README](../README.md), [FAQ](../FAQ.md). The Mac parser is `iwrl6432boost/tools/tlv_parse.py`. Checked with the AM13E2x SDK 26.01.00.03, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0 and macOS 15.7.

| File | What it is |
|---|---|
| `tlv_parser.c`, `tlv_parser.h` | The parser: plain C, one byte at a time, 882 bytes of code and 152 bytes of state |
| `main.c`, `example.syscfg` | The board program: backchannel UART at 115200, bytes in to the parser, one line out per frame |
| `build.sh` | Build, and with `--flash` flash, over the board's debug probe |
| `replay.py` | Sends a capture to the board and compares the board's lines with the Mac parser (add `--truth synthetic.json` for the generator's own record) |
| `make_synthetic.py`, `synthetic.bin`, `synthetic.json` | A SYNTHETIC frame set built from the documented format. It is not radar data |

The line the board prints: `F<frame> L<length> O<objects> T<TLVs> P<byte0>,<zone0> N<points> R<nearest range in mm>`, with -1 for absent.

If `replay.py` says `no RADARPARSE READY banner in 10 s`, the board already received bytes since its last reset; flash again or press the reset button. **Setup or handling.**

The first frame line of the real capture has an object count of 838860801. Both parsers read the capture's first bytes the same way, so the line matches; the cause of the odd header was not checked. **Unknown.**

What is open: the real wire between a radar and the board.
