# Retro games on a TI LaunchPad

A Game Boy game, Arduboy games, 2048, Minesweeper, CHIP-8 and a ZX Spectrum, each on a screen wired or plugged onto a LaunchPad.

The page: [site/project-arcade.html](../../site/project-arcade.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Game Boy: Tobu Tobu Girl on the BoosterPack screen

An LP-MSPM33C321A with the Educational BoosterPack plays a Game Boy game at the console's speed. No wiring.

Status: Works. Boards: mspm33c321a.

Folders this version uses (in a sparse clone, fetch them first, then read `ports/AGENTS.md`):

```bash
git sparse-checkout add ports portability/boosterpacks mspm33c321a tools mac-setup
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM33C321A LaunchPad development kit | 1 | runs the Peanut-GB emulator; the 256 kB game sits in its 1 MB of flash |
| BOOSTXL-EDUMKII Educational BoosterPack MKII | 1 | the 128 by 128 screen and the stick and buttons |

### Before you start

- A Mac with the MSPM33 SDK 1.04.00.00, TI Arm Clang 5.1.1.LTS and SysConfig 1.28.0 in the default `/Applications/ti` folders, and CCStudio 21.0.1 for its `DSLite` flasher (`mac-setup/README.md`).
- The code: `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git`, then `git sparse-checkout add ports portability/boosterpacks mspm33c321a tools mac-setup`. Every command on this page runs from the repository folder.
- The game is Tobu Tobu Girl by Tangram Games (code MIT, art and music CC BY 4.0). `fetch_roms.sh` downloads it and stops on a hash mismatch; no commercial ROM is used or stored.
- The board ships with a demo in its flash. To keep it, back it up first with the command in the board's `AGENTS.md`.

### Steps

1. **Mac, terminal.** Download the two freely licensed test ROMs and check their hashes.

   ```text
   bash ports/peanut-gb/fetch_roms.sh
   ```

   You should see: `ports/peanut-gb/roms/` holds Tobu Tobu Girl and dmg-acid2.

   If it stops on a hash mismatch, delete `roms/` and run it again; do not use a file it refused.

2. **Mac, terminal.** Build the Educational BoosterPack app with the Game Boy emulator at 160 MHz, drawing every second frame.

   ```text
   cd portability/boosterpacks/edumkii
   APP=gameboy EDU_CLK160=1 GB_FLAGS=-DPGB_FRAME_SKIP EDU_TAG=skip ./build.sh mspm33c321a
   ```

   You should see: The script prints the build folder and the `.out` image.

   If the SDK is not found, install it as `mac-setup/README.md` says and run the line again.

3. **Mac, terminal.** Stack the Educational BoosterPack on the LaunchPad. Copy `mspm33c321a/MSPM33C321A_<probe serial>.ccxml` to `mspm33c321a/MSPM33C321A_<serial>.ccxml` with your probe serial in place of `<probe serial>` (`xdsdfu -e` lists it), then flash the `.out` the build printed.

   ```text
   /Applications/ti/ccs2101/ccs/ccs_base/DebugServer/bin/DSLite flash -c mspm33c321a/MSPM33C321A_<serial>.ccxml -e -f -v <build folder>/<image>.out
   ```

   You should see: The logos run first; the Tobu Tobu Girl title appears after about 28 seconds. A and B together press Start, A starts the game.

   If DSLite reports `Device Connection Error ... low power mode`, reset the probe with `xds110reset -s <serial> -a toggle -d 200` (it sits beside `xdsdfu`) and flash again.

### Expected result

The BoosterPack screen shows Tobu Tobu Girl running at the console's speed.

### What this shows

- A Game Boy emulator keeps console speed on a Cortex-M33 at 160 MHz with a 128 by 128 screen: 0 late frames in 8,167.
- The 160 by 144 picture is scaled to 128 by 128 by a lookup table while the next line renders and the rows stream out by DMA.

### Limits

- Nobody has played it by hand: the buttons were driven by script on the Mac and the stick and buttons were not judged by a person on the board.
- The screen shows every second frame, about 30 a second.
- Aspect ratio: the Game Boy picture is 160 by 144 (10 to 9); the screen shows it at 128 by 128, so it is 20 percent narrower and 11 percent shorter. The true ratio is not kept.
- Joystick up was not checked by hand on the board, so the direction mapping is unconfirmed.
- No sound, no saved games.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Image | 275 kB of flash for the 256 kB game plus emulator, 32.4 kB of RAM at the first image | `data/results/mspm33c321a.csv` |
| Speed steps | table scaler, DMA rows, frame skip, 160 MHz: 2.85 million instructions a frame down to 0.59 million | `ports/peanut-gb/RESULTS-ROWS-wave4.csv` |

## Arduboy games: Snake on the BoosterPack screen

Open-source Arduboy games run on a 700-line Arduboy2 layer. Snake is drawn on the Educational BoosterPack screen.

Status: Works. Boards: mspm33c321a.

Folders this version uses (in a sparse clone, fetch them first, then read `ports/AGENTS.md`):

```bash
git sparse-checkout add ports portability/boosterpacks third-party mspm33c321a tools mac-setup
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM33C321A LaunchPad development kit | 1 | runs the game |
| BOOSTXL-EDUMKII Educational BoosterPack MKII | 1 | the screen |

### Before you start

- A Mac with the MSPM33 SDK 1.04.00.00, TI Arm Clang 5.1.1.LTS and SysConfig 1.28.0 in the default `/Applications/ti` folders, and CCStudio 21.0.1 for its `DSLite` flasher (`mac-setup/README.md`).
- The code: `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git`, then `git sparse-checkout add ports portability/boosterpacks third-party mspm33c321a tools mac-setup`. Every command on this page runs from the repository folder.
- The board ships with a demo in its flash. To keep it, back it up first with the command in the board's `AGENTS.md`.

### Steps

1. **Mac, terminal.** Build the Educational BoosterPack app with Snake, starting the game by itself.

   ```text
   cd portability/boosterpacks/edumkii
   APP=snake AUTOSTART=1 ./build.sh mspm33c321a
   ```

   You should see: The script prints the build folder and the `.out` image.

   If the SDK is not found, install it as `mac-setup/README.md` says and run the line again.

2. **Mac, terminal.** Stack the Educational BoosterPack on the LaunchPad. Copy `mspm33c321a/MSPM33C321A_<probe serial>.ccxml` to `mspm33c321a/MSPM33C321A_<serial>.ccxml` with your probe serial in place of `<probe serial>` (`xdsdfu -e` lists it), then flash the `.out` the build printed.

   ```text
   /Applications/ti/ccs2101/ccs/ccs_base/DebugServer/bin/DSLite flash -c mspm33c321a/MSPM33C321A_<serial>.ccxml -e -f -v <build folder>/<image>.out
   ```

   You should see: The snake and its food are drawn on the 128 by 128 screen.

   If DSLite reports `Device Connection Error ... low power mode`, reset the probe with `xds110reset -s <serial> -a toggle -d 200` (it sits beside `xdsdfu`) and flash again.

### Expected result

Snake runs on the BoosterPack screen.

### What this shows

- Arduboy2-style games link unchanged against a 700-line layer: 5.5 to 9.6 kB of flash and 3.3 to 3.8 kB of RAM each.

### Limits

- Nobody has steered it by hand: the stick and buzzer were not read by a person. TileWorld and Crate Confusion link and run on the Mac; they were not drawn on the board.
- Aspect ratio: the Arduboy screen is 128 by 64 (2 to 1) and the layer doubles it to 128 by 128, so the picture is stretched to twice the height. The true ratio is not kept.
- Joystick up was not checked by hand on the board. The EDUMKII stick rests off center; the layer learns the center from the first samples.
- No sound.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Snake on the M33 | 20.7 kB of flash, 6.8 kB of RAM, 32.9 frames a second at 32 MHz | `data/results/mspm33c321a.csv` |

## 2048 and Minesweeper on a 3.5 inch touch panel

An LP-MSPM0G3519 draws two phone-style games on the Waveshare panel: swipe in 2048, tap and long press in Minesweeper.

Status: Works. Boards: g3519.

Folders this version uses (in a sparse clone, fetch them first, then read `ports/AGENTS.md`):

```bash
git sparse-checkout add ports mspm0g3519 tools mac-setup
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM0G3519 LaunchPad development kit | 1 | runs the game |
| Waveshare 3.5 inch capacitive touch LCD (ST7796S screen, FT6336U touch) | 1 | the screen and touch panel |

### Before you start

- A Mac with the MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS and SysConfig, and CCStudio 21.0.1 for its `DSLite` flasher (`mac-setup/README.md`).
- The code: `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git`, then `git sparse-checkout add ports mspm0g3519 tools mac-setup`. Every command on this page runs from the repository folder.
- The board ships with a demo in its flash. To keep it, back it up first with the command in the board's `AGENTS.md`.

### Steps

1. **Bench.** Wire the Waveshare 3.5 inch panel with 13 jumper wires, at 3.3 V only. Power: VCC to J1 pin 1 (3V3), GND to J2 pin 20 (GND). Screen: SCLK to J1 pin 7 (PB31), MOSI to J2 pin 15 (PB8), MISO to J2 pin 14 (PB7, optional), CS to J2 pin 13 (PB12), DC to J4 pin 39 (PB9), BL to J4 pin 38 (PC4), RST to J4 pin 37 (PC5). Touch: SDA to J1 pin 10 (PC3), SCL to J1 pin 9 (PC2), INT to J2 pin 11 (PB14, optional), TP_RST to J2 pin 12 (PB11). Leave the panel's own 3V3 pin and SD_CS empty.

   You should see: Thirteen wires run from the panel to the headers listed, with nothing on J1 pin 2 to 6 or the other pins.

   If the screen stays dark later, check VCC is on J1 pin 1 and not the panel's own 3V3 pin.

2. **Mac, terminal.** Build for the LP-MSPM0G3519. Add `BOOT_GAME=1 BENCH=0` to start straight in 2048.

   ```text
   bash ports/touch-games/build.sh g3519
   ```

   You should see: The script prints the build folder and `touch-games-g3519.out`.

   If the SDK is not found, install it as `mac-setup/README.md` says and run the line again.

3. **Mac, terminal.** Copy `mspm0g3519/MSPM0G3519_<probe serial>.ccxml` to `mspm0g3519/MSPM0G3519_<serial>.ccxml` with your probe serial, then flash the `.out`.

   ```text
   /Applications/ti/ccs2101/ccs/ccs_base/DebugServer/bin/DSLite flash -c mspm0g3519/MSPM0G3519_<serial>.ccxml -e -f -v <build folder>/touch-games-g3519.out
   ```

   You should see: The panel shows the launcher: two buttons. Tap 2048, swipe a tile.

   If DSLite reports `Device Connection Error ... low power mode`, reset the probe with `xds110reset -s <serial> -a toggle -d 200` (it sits beside `xdsdfu`) and flash again.

### Expected result

The panel shows the launcher with two buttons. 2048 takes swipes and Minesweeper takes taps.

### What this shows

- Two games fit in 9.9 kB of flash and 3.5 kB of RAM beside a gesture layer. They port to another board by writing eight small functions.

### Limits

- Minesweeper by finger was not judged by a person; its timing is from scripted taps.
- Aspect ratio: both games draw 1 to 1 on the 320 by 480 panel in portrait; there is no source picture to stretch. There is no stick: the games take touch only, so the joystick-up check does not apply.
- The 40 MHz SPI used is above the panel's rating of about 15 MHz; at 20 MHz a swipe redraw takes 61.6 ms.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Size | 9.7 kB of flash and 3,472 of RAM | `ports/touch-games/RESULTS-ROWS.csv` |
| Redraws at 40 MHz SPI | 2048 swipe 32.6 ms mean; Minesweeper one-cell tap 1.09 ms | `ports/touch-games/RESULTS-ROWS.csv` |

## CHIP-8 on the touch panel

A 200-line CHIP-8 interpreter runs the chip8-test-suite on the LP-MSPM0G3519 and the Waveshare panel.

Status: Works. Boards: g3519.

Folders this version uses (in a sparse clone, fetch them first, then read `ports/AGENTS.md`):

```bash
git sparse-checkout add ports mspm0g3519 tools mac-setup
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM0G3519 LaunchPad development kit | 1 | runs the game |
| Waveshare 3.5 inch capacitive touch LCD (ST7796S screen, FT6336U touch) | 1 | the screen and touch panel |

### Before you start

- A Mac with the MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS and SysConfig, and CCStudio 21.0.1 for its `DSLite` flasher (`mac-setup/README.md`).
- The code: `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git`, then `git sparse-checkout add ports mspm0g3519 tools mac-setup`. Every command on this page runs from the repository folder.
- The board ships with a demo in its flash. To keep it, back it up first with the command in the board's `AGENTS.md`.

### Steps

1. **Bench.** Wire the Waveshare 3.5 inch panel with 13 jumper wires, at 3.3 V only. Power: VCC to J1 pin 1 (3V3), GND to J2 pin 20 (GND). Screen: SCLK to J1 pin 7 (PB31), MOSI to J2 pin 15 (PB8), MISO to J2 pin 14 (PB7, optional), CS to J2 pin 13 (PB12), DC to J4 pin 39 (PB9), BL to J4 pin 38 (PC4), RST to J4 pin 37 (PC5). Touch: SDA to J1 pin 10 (PC3), SCL to J1 pin 9 (PC2), INT to J2 pin 11 (PB14, optional), TP_RST to J2 pin 12 (PB11). Leave the panel's own 3V3 pin and SD_CS empty.

   You should see: Thirteen wires run from the panel to the headers listed, with nothing on J1 pin 2 to 6 or the other pins.

   If the screen stays dark later, check VCC is on J1 pin 1 and not the panel's own 3V3 pin.

2. **Mac, terminal.** Download the six chip8-test-suite ROMs.

   ```text
   ports/chip8/tools/fetch_tests.sh
   ```

   You should see: A roms folder holds the six `.ch8` files.

   If a download fails, rerun the script.

3. **Mac, terminal.** Build starting on the Corax+ screen (index 2), then flash the padded binary with DSLite.

   ```text
   BOOT_ROM=2 BENCH=0 ports/chip8/build.sh
   ports/chip8/tools/flash_look.sh
   ```

   You should see: The panel shows the Corax+ screen with its check marks.

   If DSLite reports `Device Connection Error ... low power mode`, reset the probe with `xds110reset -s <serial> -a toggle -d 200` (it sits beside `xdsdfu`) and flash again.

### Expected result

The panel shows the Corax+ test screen with its check marks, a ROM bar and a 4 by 4 key pad.

### What this shows

- The interpreter runs 526k CHIP-8 instructions a second flat out at 80 MHz, 751 times a typical 700-a-second game pace.

### Limits

- The touch key pad was not tapped by a person; the keypad test ROM waits for that.
- Sound: the timer runs and nothing plays.
- Aspect ratio: the 64 by 32 screen is drawn at 5 times in both directions (320 by 160), so the 2 to 1 shape is kept. There is no stick; the key pad is touch, so the joystick-up check does not apply.
- The check read is the frame buffer, not the screen: a camera cannot tell a check mark from a cross at its resolution.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Size | 13.5 kB of flash and 10,652 of RAM | `ports/chip8/RESULTS-ROWS-wave5.csv` |
| Full redraw | 14.2 ms for 51,200 pixels at 40 MHz SPI | `ports/chip8/RESULTS-ROWS-wave5.csv` |

## ZX Spectrum 48K with BASIC and a touch keyboard

A Z80 core runs a ZX Spectrum 48K on the LP-MSPM0G3519 at about a fifth of real speed, with a 40-key touch keyboard under the screen.

Status: Works. Boards: g3519.

Folders this version uses (in a sparse clone, fetch them first, then read `ports/AGENTS.md`):

```bash
git sparse-checkout add ports mspm0g3519 tools mac-setup
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM0G3519 LaunchPad development kit | 1 | runs the game |
| Waveshare 3.5 inch capacitive touch LCD (ST7796S screen, FT6336U touch) | 1 | the screen and touch panel |

### Before you start

- A Mac with the MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS and SysConfig, and CCStudio 21.0.1 for its `DSLite` flasher (`mac-setup/README.md`).
- The code: `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git`, then `git sparse-checkout add ports mspm0g3519 tools mac-setup`. Every command on this page runs from the repository folder.
- The board ships with a demo in its flash. To keep it, back it up first with the command in the board's `AGENTS.md`.

### Steps

1. **Bench.** Wire the Waveshare 3.5 inch panel with 13 jumper wires, at 3.3 V only. Power: VCC to J1 pin 1 (3V3), GND to J2 pin 20 (GND). Screen: SCLK to J1 pin 7 (PB31), MOSI to J2 pin 15 (PB8), MISO to J2 pin 14 (PB7, optional), CS to J2 pin 13 (PB12), DC to J4 pin 39 (PB9), BL to J4 pin 38 (PC4), RST to J4 pin 37 (PC5). Touch: SDA to J1 pin 10 (PC3), SCL to J1 pin 9 (PC2), INT to J2 pin 11 (PB14, optional), TP_RST to J2 pin 12 (PB11). Leave the panel's own 3V3 pin and SD_CS empty.

   You should see: Thirteen wires run from the panel to the headers listed, with nothing on J1 pin 2 to 6 or the other pins.

   If the screen stays dark later, check VCC is on J1 pin 1 and not the panel's own 3V3 pin.

2. **Mac, terminal.** Build with the Z80 core in SRAM. Without `ZX_ROM` the build makes the ROM-less test image.

   ```text
   ZX_ROM=/path/to/rom RAMFUNC=1 ports/zx-spectrum/build.sh
   ```

   You should see: The script prints the build folder and `zx-g3519.bin`.

   If the link fails on a missing ROM path, build once without `ZX_ROM` to confirm the toolchain, then add the path.

3. **Mac, terminal.** Flash the binary with DSLite.

   ```text
   ports/chip8/tools/flash_look.sh <build folder>/zx-g3519.bin
   ```

   You should see: The panel shows the Spectrum screen; with a ROM, the copyright line and a cursor.

   If DSLite reports `Device Connection Error ... low power mode`, reset the probe with `xds110reset -s <serial> -a toggle -d 200` (it sits beside `xdsdfu`) and flash again.

### Expected result

The panel shows the Spectrum screen at 256 by 192 with a keyboard below. With a ROM you supply, the BASIC prompt appears.

### What this shows

- The machine runs at 19.4 percent of a real Spectrum at the BASIC prompt with the Z80 core copied into SRAM, 19.0 percent with the screen drawn.

### Limits

- Nobody has typed on the touch keyboard: that check waits for a person.
- The 16 kB system ROM is copyrighted; you supply it. Without one the build runs a ROM-less test program with no Sinclair code.
- Aspect ratio: the 256 by 192 screen is drawn 1 to 1, so the 4 to 3 shape is kept. There is no stick; keys are touch, so the joystick-up check does not apply.
- Speed is a fifth of a real machine, and code in flash runs slower: 15.9 percent.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Size | 62.5 kB of flash and 62,489 of RAM (the 48 kB Spectrum RAM included) | `ports/zx-spectrum/RESULTS-ROWS-wave5.csv` |
| Speed | 19.4 percent of a real Spectrum at the BASIC prompt, core in SRAM | `ports/zx-spectrum/RESULTS-ROWS-wave5.csv` |
