# ZX Spectrum 48K on the LP-MSPM0G3519 and a touch panel

A ZX Spectrum 48K runs on the LP-MSPM0G3519 (Cortex-M0+, 80 MHz, 128 kB of RAM) with the Waveshare 3.5 inch touch panel: the Z80 core from floooh/chips (zlib), a small machine around it, the 256 x 192 screen drawn 1:1 and a 40-key touch keyboard under it. It runs at about 19 percent of a real Spectrum's speed. The 16 kB system ROM is copyrighted: the reader supplies it. Without one the build runs a ROM-less test program that contains no Sinclair code.

Terms: [vocab](../../docs/vocab.md), [LP-MSPM0G3519 FAQ](../../mspm0g3519/FAQ.md). 1 kB = 1,024 bytes. **T-state** is one Z80 clock tick; a real 48K runs 3.5 million per second and 69,888 per 50 Hz frame. **Speed** below is emulated T-states per second as a share of 3.5 million.

## What it does

| Part | What | Source |
|---|---|---|
| Z80 core | chips `z80.h`, one T-state per call, unmodified | `../../third-party/chips/z80.h` (zlib) |
| Machine | ROM at 0x0000, 48 kB RAM from 0x4000, the ULA port (keyboard half-rows, border), the 50 Hz interrupt held for 32 ticks, screen decode with attributes and flash. Not modelled: contended memory, the floating bus, tape, and the beeper (silent) | `src/zx.c`, `src/zx.h` |
| Front end | Screen drawn per 8-line band only when its bytes changed (shadow copy), border color, 40-key keyboard with latched CAP and SYM, one touch read per frame | `src/app.c` |
| Display and touch | Shared with [touch-games](../touch-games/README.md) | `../touch-games/src` |
| ROM-less test image | Boot stub plus a hand-assembled Z80 program: fill the bitmap, then rewrite 768 attribute bytes every frame after `HALT` | `tools/mkrom.py` |

## The ROM

The Spectrum ROM is not in this repository and nothing here fetches one. Supply a 16,384-byte image: `ZX_ROM=/path/to/rom ports/zx-spectrum/build.sh`. Check the license of the file you choose. The ROM used for the runs below was a freely licensed replacement firmware (OpenSE BASIC 3.1.2, distributed under GPL-2.0); it stays outside this repository, and its banner names its own copyright holders. Whether a replacement ROM is clear of the original's rights is the reader's call.

## Run it on the Mac (no board)

```bash
bash ports/zx-spectrum/host/run_host.sh     # ROM-less image: bitmap fill, one HALT wake-up per frame, screen decode, keyboard and border ports
```

## Build and flash for the LP-MSPM0G3519

```bash
RAMFUNC=1 ports/zx-spectrum/build.sh                          # ROM-less test image; the Z80 core runs from SRAM bank 1
ZX_ROM=/path/to/rom RAMFUNC=1 ports/zx-spectrum/build.sh      # with your ROM
ports/chip8/tools/flash_look.sh <build folder>/zx-g3519.bin
/usr/bin/python3 ports/chip8/tools/uart_reset.py 20           # speed log, 115200 baud, jumpers J12 and J13 on the XDS side
```

`STOP_AFTER=400` stops the machine at a frame boundary after 400 frames so the screen bytes can be compared with the host (`tools/verify_board.py`). `OPT=-O3` and `RAMFUNC=0` are the slower variants measured below.

## Results

One LP-MSPM0G3519, 80 MHz, TI Arm Clang 5.1.1, display at 40 MHz SPI. Speed is read from the firmware's own timers over 100 frames; the machine runs flat out (no 50 Hz pacing). Rows are in `RESULTS-ROWS-wave5.csv`. CoreMark scaling from an RP2040 at 400 MHz predicted about 0.20 of a real Spectrum's speed; the measurement:

| Build | Workload | Core only | With the screen |
|---|---|---|---|
| `-O2`, code in flash | ROM-less test image (attribute rewrite every frame) | 15.9 % | 12.3 % |
| `-O3`, code in flash | same | 14.0 % | 11.1 % |
| `-O2`, Z80 core in SRAM | same | 18.9 % | 14.0 % |
| `-O2`, Z80 core in SRAM | OpenSE BASIC idle at the keyboard wait | 19.4 % | 19.0 % |

So a real Spectrum's pace is out of reach on this chip by a factor of five: BASIC input and slow games run, anything that needs real-time timing does not. The screen costs 35 ms per frame on the test image because every band changes every frame (49,152 pixels at 40 MHz SPI); with BASIC idle the screen costs 1 ms per frame.

| Size | Value |
|---|---|
| Flash (ROM 16 kB included) | 62.5 kB for the SRAM build |
| RAM (data and bss, 2 kB stack included) | 61.0 kB: 48 kB Spectrum RAM, 7 kB screen shadow, 4 kB draw buffer |

### Checks and limits

- Checked: after 400 emulated frames, the screen memory (bitmap and attributes, 6.75 kB) on the board equal the host machine's, for the ROM-less image and for OpenSE BASIC (at its keyboard wait, program counter 0x15DF); the same Z80 core ran the same instructions to the same state. The host checks cover the screen decoder, the keyboard half-rows and the border port.
- The bench camera shows the attribute pattern of the test image and the keyboard. The OpenSE banner is white 8-pixel text at the bottom of a black screen and is not legible at the camera's resolution; the byte comparison is the evidence for it.
- Not checked: touch (no finger; the keyboard is drawn and polled, nothing pressed), tape, sound (not modelled, by design), contended memory timing, and the Z80 core against zexall on the board (the chips project reports passing it; this repository did not rerun it).

## Files

`src/` machine and front end, `host/` Mac checks and the dump helper, `tools/` ROM image maker, board comparison and picture scripts, `build.sh`. Results rows: `RESULTS-ROWS-wave5.csv`.
