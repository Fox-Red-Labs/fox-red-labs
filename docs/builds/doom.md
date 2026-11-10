# Doom on a TI board

The Doom engine runs on a microcontroller kit, and on the AM62L Linux board it draws straight to the HDMI screen. Shareware data only; you supply the game file.

The page: [site/project-doom.html](../../site/project-doom.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## A. On a microcontroller kit (LP-EM-CC35X1, LP-MSPM33C321A)

A Cortex-M33 with 256 to 512 kB of RAM plays the three shareware demos. Build and flash from a Mac.

Status: Works. Boards: mspm33c321a, cc35x1.

Folders this version uses (in a sparse clone, fetch them first, then read `ports/doom/AGENTS.md`):

```bash
git sparse-checkout add ports/doom third-party tools mac-setup cc35x1
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-EM-CC35X1 LaunchPad development kit | 1 | plays the demos with no screen; the console prints frame checksums and the rate |
| LP-XDS110ET debugger | 1 | programs the LP-EM-CC35X1 |
| LP-MSPM33C321A LaunchPad development kit | 1 | plays the demos on a screen; its 16 MB QSPI flash holds the game data |
| BOOSTXL-EDUMKII Educational BoosterPack MKII | 1 | the 128 by 128 screen the M33 draws on |

### Before you start

- A Mac with TI Arm Clang 5.1.1.LTS, the SimpleLink Wi-Fi SDK 10.20.00.39 and the Wi-Fi Toolbox (CC35X1 path), or the MSPM33 SDK 1.04.00.00 with SysConfig 1.28.0 (M33 path). Versions are in `mac-setup/README.md`.
- The game data. The shareware DOOM1.WAD is downloaded by `fetch.sh` and checked by SHA-1; this repository never holds it. The engine is GPL-2.0 (`ports/doom/LICENSING.md`).
- The code: `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git`, then `git sparse-checkout add ports/doom third-party tools mac-setup cc35x1`.
- The CC35X1 flash holds the board's demo or another image. Back it up first with the command in `cc35x1/AGENTS.md`; the restore is a full flash erase.

### Steps

1. **Mac, terminal.** Fetch the engine at its pinned commit, patch it, download the shareware data file and convert it to the compact WHX format (1.7 MB).

   ```text
   cd ports/doom
   ./fetch.sh
   m33-whx/convert.sh
   ```

   You should see: `wad-cache/DOOM1.whx` exists. `fetch.sh` checked the data file against its SHA-1.

   If `fetch.sh` stops on a hash mismatch, delete `wad-cache/` and run it again; do not use a file it refused.

2. **Mac, terminal.** Build and sign the image. The converted data file is linked into it and read in place from the board's flash.

   ```text
   target-cc35x1-whx/build.sh
   ```

   You should see: The script prints a build folder and the signed Toolbox files under `ex/freertos/ticlang/toolbox/`.

   If the link fails on region size, the build used the dual-image layout; the script switches OTA off, so rerun it from a clean checkout.

3. **Mac, terminal.** Connect the XDS110 probe and program the signed build with the Wi-Fi Toolbox, then reset the board.

   ```text
   /Applications/ti/simplelink_wifi_toolbox_macos_4_3_23/simplelink-wifi-toolbox programmer -i XDS110 programming --tool_settings <build folder>/ex/freertos/ticlang/toolbox/tool_settings.json
   ```

   You should see: The programmer reports success for every region and the board resets.

   If the programmer fails with error -260 on every call, remove any BoosterPack and replug the probe cable.

4. **Mac, serial terminal.** Open the probe's serial port at 921,600 baud. The console prints a checksum line every 100 frames and a line at each demo end.

   You should see: `CRC n xxxxxxxx` lines, then `DEMOEND k frames=` three times (1,736, 3,092 and 3,881 frames), `EXIT`, `FPS after400` near 39 and `DONE`.

   If the port prints nothing, check the baud rate: the console is 921,600 here, not 115,200.

5. **Mac, terminal.** The M33 has 1 MB of flash, so the data file goes in the board's QSPI flash first (`ports/doom/m33-whx/E27-qspi-dump-and-load.md`). Then build the real-time screen image with the page cache and flash it with DSLite as in the other M33 pages.

   ```text
   EDU=1 BOARD=1 WHXCACHE=8 RT=1 NOCRC=1 WHX_CRC32=0xe9018e36 m33-whx/build.sh 30
   ```

   You should see: With the Educational BoosterPack on the LaunchPad the 128 by 128 screen shows the Doom title, then the first demo at about 22 frames a second.

   If the screen stays blank, the QSPI write did not finish: read the flash back (the dump firmware in `m33-whx/board/`) and compare it with `wad-cache/DOOM1.whx`.

### Expected result

The LP-EM-CC35X1 plays the three shareware demos to the end and prints the rate and frame checksums on its console. The LP-MSPM33C321A draws the game on the BoosterPack screen.

### What this shows

- The rp2040-doom design fits a 256 kB chip: 228 kB of static data and zone, against 164 kB static plus 800 kB zone for the plain engine.
- The CC35X1 plays three demos at 39.2 frames a second, with frame checksums equal to a Cortex-M33 emulator running the same image.
- The MSPM33C321A reads 1.7 MB of game data from its QSPI flash through a software page cache (98 to 99 percent hits) and draws in real time at 22.7 frames a second.

### Limits

- It has been played by hand with the BoosterPack's stick and buttons; the frame rate during hand play was not timed.
- The 3.5 inch touch panel image for the CC35X1 builds and links; it has not been run on a panel.
- Sound, and the other six levels of the shareware episode: only the three demo levels were played.
- The CC35X1 rate is on a stepped game clock, about three game tics per frame, so the demo runs faster than real time. It is not the same test as the M33's real-time 22.7.
- A 16 kB page cache on the M33 leaves 151 bytes of RAM free.
- The LP-MSPM0G3519 (128 kB of RAM) and LP-MSPM0G3507 (32 kB of RAM, 128 kB of flash) stop at static data alone.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Design | rp2040-doom: data kept compressed (WHX, 1.7 MB) and read in place; 16-bit pointers into a 256 kB window; a new renderer | `ports/doom/WRITEUP.md` |
| CC35X1 frame rate | 39.2 frames a second at 160 MHz, render files at -O2, statics in the 128 kB fast RAM | `data/results/cc35x1.csv` |
| M33 frame rate | 22.7 (8 kB cache) and 24.5 (16 kB cache), real time, on the screen | `data/results/mspm33c321a.csv` |
| QSPI read | 256 bytes in 23.8 microseconds | `data/results/mspm33c321a.csv` |

## B. On the AM62L Linux board, straight to the screen

The TMDS62LEVM draws Doom to the HDMI framebuffer with no GPU, 186 to 193 frames a second on the timing run, played with a game pad.

Status: Works. Boards: am62l.

Folders this version uses (in a sparse clone, fetch them first, then read `ports/doom/AGENTS.md`):

```bash
git sparse-checkout add ports/doom am62l-evm tools
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| TMDS62LEVM (AM62L evaluation module) | 1 | runs Linux and the game |
| PS4 DualShock 4 game pad on a USB cable | 1 | plays the game |

### Before you start

- The AM62L booted from TI's SDK 12.01 Linux image with a network connection (`am62l-evm/README.md`). The image has no compiler, so the build runs in a Debian trixie arm64 container on the board.
- The code: `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git`, then `git sparse-checkout add ports/doom am62l-evm tools`.
- The shareware DOOM1.WAD comes from `fetch.sh` on the Mac; the repository never holds it.

### Steps

1. **Mac, terminal.** Fetch the engine at its pinned commit, patch it and download the shareware data file.

   ```text
   cd ports/doom
   ./fetch.sh
   ```

   You should see: `src-upstream/doomgeneric/` and `wad-cache/DOOM1.WAD` exist, and the data file's SHA-1 was checked.

   If it stops on a hash mismatch, delete `wad-cache/` and run it again.

2. **Mac, terminal.** Copy `srclist.sh`, `linux-fb/`, `src-upstream/doomgeneric/` and `wad-cache/DOOM1.WAD` into `/root/doom` on the board, keeping their folder names.

   You should see: `/root/doom` on the board holds the four items.

   If the board does not answer on the network, check its address on the serial console first.

3. **Board, serial or ssh.** Build in the container on the board. It takes about 75 seconds on the two cores.

   ```text
   sh linux-fb/build.sh /root/doom
   ```

   You should see: `build/fb/doom_fb` exists.

   If the container cannot pull its image, the board has no route to the registry: fix the network, then rerun.

4. **Board, HDMI monitor.** Stop the display demo that owns the screen, then run the timing demo.

   ```text
   systemctl stop ti-lvgl-demo
   ./build/fb/doom_fb -iwad wad-cache/DOOM1.WAD -timedemo demo1 -nosound
   ```

   You should see: The monitor shows the game at 4:3 and the last line reports 186 to 193 frames a second.

   If the screen stays on the display demo, `ti-lvgl-demo` did not stop: run `systemctl stop ti-lvgl-demo` again.

5. **Board, HDMI monitor.** Plug the pad into the board and start the game. Stick up walks forward; Cross fires, Square uses, Circle runs, Options is the menu.

   ```text
   DOOM_FPS_EVERY=5 ./build/fb/doom_fb -iwad wad-cache/DOOM1.WAD -nosound
   ```

   You should see: The game takes the pad and prints its frame rate every five seconds.

   If the buttons do the wrong thing on a DualShock 4, the kernel has `hid-generic` on the pad; the game prints `hid-generic DualShock 4 button table` at start and uses it. Any other pad uses the standard names.

6. **Board, serial or ssh.** Stop the game and start the display demo again.

   ```text
   pkill doom_fb
   systemctl start ti-lvgl-demo
   ```

   You should see: The monitor shows the board's display demo again.

   If the screen stays black, start the display demo again.

### Expected result

The AM62L draws Doom on its HDMI output with no GPU, and a game pad plays it.

### What this shows

- A frame-buffer port reaches 186 to 193 frames a second at 1280 by 1000; the same engine through Weston and a software GL renderer reaches 6.5.
- The picture is 320 by 200 scaled by whole numbers to 1280 by 1000 and reads as 4:3 on the monitor.

### Limits

- The timing figure is the engine's own timing run, not the monitor's refresh rate: there is no vsync, so a fast turn can tear.
- Sound: the game runs with `-nosound`.
- The board reaches 413 frames a second unscaled and 241 at 960 by 800; the scale you pick sets the rate.
- 320 by 200 shown 4 by 5 times is 1280 by 1000, aspect 1.28 against the 1.33 of a 4:3 screen; it is the nearest whole-number pair that fits 1080 lines.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Timing demo | demo1: 185.8 to 192.7 frames a second at 1280 by 1000, 240.6 at 960 by 800, 412.9 at 320 by 200 | `data/results/am62l-evm.csv` |
| Build | GCC 14.2.0 -O2 static in a Debian trixie arm64 container, about 75 s on the board | `ports/doom/linux-fb/README.md` |
