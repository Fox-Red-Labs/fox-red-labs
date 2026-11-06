# minimp3 on the LP-MSPM33C321A and LP-MSPM0G3519: decode speed of an MP3 on two Cortex-M cores

A 5-second MP3 decoded frame by frame by minimp3 (CC0, one header) on a Cortex-M33 with a floating-point unit, and the same code built for a Cortex-M0+ without one. Decode only: no audio is played.

Terms are in [docs/vocab.md](../../docs/vocab.md). 1 kB = 1,024 bytes. Contents: [Result](#result), [Build options on the M33](#build-options-on-the-m33), [the LP-MSPM0G3519](#the-lp-mspm0g3519-cortex-m0-soft-float), [Checks](#checks), [Ten-minute path](#ten-minute-path).

Status: **Works** on the LP-MSPM33C321A and on the LP-MSPM0G3519. The PCM output is bit-exact against the Mac's scalar build on both clips on both boards (194 of 194 frame checksums each). The M33 decodes in real time with room to spare; the Cortex-M0+ takes 9.8 times the audio length for stereo and 7.4 for mono, so it does not keep up.

## Result

| | LP-MSPM33C321A, 160 MHz, hardware float | LP-MSPM0G3519, 80 MHz, soft float |
|---|---|---|
| 128 kbps stereo, 44.1 kHz | **0.151 times real time** (764.7 ms for 5.068 s of audio) | **9.84 times real time** (49.9 s for 5.068 s of audio) |
| 64 kbps mono, 44.1 kHz | **0.109 times real time** (551.4 ms) | **7.38 times real time** (37.4 s) |
| Clock needed to keep up in real time | 24.1 MHz (stereo), 17.4 MHz (mono) | about 787 MHz (stereo), 591 MHz (mono) |
| PCM against the Mac | identical: 194 frames, one 32-bit checksum per clip equal, maximum sample error 0 | identical: 194 frames, every per-frame checksum equal on both clips |

Real-time factor is decode time divided by audio time; below 1 keeps up. The M33 figures are DWT cycle-counter reads around every `mp3dec_decode_frame` call (checksumming excluded), summed over 194 frames; two runs gave identical cycle counts. The M0+ figures are the same kind of read with a 32-bit count built from SysTick and an overflow interrupt, summed over 194 frames; two runs agree to within 70 cycles in 4 billion. The clip is 223k samples per channel, which is 5.068 s: 194 frames, 1152 per frame, including the encoder's start and end padding.

Expectation written before the runs: about 0.1 to 0.25 on the M33 (the library's own benchmark shows 1.5 to 2.7 million clock ticks per second of audio on an x86 with SSE; scalar code on an FPU core should cost several times that), and above 1 on the M0+ because every float operation becomes a library call. Both held; the M0+ is 25 times slower per frame than the M33.

## Does it use the FPU

Yes. The M33 image has 1.9k FPU instructions (`vmul.f32`, `vadd.f32`, `vsub.f32` and others) and no call into the float library; no fused multiply-add is emitted, which is why it matches the Mac's `-ffp-contract=off` build to the last bit. minimp3 has no fixed-point path: the arithmetic is float only, and `MINIMP3_FLOAT_OUTPUT` only changes the output format. So there is no fixed-point row; on a chip without an FPU the choice is soft float, as on the M0+ below. A soft-float build of the M33 image did not link: the SDK's driverlib archive is hard float.

## Build options on the M33

| Build (TI Arm Clang 5.1.1.LTS) | Flash | Stereo, real-time factor | Mono, real-time factor | Checksums |
|---|---|---|---|---|
| `-O2` | 153.4 kB | 0.1509 (122.35 M cycles) | 0.1088 (88.22 M cycles) | equal |
| `-O3` | 154.5 kB | 0.1494 (121.11 M cycles) | 0.1079 (87.51 M cycles) | equal |
| `-Os` | 138.9 kB | 0.1892 (153.44 M cycles) | 0.1435 (116.40 M cycles) | equal |

Flash holds 118.8 kB of the two clips; the decoder and its tables are about 35 kB at `-O2`. RAM is 33.2 kB static (decoder state 6.6 kB, a PCM frame 4.6 kB, the result buffers 22.5 kB) plus the stack: one `mp3dec_decode_frame` call uses a 17.3 kB frame on the stack, so the image reserves 32 kB.

Per-frame spread at `-O2`: stereo 618k to 680k cycles (mean 631k), mono 446k to 472k (mean 455k). A frame is 26.12 ms of audio, which at 160 MHz is 4.18 million cycles, so the worst frame uses 16 percent of its budget. Mono costs 0.72 of stereo, not 0.5; the Mac build shows the same ratio (7.7 ms against 10.4 ms per run), so it is the decoder, not the chip.

## The LP-MSPM0G3519 (Cortex-M0+, soft float)

The same source builds with `arm-none-eabi-gcc` 14.3.1 `-O2 -mcpu=cortex-m0plus -msoft-float` and TI's driverlib: 150.3 kB of flash and 34.1 kB of static RAM plus the 32 kB stack, well inside the 512 kB and 128 kB (`target-g3519/build.sh`). It runs on the board at 80 MHz (SYSPLL, flash wait state 2) and reads back through DSLite.

Expectation written before the run: 8.5 to 13 times real time for stereo and 6.3 to 9.5 for mono, from the emulator model at the end of this section, pushed up because the model leaves out flash wait states. Both held.

| Clip | Cycles per frame, frames 2 to 13 | Slowest frame | Real-time factor | Emulator model, 1-cycle and 32-cycle multiply |
|---|---|---|---|---|
| 128 kbps stereo | 20.53 M | 20.72 M (259 ms) | 9.84 | 7.7 and 10.7 (16.11 M per frame) |
| 64 kbps mono | 15.43 M | 15.53 M (194 ms) | 7.38 | 5.8 and 7.9 (12.12 M per frame) |

A frame is 26.12 ms of audio, which is 2.09 M cycles at 80 MHz, so a stereo frame takes ten times its budget. Frame 0 (14.9 M stereo) and frame 1 (18.6 M) are cheaper because the bit reservoir is still empty. The whole bench, both clips with hashing, wrapped the 32-bit counter once; the hashing cost works out to about 66 M cycles, under one percent of the 7.0 billion decode cycles.

**How close the model was.** The board needs 1.27 times the cycles the emulator priced (20.53 M against 16.11 M, 15.43 M against 12.12 M), the same ratio for both clips. That is the part the model left out, flash wait states and fetch stalls; it was not separated further here. The multiply-cost range bracketed the measurement: the 1-cycle column was 22 percent low (7.7 against 9.84), the 32-cycle column 9 percent high (10.7). The measured factor sits between them, closer to the 32-cycle end, so a 1-cycle multiplier alone does not predict this chip at 80 MHz. Use the emulator as a floor and add about a quarter for a flash-resident build at this clock.

**The timer.** SysTick is 24 bits and wraps every 16.78 M cycles, and a steady stereo frame is 20.5 M cycles, so a bare SysTick read would have returned each frame modulo 2^24 and understated it by a fifth. The image counts SysTick wraps in its interrupt and rebuilds a 32-bit value (`target-g3519/main.c`); the read runs with interrupts off and corrects for a wrap whose interrupt has not run. Checked against two known delays before the decode: `delay_cycles(80,000,000)` counted 80,000,264 and `delay_cycles(160,000,000)` counted 160,000,551 (3 ppm). A second check against the host's clock, reading the counter twice 22.6 s apart, gave 80.27 and 80.39 MHz, within the 0.3 s read jitter of that method.

Consequence: this chip cannot decode stereo MP3 in real time in float; it would need about 787 MHz. Layer III on a part like this needs a fixed-point decoder, which minimp3 is not.

The emulator model (made before the board run): `tests/emu_m0.py` runs the real image in a Cortex-M0 emulator (Unicorn) for the first 14 frames of each clip and prices every executed instruction with a Cortex-M0+ cycle table: one cycle for most, two for loads and stores, taken branches two, `bl` three, and a multiply at either 1 or 32 cycles. It leaves out flash wait states and fetch stalls. It executed 303 million instructions, 4.4 million of them multiplies, and gave 16.11 M cycles per steady stereo frame (16.03 to 16.20 M) and 12.12 M per mono frame.

## Test clips

`clips/stereo128.mp3` (79.2 kB) and `clips/mono64.mp3` (39.6 kB), both 44.1 kHz, CBR, made by `tools/make_clips.sh` from ffmpeg signal sources (two slow-gliding tones with a decay pattern and a little pink noise, so the Huffman tables see real work) and `lame` 4.0. They contain no third-party recording; they are generated here and free to reuse. Under 300 kB together, linked into the image as arrays.

## Checks

| Check | Result |
|---|---|
| M33 against the Mac scalar build (`-DMINIMP3_NO_SIMD -ffp-contract=off`) | 194 of 194 per-frame checksums equal on both clips; last frame sample by sample, maximum error 0 |
| Mac SSE build against the Mac scalar build | 263 of 447k stereo samples and 125 of 223k mono samples differ, by 1 at most |
| Mac scalar build against ffmpeg's decoder (start delay removed) | differs by 1 at most in 1,211 of 441k stereo samples and 465 of 220k mono samples |
| LP-MSPM0G3519 against the Mac scalar build | 194 of 194 per-frame checksums equal on both clips, in two runs; the last frame's samples were not read back |
| M0+ emulator against the Mac scalar build | first 14 frames of each clip, per-frame checksums equal |

The checksum is 32-bit FNV-1a over the little-endian 16-bit samples, per frame and for the whole clip. The Mac reference is `host/ref.c`, which uses the same `common/mp3run.c` as the boards. A matching hash proves the output; the 1-sample differences above belong to the SIMD path and to another decoder, not to the Cortex-M builds.

## Ten-minute path

Needs the MSPM33 SDK 1.04.00.00, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0 (the SysConfig output of `ports/llama2c/build.sh`, folder `gen160`), `ffmpeg` and `lame` only to remake the clips, and for the M0+ build `arm-none-eabi-gcc`, and for the optional M0+ emulator model a Python with `unicorn`.

```bash
cd ports/minimp3
tests/check_host.sh                      # Mac: build the references, decode both clips, compare with the SSE build and ffmpeg
target-m33/build.sh O2 hard              # M33 image: $OUT/mp3-O2-hard/mp3.out (flashes nothing)
cd ../.. && python3 ports/minimp3/run_board.py <that mp3.out>   # flash, read the results, compare checksums
PATH=<gnu arm bin>:$PATH OUT=<dir> ports/minimp3/target-g3519/build.sh -O2   # M0+ image, size line
cd ../.. && python3 ports/minimp3/run_board_g3519.py <dir>/mp3.elf   # flash the LP-MSPM0G3519, wait, read back, compare checksums (about 2 minutes)
cd ports/minimp3
XFLAGS="-DMP3_EMU -DMP3_STOP_AT=14 -UMP3_TIMER_MASK" OUT=<dir> ports/minimp3/target-g3519/build.sh -O2
<python with unicorn> -I ports/minimp3/tests/emu_m0.py <dir>/mp3.elf <dir>/mp3.bin [--mul 32]   # about 2 minutes
```

`run_board.py` and `run_board_g3519.py` leave the board running the bench image, so flash your own image afterwards; they play no sound and touch no pin. Read the LP-MSPM0G3519 with DSLite `memory` or while the core is running: an OpenOCD connect resets this board through the XDS110, which restarts the run and clears the results.

## Files

| Path | What |
|---|---|
| `third-party/minimp3/` | minimp3, unmodified, pinned, license |
| `common/mp3run.c`, `common/mp3bench.c` | The decode loop (timing, checksums) and the two-clip bench, shared by every target |
| `host/` | Mac reference builds |
| `target-m33/`, `target-g3519/` | Board mains and build scripts |
| `run_board.py`, `run_board_g3519.py` | Flash, wait, read back and compare, for each board |
| `tests/` | Host check, M0+ emulator estimate |
