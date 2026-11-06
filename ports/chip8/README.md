# CHIP-8 on the LP-MSPM0G3519 with a touch panel

A CHIP-8 interpreter (about 200 lines of portable C) runs the six chip8-test-suite ROMs on the LP-MSPM0G3519 and the Waveshare 3.5 inch touch panel. The 64 x 32 screen is drawn at 5x (320 x 160) at the top, a ROM bar sits under it (touch the left half for the previous ROM, the right half for the next) and a 4 x 4 key pad fills the bottom. The sound timer runs and nothing is played.

Terms: [vocab](../../docs/vocab.md). 1 kB = 1,024 bytes. **CHIP-8** is a 1970s virtual machine with 4 kB of memory, sixteen 8-bit registers, a 64 x 32 monochrome screen and a 16-key pad. **Quirk** is a place where interpreters differ on purpose; the test suite checks them.

## What it does

| Part | What | Source |
|---|---|---|
| Interpreter core | All CHIP-8 opcodes, the six quirks as fields (COSMAC VIP behavior is the default), 60 Hz timers, display wait, key wait with release. No hardware, no allocation | `src/chip8.c`, `src/chip8.h` |
| Front end | Scaled screen with only the changed rows redrawn, the ROM bar, the 16-key pad, touch to key | `src/app.c` |
| Display and touch | Shared with [touch-games](../touch-games/README.md): the same `hal.h`, board file, SysConfig file and drawing code | `../touch-games/src` |
| Test ROMs | The chip8-test-suite (GPL-3.0) is fetched, never stored here; `build.sh` turns it into a C array | `tools/fetch_tests.sh`, `tools/mkroms.py` |

## Run it on the Mac (no board)

```bash
bash ports/chip8/host/run_host.sh     # fetches the ROMs, runs six screens, compares each with host/expected.sha256
```

The hashes were taken from screens looked at against the suite's own pictures: every check mark on the Corax+ and flags screens, all six quirk lines "ok" for the CHIP-8 platform, both logos, and the keypad test with keys 1 and 5 held. `tools/txt2png.py` turns a host screen into a picture.

## Build and flash for the LP-MSPM0G3519

```bash
ports/chip8/tools/fetch_tests.sh                    # the six ROMs, into a roms folder
BOOT_ROM=2 BENCH=0 ports/chip8/build.sh             # starts on the Corax+ screen, skips the speed run at boot
ports/chip8/tools/flash_look.sh                     # DSLite, a 16-byte padded .bin at 0x0
```

`BOOT_ROM` is the index of the first ROM (0 logo, 1 IBM, 2 Corax+, 3 flags, 4 quirks, 5 keypad). More ROMs: put `.ch8` files in `ports/chip8/roms/` and rebuild; check each one's license first. The serial console (115200 baud, jumpers J12 and J13 on the XDS side) prints the speed run: `/usr/bin/python3 ports/chip8/tools/uart_reset.py 12`.

The quirks ROM starts with the CHIP-8 platform already chosen (its automation byte at 0x1FF); build with `-DAUTO_PLATFORM=0` to get the menu and choose with the pad. The keypad ROM's menu uses keys 1 to 3.

## Results

One LP-MSPM0G3519, 80 MHz, code in flash, display at 40 MHz SPI. Rows are in `RESULTS-ROWS-wave5.csv`.

| What | Value |
|---|---|
| Interpreter speed with no screen, 200k instructions per ROM, display wait off | 526k instructions per second on the logo, IBM, Corax+ and flags ROMs; 473k on the quirks and keypad ROMs |
| Headroom over the usual game pace (about 700 instructions per second) | about 700 times (computed) |
| Full redraw of the 64 x 32 screen (51,200 pixels) | 14.2 ms on the logo ROM (firmware timestamp) |
| Frame buffer on the board against the host after the same ROM ran | byte for byte equal for logo, IBM, Corax+, flags and quirks (2 kB each) |
| Flash | 13.8 kB play build, 14.5 kB with the speed run; 6.3 kB of that is the six ROMs |
| RAM (data and bss, 1 kB stack included) | 10.4 kB |

### What was checked and what was not

- Checked: the frame buffer read through OpenOCD after each ROM settled equals the host interpreter's screen (`tools/verify_board.py`), and the host screens match the suite's pictures by eye. A camera sees the Corax+ screen lit with the same layout; at its resolution a check mark and a cross are not told apart, so the byte comparison is the evidence.
- Not checked: touch. No finger touched the panel, so the pad and the ROM bar have not been pressed and the touch axes are as untested as in [touch-games](../touch-games/README.md) (`TOUCH_SWAP_XY`, `TOUCH_FLIP_X`, `TOUCH_FLIP_Y` in `hal_mspm0.c`). Quirk sets other than CHIP-8 (SUPER-CHIP, XO-CHIP) are not run on the board; SUPER-CHIP opcodes and the 128 x 64 screen are not implemented.
- Limits: one touch at a time; the pad holds a key while touched.

## Files

`src/` core and front end, `host/` Mac checks, `tools/` fetch, ROM packer, flash and board check scripts, `build.sh`, and the measured rows in `RESULTS-ROWS-wave5.csv`.

License of this port: the repository's own. The interpreter is written from the public opcode tables; no CHIP-8 interpreter source is copied. The test suite (GPL-3.0, Timendus) is run, not copied.
