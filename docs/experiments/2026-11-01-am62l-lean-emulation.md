<!-- experiment
id: E295
board: TMDS62LEVM (AM62L evaluation module)
date: 2026-11-01
status: works
reached: run
line: With no X server, no container and no OpenGL, a 300-line libretro frontend that draws straight onto the HDMI connector through DRM/KMS runs Snes9x 2005 at a locked 60 fps at every HDMI mode up to 1920x1080 (core 2.5 to 3.6 ms of the 16.7 ms frame, with audio and with a video thread), and PCSX-ReARMed at 60 fps on PlayStation test programs. The same Gambatte core that managed 48 fps and 10.4 fps in the showcase runs at 60 fps at 640x480 and at 1080p here. The display path was the problem, not the cores.
stop: none for the cores; only free homebrew and test programs were run, so commercial-sized loads are not measured; the plane cannot scale, so the CPU scales
-->
# Can the AM62L run SNES and PlayStation emulation at full speed on HDMI?

**Short answer.** Yes, for the free content that could be tested. Snes9x 2005 (a libretro core) holds 60 fps at 640x480, 800x600, 1280x720 and 1920x1080, with the core using 2.5 to 3.6 ms of each 16.7 ms frame. PCSX-ReARMed holds 60 fps on a PlayStation GPU test program at 3.6 ms a frame. Audio through ALSA, a second video thread and the largest HDMI mode each cost under 1.5 ms more. What made the difference from the showcase's 48 fps is the display path: the same Gambatte Game Boy core runs at 60 fps here at 640x480 and at 1920x1080, where RetroArch under Xorg in a container gave 48 fps and 10.4 fps.

Terms: [vocab](../vocab.md). **libretro** is the plug-in interface emulator cores share; a core is one `.so` file. **DRM/KMS** is the Linux display interface that writes to the screen without X. A **dumb buffer** is plain memory the display controller scans out. **HLE BIOS** is PCSX-ReARMed's built-in replacement for Sony's boot code, so no BIOS file is needed. **Vblank** is the display's 60 Hz refresh tick. Code and logs: `am62l-evm/lean/`; rows in `data/results/am62l-evm.csv`.

**Question.** The earlier HDMI showcase ran a Game Boy core at 48 fps. A Game Boy is far lighter than an SNES, so was that the CPU, or the way the picture reached the screen? And does the chip (two Cortex-A53 at 1.25 GHz, no GPU) hold SNES and PlayStation at full speed when nothing sits between the emulator and the display?

**Expectation.** Written before the runs: Gambatte at 60 fps in any mode once X and the container are gone; SNES homebrew at 60 fps with a core time under 8 ms; PlayStation uncertain, because its GPU is emulated in software too.

**Setup.** TI SDK 12.01 Linux (kernel 6.18.38-ti-kvm), both cores at 1.25 GHz, `performance` governor, 2 GB RAM, HDMI-A-1 connected, nothing else running (the showcase loop and containers stopped). Everything runs on the board, as root, from a text console.

- Frontend: `am62l-evm/lean/lean_fe.c`. It loads a core with `dlopen`, calls `retro_run` once per frame, copies the frame to a dumb buffer, and flips on vblank. Integer nearest-neighbor scaling in software (2x at 640x480, 3x at 1280x720, 4x at 1080p for a 256x224 SNES picture), 16-bit RGB565 throughout. Options: `-t` video thread, `-a` audio through ALSA `plughw:0,0`, `-n` no vblank wait (free run).
- Cores built on the board with GCC 15.3: Snes9x 2005 (libretro, commit a79dfe9, about 3 minutes), PCSX-ReARMed r26 (commit 7b0e8f8, ARM64 dynarec and NEON GPU, about 19 minutes). Gambatte is the Debian core copied out of the showcase container.
- Measurement: 2 s warm-up, then 10 s, three runs per row, median. Frame time is the core's `retro_run` alone, timed with the monotonic clock. A vblank is missed when two flips are more than 1.5 frames apart. CPU use is the share of each core's time that was not idle, from `/proc/stat`.
- Content (all free to redistribute; no commercial title, no Sony BIOS):

| File | What it is | Source and license |
|---|---|---|
| `Mode7LZ77.sfc`, `Transparency.sfc`, `effectsandmusic.sfc` (plus `Mode1ContinuousScroll`, `DynamicEngineMetaSprite`, `breakout`, `music` for the free-run table) | PVSnesLib example programs: a rotating Mode 7 plane, full-screen transparency, sprites with sound effects and music | PVSnesLib 4.6.0 release (`github.com/alekmaul/pvsneslib`), MIT license as stated by the repository; per-asset credits not audited. sha256 (first 12 hex) of the three main files: `04ccdd4d7043`, `9e641a051f40`, `e10465545dff` |
| `benchmark.exe`, `animated-triangle.exe` | PlayStation test programs: flat triangles drawn by the GPU, and one rotating triangle | `github.com/JaCzekanski/ps1-tests`, release build-158, MIT. sha256 (first 12 hex) `372255c4d617` and `9b30c615dfd1` |
| `tobu.gb` | Tobu Tobu Girl, a Game Boy game (control run) | gbdev database; code MIT, assets CC BY 4.0, Tangram Games. sha256 (first 12 hex) `5d3871cae77d` |

**Result.** Medians of three 10 s runs, `scale` is the software scaling and copy per frame. Raw lines: `am62l-evm/lean/matrix-2026-11-01.txt`.

| Core and content | HDMI mode (scale) | Options | fps | Core ms per frame (p95) | Scale ms | Vblanks missed | CPU, core 0 / core 1 |
|---|---|---|--:|--:|--:|--:|--|
| Snes9x 2005, Mode 7 | 640x480 (2x) | | 60.00 | 2.48 (2.66) | 0.46 | 0 of 600 | 12 / 20 percent |
| Same | 800x600 (2x) | | 60.32 | 2.56 (2.73) | 0.38 | 0 of 604 | 10 / 20 |
| Same | 1280x720 (3x) | | 60.00 | 2.56 (2.72) | 1.01 | 0 of 600 | 12 / 23 |
| Same | 1920x1080 (4x) | | 60.00 | 2.58 (2.76) | 1.66 | 0 of 600 | 12 / 29 |
| Same | 1920x1080 (4x) | video thread | 60.00 | 3.11 (3.36) | 1.60 | 0 of 600 | 31 / 22 |
| Snes9x 2005, full-screen transparency | 640x480 (2x) | | 60.00 | 3.39 (3.68) | 0.48 | 0 of 600 | 13 / 24 |
| Same | 1920x1080 (4x) | | 60.00 | 3.55 (3.86) | 1.52 | 1 of 599 (one run) | 13 / 32 |
| Snes9x 2005, sound effects and music | 640x480 (2x) | audio 32,040 Hz | 60.00 | 0.73 (0.84) | 0.35 | 0 of 600 | 15 / 10 |
| Same | 1920x1080 (4x) | audio, video thread | 60.00 | 1.11 (1.46) | 1.60 | 0 of 600 | 26 / 15 |
| PCSX-ReARMed, GPU benchmark (100 flat triangles) | 640x480 (2x) | | 60.00 | 3.61 (3.89) | 0.39 | 0 of 600 | 12 / 28 |
| Same | 1280x720 (3x) | | 60.00 | 3.70 (4.08) | 1.30 | 0 of 600 | 14 / 31 |
| Same | 1920x1080 (4x) | | 60.00 | 3.86 (4.15) | 1.88 | 0 of 600 | 14 / 35 |
| Same | 1920x1080 (4x) | video thread | 60.00 | 5.15 (5.55) | 2.16 | 0 of 600 | 38 / 28 |
| Same | 640x480 (2x) | audio 44,100 Hz | 60.00 | 3.65 (3.94) | 0.39 | 0 of 600 | 13 / 26 |
| Gambatte, Tobu Tobu Girl (control) | 640x480 (3x) | | 60.00 | 0.87 (0.98) | 0.35 | 0 of 600 | 14 / 11 |
| Same | 1920x1080 (7x) | | 60.00 | 0.87 (0.99) | 1.30 | 0 of 600 | 16 / 18 |

Audio ran clean: 0 underruns and 0 dropped samples in every run, and ALSA took 384,670 frames in 12 s at 32,040 Hz (SNES) and 531,250 at 44,100 Hz (PlayStation), so the stream was real-time. Largest single core frame in any vblank-locked run: 14.1 ms (Mode 7, 1080p), still inside the 16.7 ms budget.

Headroom, from free-run (`-n`, no vblank wait): the heaviest SNES program here (transparency) runs 272 fps at 640x480 and 194 fps at 1080p including scaling. The other free-run readings, one 6 s run each at 640x480: Mode 7 354 fps, Mode 1 scrolling 540, sprites 1,117, breakout 633, music 846, effects and music 1,053. The PlayStation benchmark free-runs at 250 fps (three runs); the rotating-triangle program takes 1.0 ms a frame.

**What made the difference from the 48 fps showcase.** The control: Gambatte core, same board, same monitor, same 640x480 mode. Showcase (RetroArch under Xorg `modesetting` with `AccelMethod none`, Mesa llvmpipe doing the scaling, in a container): 48 fps at 640x480 and 10.4 fps at 1080p. Lean frontend: 0.87 ms of core time and 60 fps at both modes. The core never was the cost. The cost was a software OpenGL scaler on two A53 cores plus X, and it grows with the screen: 1080p is 6.75 times the pixels of 640x480, and the showcase frame rate fell 4.6 times between the two. Writing integer-scaled lines straight into the scan-out buffer costs 0.35 ms at 640x480 and 1.3 to 1.9 ms at 1080p.

**Does the display controller scale?** No. The AM62L's tidss driver exposes one plane (id 35, primary, RGB565, XRGB8888 and YUV formats, no scaling property). `drmModeSetPlane` with a 256x224 source on a 1:1 destination works (60 fps, 0.2 ms of copying per frame); every destination larger than the source (2x, 3x, 4x) returns `EINVAL`. So hardware scaling is not an option on this chip, and the CPU does the scaling. It is cheap at integer ratios, using one `memcpy` of a prepared line per output line. Writing into a dumb buffer is write-combined memory, so the frontend never reads back from it.

**Things that went wrong, so they do not cost the next person time.**

- Snes9x 2010 did not build in a reasonable time: GCC 15.3 spent over 14 minutes and 800 MB on `src/tile.c` at `-O2` and the build was stopped. Snes9x 2005 builds in about 3 minutes and was enough.
- The first matrix run of the 1080p free-run row printed no result line (cause not found); three reruns gave 192 to 194 fps and are the ones quoted.
- Controller input to the PlayStation core did not register in three tries (port device set, bitmask polling added, button pulses), so the GPU benchmark stayed at its default of 100 flat triangles instead of being pushed up. The result is a light PlayStation load.
- The lab camera feed was down, so the HDMI panel was not photographed. What backs "on HDMI": the connector reports connected, every flip returned a vblank event at 60.00 Hz, and a frame dumped from the core shows the Mode 7 scene drawn correctly (a rotating Mode 7 plane). Nobody has looked at the monitor.

**What it does not show.**

- Commercial games. Only free homebrew and test programs were run, and they may be lighter than a typical cartridge or disc. The heaviest SNES program used 3.6 ms of 16.7 ms, so a commercial title would need to cost about 4.6 times more per frame before it drops below 60 fps. That is arithmetic on the measured number, not a run. A title with sprites, HDMA effects, a coprocessor and music all at once can cost more than any program here.
- PlayStation games. The 100-triangle test does not stress the software GPU (fill rate and texture mapping dominate it in a real game). Whether 3D PlayStation games hold 60 fps on two A53 cores is open; the core's built-in HLE BIOS runs test programs but was not tried on a disc image (no free disc image was on hand).
- Sound out of the speaker. The audio stream was accepted by ALSA but nobody listened at the codec. (Input was added the same day: see "Play on the TV" below. The benchmark rows above still used scripted button pulses.)
- Three runs per row from one board; no second builder.

**Verdict.** SNES: full speed on this chip, at every HDMI mode the monitor offers, with audio and with a video thread; 4.6 times headroom on the heaviest free program. PlayStation: full speed on the free test programs; the chip's capability for commercial titles is not measured. Both need a frontend that writes to DRM directly: RetroArch on X or a container is the wrong stack for this chip.

**If you try it.** Build the three pieces on the board, then run:

```
gcc -O3 -march=native -o lean_fe lean_fe.c -I<folder with libretro.h> -I/usr/include/libdrm -ldl -ldrm -lasound -lpthread -lm
./lean_fe -c snes9x2005_libretro.so -g Mode7LZ77.sfc -m 1920x1080 -d 30 -p
./lean_fe -c pcsx_rearmed_libretro.so -g benchmark.exe -m 640x480 -d 30 -a
```

If `lean_fe` prints `no 640x480 mode`, the connector has no such mode: `modetest -M tidss` lists the EDID modes. If it prints `card0: Permission denied` or the picture does not change, stop the showcase loop and `ti-lvgl-demo` first (`sh am62l-evm/showcase/run.sh stop`, `systemctl stop ti-lvgl-demo`); one program owns the display at a time. If `SetPlane ... failed: Invalid argument` appears with `-s hw`, that is the missing plane scaler, use the default `-s sw`. If a build of Snes9x 2010 sits on `tile.c` for many minutes, use Snes9x 2005.

## Play on the TV: gamepad, game picker, boot service (added 2026-11-01)

`lean_fe` now reads gamepads from `/dev/input/event*` and has a picker (`-M /root/games`) that lists the games in big text and starts the chosen one with the right core. Usage, mapping, pads and how to add games: [AM62L EVM README, "Play on the TV"](../../am62l-evm/README.md). `am62l-evm/lean/frl-lean.service` starts the picker at boot and replaces the `ti-lvgl-demo` screen.

- **Pad on the bench:** Sony DualShock 4 over USB (`054c:05c4`, `/dev/input/event1`; its motion sensors and touchpad are separate event devices and are skipped). Other pads are mapped by the standard Linux gamepad button codes (Xbox, PlayStation, 8BitDo) and by the usual generic USB SNES-pad layout; none of those were plugged in.
- **Mapping:** libretro positions, so the bottom face button is B and the right one A on every pad. D-pad hat, d-pad buttons and the left stick all work; the stick's dead zone is 30 percent of its travel. Select and Start held together for 2 seconds end the game and return to the picker.
- **Input latency** (`emu_input_ms`, `data/results/am62l-evm.csv`): kernel timestamp of a button event to the vblank that shows the frame drawn after it. Snes9x 2005 running breakout at 1080p with audio: mean 31.9 ms over 121 presses, fastest 16.6 ms, slowest 155.9 ms (a rare stall, cause not found). The presses came from a virtual pad made with uinput, not from a hand on the DualShock, so a real pad adds its USB poll (4 ms at its default 250 Hz). The TV's own delay is not included. The input is read once at the start of each frame, which puts the floor at one frame (16.7 ms) and the average near one and a half; reading it later in the frame, just before the core runs, would cut about 10 ms and was not tried.
- **Games that ship in the picker** (all free to redistribute, sources and sha256 in `am62l-evm/lean/games-sources.tsv`): the seven PVSnesLib examples and two ps1-tests programs used above, and fifteen Game Boy homebrew games from the gbdev database that list an open-source license (MIT or GPL-3.0-or-later): Tobu Tobu Girl and its Deluxe version, Max Pirate and Max Pirate Extra Boom, PlantBoy, Rex Runner, JP, Carazu, Geometrix, Big2Small, Rebound, Renegade Rush, uCity, PostBot, CrossConnect, Tuff. No commercial game and no BIOS is in the repo or on the board. The SNES list is demos plus one breakout clone: no free SNES homebrew game with a clear license was added (the search was cut short).
- **Not checked:** nobody has yet looked at the TV or played on it (the picker started without errors and the connector reported connected); PlayStation input (the earlier scripted presses did not register either); save files are not written (`retro_get_memory_data` is not used), so games that save lose progress when the picker restarts them.
