# libopus on the LP-MSPM33C321A: Opus voice and music frames on a Cortex-M33

libopus 1.5.2 (BSD-3-Clause) encodes and decodes a generated test signal on a Cortex-M33 at 160 MHz: SILK at 16 kHz with 20 ms frames, CELT at 48 kHz with 10 and 20 ms frames, and CELT at 16 kHz with 10 ms frames, each at encoder complexity 0, 5 and 10, in a fixed-point build and a floating-point build. This is the codec only: no radio, no Bluetooth link, no sound output.

Terms are in [docs/vocab.md](../../docs/vocab.md). 1 kB = 1,024 bytes. Contents: Result, Against LC3, Fixed-point or floating-point, Bit-exactness, Build options, Flash and RAM, Method, Ten-minute path, Files.

Status: **Works** on the LP-MSPM33C321A. The fixed-point build is bit-exact against a Mac build that uses the same multiply macros, in all 12 runs and at `-O2`, `-O3` and `-Os`. The floating-point build runs, but it is not bit-exact: its packets match the Mac in 11 of 12 runs and its decoded samples in 2 of 12.

## Result

Fixed-point build, mono, constant bit rate, TI Arm Clang 5.1.1.LTS `-O2`, 160 MHz, code in flash. Cycles are DWT counter reads around each `opus_encode` and `opus_decode` call, averaged over one second of audio. "Share" is the cycles divided by the cycles in one frame.

| Configuration | Complexity | Encode, cycles per frame | Encode, share | Decode, cycles per frame | Decode, share |
|---|---|---|---|---|---|
| SILK, 16 kHz, 20 ms, 32 kbps (80 B) | 0 | 1,794,510 | 56.1% | 155,615 | 4.9% |
| | 5 | 7,863,088 | 245.7% | 163,405 | 5.1% |
| | 10 | 15,356,490 | 479.9% | 164,509 | 5.1% |
| CELT, 48 kHz, 10 ms, 96 kbps (120 B) | 0 | 467,578 | 29.2% | 276,757 | 17.3% |
| | 5 | 858,031 | 53.6% | 358,367 | 22.4% |
| | 10 | 861,079 | 53.8% | 358,374 | 22.4% |
| CELT, 48 kHz, 20 ms, 96 kbps (240 B) | 0 | 811,976 | 25.4% | 439,733 | 13.7% |
| | 5 | 1,446,669 | 45.2% | 555,290 | 17.4% |
| | 10 | 1,458,902 | 45.6% | 555,394 | 17.4% |
| CELT, 16 kHz, 10 ms, 32 kbps (40 B) | 0 | 342,034 | 21.4% | 224,828 | 14.1% |
| | 5 | 690,052 | 43.1% | 271,330 | 17.0% |
| | 10 | 693,255 | 43.3% | 271,330 | 17.0% |

The SILK encoder is real time on this core only at complexity 0, and then with a worst frame of 79.5%. At complexity 5 it needs 2.5 times the core and at complexity 10 4.8 times. The SILK decoder and every CELT run fit. Above complexity 5 the CELT encoder does no more work: the 5 and 10 rows agree within 1 percent, and the 16 kHz CELT packets are identical. The mode was read from the first byte of each packet: SILK-only for the 16 kHz voice rows, CELT-only for the others, no hybrid packets.

Expectation written before the first run, from published Cortex-M4 figures: SILK encode 18, 35 and 55 percent at complexity 0, 5 and 10, CELT 48 kHz encode 22, 45 and 60 percent, decode 25 percent. The 48 kHz CELT figures landed within 45 percent of that. The 16 kHz CELT rows were higher (decode 14 to 17 percent against 8 expected), and the SILK encode was 3 to 9 times higher. The same SILK run on the Mac counts 1.16M instructions per frame (encode and decode together) at complexity 0 and 2.7M at complexity 5 (`/usr/bin/time -l`, Intel, 64-bit), so the cost rises with the algorithm and a slow board is not the cause. The board's cycles rise 4.1 times from complexity 0 to 5 (encode and decode together) where the Mac's instruction count rises 2.3 times; the reason is not isolated, and the 64-bit arithmetic in SILK is the suspect.

## Against LC3 at the same bit rate

LC3 numbers are from the [LC3 port](../lc3/README.md): 10 ms frames, mono, strict arithmetic, same board, clock and signal. Opus is the fixed-point build at `-O2`. LC3 has no 20 ms frame, so the SILK rows compare a 20 ms Opus frame with 10 ms LC3 frames at the same rate.

| Rate | Codec and complexity | Encode, share | Decode, share | Encode against LC3 | Decode against LC3 |
|---|---|---|---|---|---|
| 48 kHz, 96 kbps | LC3 | 24.2% | 10.5% | 1.0 | 1.0 |
| | Opus CELT 10 ms, 0 | 29.2% | 17.3% | 1.2 | 1.6 |
| | Opus CELT 10 ms, 5 and 10 | 53.6% | 22.4% | 2.2 | 2.1 |
| 16 kHz, 32 kbps | LC3 | 12.4% | 4.6% | 1.0 | 1.0 |
| | Opus CELT 10 ms, 0 | 21.4% | 14.1% | 1.7 | 3.1 |
| | Opus CELT 10 ms, 5 and 10 | 43.1% | 17.0% | 3.5 | 3.7 |
| | Opus SILK 20 ms, 0 | 56.1% | 4.9% | 4.5 | 1.1 |
| | Opus SILK 20 ms, 5 | 245.7% | 5.1% | 19.8 | 1.1 |

Memory, fixed-point Opus against LC3 (LC3 sizes are the bytes of its results rows divided by 1,024): encoder state 24.0 kB against 2.5 to 5.3 kB, decoder state 17.4 kB against 3.1 to 8.9 kB, peak stack 27.5 kB against 5.5 kB. Whole test image at `-O2`: 331.4 kB against 157.8 kB; at `-Os`: 164.7 kB against 117.8 kB (LC3 alone at `-Os`: 83.2 kB). The decoded signal of the two codecs was not compared for quality; see the method notes.

## Fixed-point or floating-point

The LP-MSPM33C321A has a single-precision FPU. libopus's floating-point build keeps double-precision arithmetic in SILK and in some CELT code, and the board does every double operation in software: the floating-point image has 1,080 call sites into the soft-double routines and the fixed-point image has none.

| Configuration, complexity | Fixed encode | Float encode | Fixed decode | Float decode |
|---|---|---|---|---|
| SILK 16 kHz, 20 ms, 0 | 56.1% | 401.1% | 4.9% | 11.2% |
| SILK 16 kHz, 20 ms, 5 | 245.7% | 1,044.0% | 5.1% | 11.5% |
| CELT 48 kHz, 10 ms, 5 | 53.6% | 62.0% | 22.4% | 48.3% |
| CELT 48 kHz, 20 ms, 5 | 45.2% | 49.5% | 17.4% | 39.6% |
| CELT 16 kHz, 10 ms, 5 | 43.1% | 47.4% | 17.0% | 27.8% |

The fixed-point build is faster in all 12 runs, by 1.0 to 7.2 times on encode and 1.6 to 2.4 times on decode for CELT (the SILK decode is 2.3 times), and it needs less memory (encoder state 24.0 kB against 42.5 kB; peak stack 27.5 kB against 34.6 kB). The float build's flash is 392.4 kB at `-O2` and 201.1 kB at `-Os`; the fixed build's is 331.4 and 164.7 kB. The expectation was CELT within 20 percent of the fixed build and SILK float encode 2 to 4 times fixed; CELT decode was 1.6 to 2.4 times slower and SILK encode 3.3 to 7.2 times. It expected float packets to match the Mac in CELT and to differ in SILK; the result was the reverse. It expected 190 and 260 kB of flash at `-O2`; the images were 1.5 to 1.7 times that, and the `-Os` builds came in under the estimate.

## Bit-exactness

libopus picks 64-bit multiply macros when the host is 64-bit and a 32-bit set when it is not (`celt/arch.h`, `OPUS_FAST_INT64`). A 64-bit Mac and this 32-bit core therefore do not run the same arithmetic by default. The two sets differ in the CELT pitch pre-filter, which the encoder uses from complexity 5.

| Board build | Mac reference | Packets that differ | Decoded frames that differ |
|---|---|---|---|
| fixed, `-O2`, default (32-bit macros) | Mac with 32-bit macros | 0 of 12 runs | 0 |
| fixed, `-O2`, default | stock Mac (64-bit macros) | CELT complexity 5 and 10: 6 of 12 runs | those 6 runs |
| fixed, `-O2`, `OPUS_FAST_INT64` forced to 1 | stock Mac | 0 of 12 runs | 0 |
| fixed, `-O3` and `-Os` | Mac with 32-bit macros | 0 of 12 runs | 0 |
| fixed, `-O2`, `-DOPUS_ARM_INLINE_EDSP` | Mac with 32-bit macros | SILK equal; CELT differs in 23 to 82 percent of packets | CELT differs in 79 to 100 percent of frames |
| float, `-O2` | Mac float | 1 of 12 runs (SILK complexity 10: 44 of 50 packets) | 10 of 12 runs: 2 to 36 percent of frames in the 9 CELT runs, 45 of 50 in SILK complexity 10 |

Both fixed-point arithmetic sets are valid Opus; a stream from one decodes with the other, but the bytes are not interchangeable between machines. The Mac reference is built from a copy of the library with the one `#define` changed (`host/build.sh`, `INT64=0`); the library files in `third-party/opus/` are untouched. Floating-point decode differs from the Mac on identical packets in CELT. The cause is not isolated; the float code calls the C math library (`exp2`, `log`, `cos`), which differs between the Mac and the board, and the size of the difference in samples was not measured.

## Build options

| Fixed-point build | Flash | Encode, SILK complexity 0 | Encode and decode, CELT 48 kHz 10 ms, complexity 5 |
|---|---|---|---|
| `-O2` | 331.4 kB | 1,794,510 | 858,031 and 358,367 |
| `-O3` | 349.4 kB | 1,762,844 | 861,045 and 349,896 |
| `-Os` | 164.7 kB | 1,809,675 | 852,492 and 337,317 |
| `-O2`, `OPUS_FAST_INT64` 1 | 325.1 kB | 1,639,702 | 875,411 and 382,596 |
| `-O2`, `-DOPUS_ARM_INLINE_EDSP` | 303.9 kB | 1,626,366 | 826,701 and 328,848 |

`-Os` halves the flash. In these rows its encode is within 1 percent of `-O2` and its decode is 6 percent faster, so it is the build to start from. The 64-bit macros speed SILK by 7 to 10 percent and slow CELT by 2 to 12 percent. The library's own Arm assembly multiplies (`OPUS_ARM_INLINE_EDSP`) speed every row by 4 to 12 percent, but change the CELT output.

## Flash and RAM

Whole test image, fixed-point `-O2`: 331.4 kB, of which 309.1 kB code and 22.0 kB constants (0.3 kB other). The application supplies the codec state: `opus_encoder_get_size(1)` is 24.0 kB (fixed) or 42.5 kB (float) and `opus_decoder_get_size(1)` is 17.4 or 17.8 kB. Peak stack with the encoder at complexity 10: 27.5 kB (fixed) and 34.6 kB (float), found by painting 64 kB before the run. libopus allocates its scratch on the stack here (`VAR_ARRAYS`), so a task needs that much stack.

## Method

- Signal: the generator of the LC3 port, integer arithmetic only, so the Mac and the board make the same samples. One second per run: a 0.5 s parabolic-sine sweep from 100 Hz to 0.45 of the sample rate, then 0.5 s of speech-like sound. No recording is used.
- Library: `third-party/opus/`, tag v1.5.2, unmodified. Compile flags and a `config.h` only (`common/config.h`). No `lrint`, so float-to-integer conversion rounds the same on the Mac and on the board. The deep-learning concealment and extensions in `dnn/` are not built.
- Encoder settings: constant bit rate, no in-band FEC, no DTX, signal type and bandwidth fixed (voice and wideband at 16 kHz, music and full band at 48 kHz), `OPUS_APPLICATION_VOIP` for SILK, `OPUS_APPLICATION_AUDIO` for 48 kHz CELT and `OPUS_APPLICATION_RESTRICTED_LOWDELAY` for 16 kHz CELT. The mode in each packet's first byte was counted.
- Per run the bench records cycles inside each call, a CRC-32 of every packet and of every decoded frame, and the CRC-32 of the whole stream. The decoder reads the stored packets, so encode and decode are timed apart.
- Clock: 160 MHz from the SYSPLL, two flash wait states, caches on (the SysConfig output of `ports/llama2c/build.sh`, folder `gen160`). Results sit in RAM and are read over the probe with DSLite. The image touches no pin and makes no sound.
- Quality: the decoded second correlates with the input at a delay of 2.5 to 6.5 ms, with a signal-to-noise ratio of 9 to 11 dB at the best delay on the Mac fixed build (`tests/quality.sh`). That shows the codec ran, not how it sounds; the test signal contains noise and a sweep that a perceptual codec does not reproduce sample by sample.
- Not covered: stereo, other bit rates and frame durations, packet loss and FEC, the hybrid mode, real speech or music, power, and the neural extensions of 1.5.

## Ten-minute path

Needs the MSPM33 SDK 1.04.00.00, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0 (the SysConfig output of `ports/llama2c/build.sh`, folder `gen160`) and a C compiler for the Mac reference.

```bash
cd ports/opus
INT64=0 host/build.sh                               # Mac references: stock (64-bit macros) and 32-bit macros, with per-frame CRCs
target-m33/build.sh fixed Os                        # board image: $OUT/opus-fixed-Os/opus.out (flashes nothing)
cd ../.. && python3 ports/opus/run_board.py <that opus.out> fixed --ref fixed32   # flash, read, compare CRCs
ports/opus/target-m33/build.sh float O2             # floating-point image
FASTINT64=1 ports/opus/target-m33/build.sh fixed O2 # 64-bit multiply macros (compare with the stock Mac reference: omit --ref)
EXTRA=-DOPUS_ARM_INLINE_EDSP TAG=edsp ports/opus/target-m33/build.sh fixed O2   # library Arm multiplies
ports/opus/tests/quality.sh fixed                   # Mac signal-to-noise check
```

`run_board.py` leaves the board running whatever the next image is.

## Files

| Path | What |
|---|---|
| `third-party/opus/` | libopus 1.5.2, unmodified, pinned, license |
| `common/opusrun.c`, `common/opusrun.h`, `common/opuscfg.h`, `common/config.h` | Signal generator, encode and decode loop (timing, CRCs), the run list, build options |
| `sources.sh` | Source file lists, read from libopus's own `*_sources.mk` |
| `host/` | Mac reference builds |
| `target-m33/` | Board main and build script |
| `run_board.py` | Flash, read results, compare CRCs |
| `tests/` | Mac quality check, results-row writer |
