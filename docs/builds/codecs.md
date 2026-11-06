# Audio and video codecs on a LaunchPad

Pick a codec, build it for the board, flash it and compare every frame with a Mac build. LC3, Opus, MP3 and Plaits run on the LP-MSPM33C321A, and H.264 on it and the LP-AM13E230.

The page: [site/project-codecs.html](../../site/project-codecs.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## LC3 (LE Audio) on the LP-MSPM33C321A

Build the Mac reference and the board image, then flash and compare every frame.

Status: Works. Boards: mspm33c321a.

Folders this version uses (in a sparse clone, fetch them first, then read `ports/lc3/AGENTS.md`):

```bash
git sparse-checkout add ports/lc3 ports/llama2c mspm33c321a tools mac-setup
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM33C321A LaunchPad development kit | 1 | runs the codec and times it with its cycle counter |

### Before you start

- A Mac with the MSPM33 SDK 1.04.00.00, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0 and CCStudio 21.0.1 in the default `/Applications/ti` folders (`mac-setup/README.md`), and a C compiler for the Mac reference.
- The code: `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git`, then `git sparse-checkout add ports portability mspm33c321a tools mac-setup`. Every command on this page runs from the repository folder.
- Run `cd ports/llama2c && PYTHON=python3 ./build.sh O2` once from the repository folder; the codec builds reuse its `gen160` SysConfig output (point `G` at it if a build cannot find it).
- The runner flashes the board and leaves the benchmark image on it. To keep what is on the board, back it up first with the command in `mspm33c321a/AGENTS.md`.

### Steps

1. **Mac, terminal.** Build the strict and fast-math references with a per-frame CRC for each frame.

   ```text
   cd ports/lc3
   host/build.sh
   ```

   You should see: The Mac references are built under `ports/lc3/results/host`.

   If the compiler is missing, install the Xcode command line tools and run it again.

2. **Mac, terminal.** Build at -O2 with the float unit on and strict arithmetic. Nothing is flashed.

   ```text
   target-m33/build.sh O2 hard strict
   ```

   You should see: The script ends with `built .../lc3.out` and a size line.

   If it cannot find the SysConfig output, set `G` to the `gen160` folder from the prerequisites.

3. **Mac, terminal.** Flash the image, wait for the finished flag, read the results back over the probe and compare every checksum with the Mac reference. It prints cycles, the share of real time and the stack use. It plays no sound and drives no pin.

   ```text
   cd ../..
   PROBE=<probe serial> python3 ports/lc3/run_board.py <path of lc3.out>
   ```

   You should see: The CRCs match and the printed shares are about 12, 16 and 24 percent for encoding at 16, 24 and 48 kHz.

   If the probe is not found, pass the serial of your probe that `xdsdfu -e` lists.

### Expected result

The board encodes and decodes LC3 frames with every checksum equal to the Mac's.

### What this shows

- LE Audio's codec runs in real time on a Cortex-M33 at 160 MHz with room left: the heaviest encode is 24.2 percent of a 10 ms frame.
- The output is bit-exact against the Mac build in all three configurations, and the 7.5, 5 and 2.5 ms frames, packet-loss concealment and two channels also match in 9 of 9 runs.

### Limits

- Codec only: a generated test signal, no Bluetooth link and no sound output.
- A Cortex-M0+ at 80 MHz is modelled in an emulator, not run: it needs about 329 percent of real time for the 16 kHz encode.
- Build size is 157.8 kB of flash with every feature and 83.2 kB with LC3 only at -Os.
- A fast-math build makes different encoder bytes from the strict build; both are valid LC3 but not byte-identical.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Encode, 16 / 24 / 48 kHz | 198k / 249k / 388k cycles a frame |  |
| Decode, 16 / 24 / 48 kHz | 73k / 114k / 168k cycles a frame |  |

## Opus voice and music on the LP-MSPM33C321A

Build the Mac references and the fixed-point board image, then flash and compare.

Status: Works. Boards: mspm33c321a.

Folders this version uses (in a sparse clone, fetch them first, then read `ports/opus/AGENTS.md`):

```bash
git sparse-checkout add ports/opus ports/llama2c mspm33c321a tools mac-setup
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM33C321A LaunchPad development kit | 1 | runs the codec and times it with its cycle counter |

### Before you start

- A Mac with the MSPM33 SDK 1.04.00.00, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0 and CCStudio 21.0.1 in the default `/Applications/ti` folders (`mac-setup/README.md`), and a C compiler for the Mac reference.
- The code: `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git`, then `git sparse-checkout add ports portability mspm33c321a tools mac-setup`. Every command on this page runs from the repository folder.
- Run `cd ports/llama2c && PYTHON=python3 ./build.sh O2` once from the repository folder; the codec builds reuse its `gen160` SysConfig output (point `G` at it if a build cannot find it).
- The runner flashes the board and leaves the benchmark image on it. To keep what is on the board, back it up first with the command in `mspm33c321a/AGENTS.md`.

### Steps

1. **Mac, terminal.** Build the stock and the 32-bit-macro references with a CRC for each frame.

   ```text
   cd ports/opus
   INT64=0 host/build.sh
   ```

   You should see: The references are built under `ports/opus/results/host`.

   If the compiler is missing, install the Xcode command line tools and run it again.

2. **Mac, terminal.** Build at -Os. Nothing is flashed.

   ```text
   target-m33/build.sh fixed Os
   ```

   You should see: The script ends with `built .../opus.out` and a size line.

   If it cannot find the SysConfig output, set `G` to the `gen160` folder from the prerequisites.

3. **Mac, terminal.** Flash the image, wait for the finished flag, read the results back over the probe and compare every checksum with the Mac reference. It prints cycles and the share of real time for each configuration. It plays no sound and drives no pin.

   ```text
   cd ../..
   PROBE=<probe serial> python3 ports/opus/run_board.py <path of opus.out> fixed --ref fixed32
   ```

   You should see: Every packet and decoded block CRC matches the 32-bit-macro reference.

   If the probe is not found, pass the serial of your probe that `xdsdfu -e` lists.

### Expected result

The board encodes and decodes Opus voice and music frames with every checksum equal to the Mac's.

### What this shows

- CELT at 48 kHz encodes in 29 to 54 percent and decodes in 17 to 22 percent of real time at 160 MHz; SILK decodes in 5 percent.
- The fixed-point build is bit-exact against the Mac in 12 of 12 runs.

### Limits

- Mono, constant bit rate, a generated signal; no radio and no sound output.
- The SILK encoder keeps up only at complexity 0 (56 percent); at complexity 5 it needs 2.5 times the core.
- Flash is 331.4 kB at -O2 and 164.7 kB at -Os.
- A Mac build with its default 64-bit multiply macros differs from CELT complexity 5 and up.

### Numbers and sources

| | Value | Source |
|---|---|---|
| CELT 48 kHz 10 ms, complexity 5 | 858k cycles to encode, 358,367 to decode | `ports/opus/README.md` |
| SILK 16 kHz 20 ms, complexity 0 | 1.79M cycles to encode, 155,615 to decode | `ports/opus/README.md` |

## MP3 decoding (minimp3) on two LaunchPad kits

Decode a 5-second MP3 on the Cortex-M33 and on the Cortex-M0+ and compare every frame with the Mac.

Status: Works. Boards: mspm33c321a, g3519.

Folders this version uses (in a sparse clone, fetch them first, then read `ports/minimp3/AGENTS.md`):

```bash
git sparse-checkout add ports/minimp3 ports/llama2c mspm33c321a mspm0g3519 tools mac-setup
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM33C321A LaunchPad development kit | 1 | runs the codec and times it with its cycle counter |
| LP-MSPM0G3519 LaunchPad development kit | 1 | the Cortex-M0+ run, with no float unit |

### Before you start

- A Mac with the MSPM33 SDK 1.04.00.00, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0 and CCStudio 21.0.1 in the default `/Applications/ti` folders (`mac-setup/README.md`), and a C compiler for the Mac reference.
- The code: `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git`, then `git sparse-checkout add ports portability mspm33c321a tools mac-setup`. Every command on this page runs from the repository folder.
- Run `cd ports/llama2c && PYTHON=python3 ./build.sh O2` once from the repository folder; the codec builds reuse its `gen160` SysConfig output (point `G` at it if a build cannot find it).
- The runner flashes the board and leaves the benchmark image on it. To keep what is on the board, back it up first with the command in `mspm33c321a/AGENTS.md`.
- The Cortex-M0+ build uses `arm-none-eabi-gcc` and reads the board through DSLite while the core runs; an OpenOCD connect resets this board through the XDS110 and clears the results.

### Steps

1. **Mac, terminal.** Build the references, decode both clips and compare them with the SSE build and ffmpeg.

   ```text
   cd ports/minimp3
   tests/check_host.sh
   ```

   You should see: The check ends with the clips matching.

   If `ffmpeg` is missing, install it; it is needed only to remake the clips.

2. **Mac, terminal.** Build at -O2 with the hardware float unit. Nothing is flashed.

   ```text
   target-m33/build.sh O2 hard
   ```

   You should see: The script ends with `built .../mp3.out`; flash is 153.4 kB.

   If it cannot find the SysConfig output, set `G` to the `gen160` folder from the prerequisites.

3. **Mac, terminal.** Flash the image, wait for the finished flag, read the results back over the probe and compare every checksum with the Mac reference. The stereo clip reads 0.151 and the mono clip 0.109 times real time. It plays no sound and drives no pin.

   ```text
   cd ../..
   PROBE=<probe serial> python3 ports/minimp3/run_board.py <path of mp3.out>
   ```

   You should see: Every frame checksum matches, 194 of 194 per clip.

   If the probe is not found, pass the serial of your probe that `xdsdfu -e` lists.

4. **Mac, terminal.** Build the soft-float image with `arm-none-eabi-gcc`, then flash the LP-MSPM0G3519, wait and compare. It takes about 2 minutes.

   ```text
   PATH=<gnu arm bin>:$PATH OUT=<dir> ports/minimp3/target-g3519/build.sh -O2
   PROBE=<probe serial> python3 ports/minimp3/run_board_g3519.py <dir>/mp3.elf
   ```

   You should see: The stereo clip reads 9.84 and the mono clip 7.38 times real time, with every frame checksum equal.

   If the results read zero, an OpenOCD connect reset the board; read it with DSLite instead.

### Expected result

Both boards decode the clip with every frame checksum equal to the Mac's; the M33 keeps up in real time and the M0+ does not.

### What this shows

- A Cortex-M33 at 160 MHz decodes stereo MP3 in 15 percent of real time with the hardware float unit.
- The Cortex-M0+ at 80 MHz, with no float unit, needs about 787 MHz to keep up with stereo.

### Limits

- Decode only: no audio is played.
- minimp3 has no fixed-point path, so there is no fixed-point row.
- The M0+ image is 150.3 kB of flash and 34.1 kB of static RAM plus a 32 kB stack.
- The M0+ timer is SysTick plus an overflow count; a bare SysTick read would understate each frame.

### Numbers and sources

| | Value | Source |
|---|---|---|
| LP-MSPM33C321A | 0.151 (stereo) and 0.109 (mono) times real time |  |
| LP-MSPM0G3519 | 9.84 (stereo) and 7.38 (mono) times real time |  |

## Plaits synthesis (24 engines) on the LP-MSPM33C321A

Render all 24 synth engines on the board and compare every block with the Mac.

Status: Works. Boards: mspm33c321a.

Folders this version uses (in a sparse clone, fetch them first, then read `ports/plaits/AGENTS.md`):

```bash
git sparse-checkout add ports/plaits ports/llama2c mspm33c321a tools mac-setup
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM33C321A LaunchPad development kit | 1 | runs the codec and times it with its cycle counter |

### Before you start

- A Mac with the MSPM33 SDK 1.04.00.00, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0 and CCStudio 21.0.1 in the default `/Applications/ti` folders (`mac-setup/README.md`), and a C compiler for the Mac reference.
- The code: `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git`, then `git sparse-checkout add ports portability mspm33c321a tools mac-setup`. Every command on this page runs from the repository folder.
- Run `cd ports/llama2c && PYTHON=python3 ./build.sh O2` once from the repository folder; the codec builds reuse its `gen160` SysConfig output (point `G` at it if a build cannot find it).
- The runner flashes the board and leaves the benchmark image on it. To keep what is on the board, back it up first with the command in `mspm33c321a/AGENTS.md`.
- `clang++` for the Mac reference.

### Steps

1. **Mac, terminal.** Build the reference and write its per-block CRCs.

   ```text
   cd ports/plaits
   host/build.sh /tmp/plaits-host && /tmp/plaits-host/ref v > /tmp/plaits-host/ref.txt
   ```

   You should see: `/tmp/plaits-host/ref.txt` lists a CRC for each block.

   If `clang++` is missing, install the Xcode command line tools.

2. **Mac, terminal.** Build at -O2 with the float unit on. Nothing is flashed.

   ```text
   target-m33/build.sh O2 hard
   ```

   You should see: The script ends with `built .../plaits.out`; flash is 202.2 kB.

   If it cannot find the SysConfig output, set `G` to the `gen160` folder from the prerequisites.

3. **Mac, terminal.** Flash the image, wait for the finished flag, read the results back over the probe and compare every checksum with the Mac reference. It prints the cycles per block for each engine. It plays no sound and drives no pin.

   ```text
   cd ../..
   PROBE=<probe serial> python3 ports/plaits/run_board.py <path of plaits.out> --ref /tmp/plaits-host/ref.txt
   ```

   You should see: 24 of 24 engines match the Mac, 23 of them under 37 percent of real time in every block.

   If the probe is not found, pass the serial of your probe that `xdsdfu -e` lists.

### Expected result

All 24 engines render with every block checksum equal to the Mac's.

### What this shows

- A full synthesizer voice costs about a quarter of the core: 23 of 24 engines stay under 37 percent of real time in every block.
- The output is bit-exact against the Mac in all 24 engines.

### Limits

- Synthesis only: the sound goes to a RAM buffer, with no DAC, MIDI or listening test. The Audio BoosterPack page plays a tone and a tune through a DAC.
- The speech engine has three blocks over real time, where the harmonics jump.
- One voice and one fixed note sequence.
- Flash is 202.2 kB at -O2. Switching to six of the engines costs one slow block, so switch while the output is muted.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Block cost | 4.3k cycles (chiptune) to 13.8k (modal) of a 40k-cycle block | `ports/plaits/README.md` |

## H.264 video decoding on two LaunchPad kits

Decode small H.264 streams on the LP-MSPM33C321A and the LP-AM13E230 and compare the frames with the Mac.

Status: Works. Boards: mspm33c321a, am13e230.

Folders this version uses (in a sparse clone, fetch them first, then read `portability/h264/AGENTS.md`):

```bash
git sparse-checkout add portability/h264 mspm33c321a am13e230 tools mac-setup
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM33C321A LaunchPad development kit | 1 | runs the codec and times it with its cycle counter |
| LP-AM13E230 LaunchPad development kit | 1 | the second board |

### Before you start

- A Mac with TI Arm Clang 5.1.1.LTS, CCStudio 21.0.1 for `DSLite`, the MSPM33 SDK 1.04.00.00 and the AM13E2x SDK 26.01.00.03 in the default `/Applications/ti` folders (`mac-setup/README.md`).
- The code: `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git`, then `git sparse-checkout add portability mspm33c321a am13e230 tools mac-setup`. Every command on this page runs from the repository folder.
- The decoder is h264bsd at a pinned commit (Apache-2.0); `fetch_src.sh` clones it into a work folder and lowers two parameter-set tables. Use baseline streams: main profile is refused.
- Back up the board before the first flash with the command in its `AGENTS.md`.

### Steps

1. **Mac, terminal.** Build the decoder and the harness for the Mac and decode every test stream. It prints one `H264R` line per stream and pass.

   ```text
   portability/h264/build_host.sh
   ```

   You should see: One `H264R` line per stream, each with its frame checksums.

   If `fetch_src.sh` cannot clone the decoder, check the network and run it again.

2. **Mac, terminal.** Build the 176 by 144 set at -Os for the LP-MSPM33C321A; `s240` adds a 240 by 240 stream. The LP-AM13E230 has `am13e230/tools/build_h264.sh`.

   ```text
   mspm33c321a/tools/build_h264.sh s176 Os
   ```

   You should see: The script ends with `built .../h264.out` and a size listing.

   If the SDK is not found, install it as `mac-setup/README.md` says and run it again.

3. **Mac, terminal.** Flash the `.out` with DSLite as the board's `AGENTS.md` shows. The program leaves one record per stream in RAM (`h264_rec`). The results were read back over the probe, and `portability/h264/parse_rec.py` turns the dump into the same `H264R` lines as step 1.

   You should see: The board's lines carry the same frame checksums as the Mac's and a cycle count per frame.

   If a line shows a nonzero status or concealed frames, the stream is not baseline or did not fit; use a stream from `portability/h264/streams`.

### Expected result

Each board decodes the streams with every frame checksum equal to the Mac's.

### What this shows

- Small H.264 baseline video decodes in software on both boards: 176 by 144 at 57.4 frames a second on the LP-MSPM33C321A and at 86.1 on the LP-AM13E230.
- The decoded frames equal the Mac's.

### Limits

- Decode only: no display, and the numbers are test-pattern streams.
- Streams of 240 by 240 and up do not fit the LP-AM13E230's RAM, and 320 by 240 and up do not fit the LP-MSPM33C321A's.
- Baseline profile only. The decoder is a software decoder with no video block.

### Numbers and sources

| | Value | Source |
|---|---|---|
| LP-MSPM33C321A | 176 by 144 at 57.4 frames a second (-Os), 240 by 240 at 33.3; 110.9 kB of RAM for the first | `data/results/mspm33c321a.csv` |
| LP-AM13E230 | 176 by 144 at 86.1, 160 by 112 at 109.9, 128 by 96 at 149.8 frames a second | `data/results/am13e230.csv` |
