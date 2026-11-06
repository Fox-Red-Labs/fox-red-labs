# cc35x1: Doom on the LP-EM-CC35X1, three shareware demos

**What it is.** The Doom engine, in the rp2040-doom design, on a Cortex-M33 at 160 MHz with no screen; the build uses 260 kB of RAM. The console prints frame checksums and the frame rate. Build steps: [docs/builds/doom.md](https://github.com/Fox-Red-Labs/fox-red-labs/blob/main/docs/builds/doom.md). The build page status is Works.

**What it runs on.** `cc35x1` (LP-EM-CC35X1), TI Arm Clang 5.1.1.LTS, SimpleLink Wi-Fi SDK 10.20.00.39, built and flashed from a Mac.

**Shown by.** All three demos finish: 1,736, 3,092 and 3,881 frames. The frame checksums equal the Cortex-M33 emulator's run of the same engine, and the rate is 39.2 frames per second at 160 MHz after frame 400. That rate is on a stepped game clock, which runs faster than real time, so it is not the same test as the real-time 22.7 frames per second on the LP-MSPM33C321A. Log and method: [ports/doom/target-cc35x1-whx/README.md](https://github.com/Fox-Red-Labs/fox-red-labs/blob/main/ports/doom/target-cc35x1-whx/README.md).

**What did not work.** The stock engine needs at least 593 kB of zone to start and 800 kB to finish all three demos; the stock engine was given 480 kB on this board, so it stops on zone memory in all three demos. The first placement of the memory ran at 22.5 frames per second; moving the render code to fast RAM and building it with -O2 gave 39.2. The LP-MSPM0G3507 (32 kB of RAM) does not fit it at all. The game data is the shareware file, which you download yourself.

Built on the lab's bench, not by a reader. If you repeat it, reply with your board, version and result.
