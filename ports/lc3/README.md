# liblc3 on the LP-MSPM33C321A: the cost of a 10 ms LE Audio frame on a Cortex-M33

Google's liblc3 (Apache-2.0) encodes and decodes a generated test signal on a Cortex-M33 with a floating-point unit, in the three LE Audio configurations, and the same code is priced with the FPU off. LC3 is the audio codec of Bluetooth LE Audio. This is the codec only: no radio, no Bluetooth link, no sound output.

Terms are in [docs/vocab.md](../../docs/vocab.md); build problems are in the [FAQ](FAQ.md). 1 kB = 1,024 bytes. Contents: Result, Bit-exactness, FPU use, other frame lengths, packet loss and two channels, no-FPU cores, flash and RAM, method, ten-minute path, files.

Status: **Works** on the LP-MSPM33C321A. The encoded bytes and the decoded PCM are bit-exact against the Mac build in all three configurations. The Cortex-M0+ figures are a model run in an emulator, not a measurement on a board.

## Result

One channel, 10 ms frames, 100 frames per configuration, TI Arm Clang 5.1.1.LTS `-O2`, 160 MHz, code in flash. Cycles are DWT counter reads around each `lc3_encode` and `lc3_decode` call. "Share" is the cycles divided by the 1.6 million cycles in 10 ms.

| LE Audio configuration | Encode, cycles per frame | Encode, share of real time | Decode, cycles per frame | Decode, share of real time | Both, clock needed for real time |
|---|---|---|---|---|---|
| 16 kHz, 32 kbps (40 bytes) | 197,885 | 12.4% | 73,459 | 4.6% | 27 MHz |
| 24 kHz, 48 kbps (60 bytes) | 249,279 | 15.6% | 114,018 | 7.1% | 36 MHz |
| 48 kHz, 96 kbps (120 bytes) | 387,683 | 24.2% | 168,113 | 10.5% | 56 MHz |

The slowest single frame is 26.6% (encode) and 12.0% (decode) at 48 kHz. Two channels cost twice as much; that is arithmetic, not a run. Two runs of the same image gave identical cycle counts.

Expectation written before the first run: encode 0.35, 0.55 and 1.1 million cycles, decode 0.20, 0.30 and 0.65 million, from the one decoder timing found for a Cortex-M4F and a vendor rule of thumb. The chip came in at 0.35 to 0.57 of that for encode and 0.26 to 0.38 for decode, so the estimate was 2 to 4 times too high. Flash and RAM landed inside the estimated ranges.

## Bit-exactness

The Mac reference is the same C built with `-ffp-contract=off` and no fast-math (`host/build.sh`). The board build with the same two choices matches it in every frame.

| Build on the board | Encoded bytes against Mac strict | Decoded PCM against Mac strict |
|---|---|---|
| `-O2`, FPU on, strict | equal in 100 of 100 frames, all three configurations | equal, CRC-32 of all samples, all three configurations; at 48 kHz none of the 48k samples differ |
| `-O2`, FPU on, `-ffast-math` (upstream's flag) | not equal (the bytes follow the fast-math arithmetic) | not equal, because the bytes differ |
| same fast-math build against a Mac fast-math build | equal in 100 of 100 frames | 3, 5 and 16 of 100 frames differ; at 48 kHz 21 of the 48k samples differ, largest difference 1 |
| `-O2`, FPU off, strict | equal in 100 of 100 frames | 6, 5 and 21 of 100 frames differ; at 48 kHz 23 of the 48k samples differ, largest difference 1 |

Fast-math changes the encoder's output bytes from the strict build's, on the Mac and on the board alike, so a fast-math stream and a strict stream are not interchangeable bit for bit. Both are valid codec output. The decoder differs by one least-significant bit in a few samples when its float arithmetic changes (fast-math, or the soft-float math library). A quick quality check on the Mac gave a signal-to-noise ratio of 22.2, 25.1 and 10.9 dB at the three configurations (`tests/quality.c`); the low figure at 48 kHz is the sweep above the codec's bandwidth, so the number says the codec worked, not how it sounds.

## Does it use the FPU

Yes. The FPU build has 3,023 single-precision FPU instructions in the image and no call into the soft-float helpers (counted in the disassembly). No fused multiply-add is emitted in the codec. The FPU-off build has none and has 2,165 call sites into the library's float routines (`__mulsf3` 908 sites, `__aeabi_fadd` 713, `__subsf3` 273).

The library has Arm DSP paths (`ltpf_arm.h`, a saturating store in `common.h`) behind `__GNUC__ >= 10`. TI Arm Clang reports `__GNUC__` 4, so the paths are off by default. Compiling the library files with `-U__GNUC__ -D__GNUC__=10` turns them on: encode drops 15.8%, 13.6% and 11.4% (166.6k, 215.3k and 343.3k cycles), decode does not change, and the output stays bit-exact.

## Other frame lengths, packet loss and two channels

Same board, clock, compiler flags and test signal as above (`-O2`, strict arithmetic, FPU on), one second of audio per run. Shares are of one frame at 160 MHz. Cycles are per frame, channels together.

| Frame | Rate and bit rate (bytes per frame) | Encode, cycles | Encode, share | Decode, cycles | Decode, share |
|---|---|---|---|---|---|
| 7.5 ms | 16 kHz, 32 kbps (30) | 161,747 | 13.5% | 58,014 | 4.8% |
| 7.5 ms | 24 kHz, 48 kbps (45) | 202,380 | 16.9% | 88,649 | 7.4% |
| 7.5 ms | 48 kHz, 96 kbps (90) | 304,970 | 25.4% | 129,270 | 10.8% |
| 5 ms | 16 kHz, 32 kbps (20) | 119,266 | 14.9% | 41,182 | 5.1% |
| 5 ms | 24 kHz, 48 kbps (30) | 145,564 | 18.2% | 59,288 | 7.4% |
| 5 ms | 48 kHz, 96 kbps (60) | 207,723 | 26.0% | 132,934 | 16.6% |
| 2.5 ms | 16 kHz, 64 kbps (20) | 97,999 | 24.5% | 26,307 | 6.6% |
| 2.5 ms | 24 kHz, 64 kbps (20) | 108,806 | 27.2% | 33,383 | 8.3% |
| 2.5 ms | 48 kHz, 96 kbps (30) | 136,241 | 34.1% | 67,322 | 16.8% |

The 5 and 2.5 ms frames are LC3plus, which the default build includes. A build with `-DLC3_PLUS=0` refuses them (`lc3_frame_bytes` returns -1 and the setup call returns NULL). At 2.5 ms the smallest frame is 20 bytes, so a request for 32 or 48 kbps at 16 and 24 kHz is raised to 64 kbps; the table shows the rate that ran.

**Packet loss concealment.** 10 ms frames, every 10th frame lost: the decoder is called with no data and returns 1.

| Rate | Decode, mean per frame | Share | Good frame | Concealed frame | Concealed against good |
|---|---|---|---|---|---|
| 16 kHz | 69,051 | 4.3% | 73,208 | 31,638 | 43% |
| 24 kHz | 107,420 | 6.7% | 113,753 | 50,420 | 44% |
| 48 kHz | 161,072 | 10.1% | 168,094 | 97,875 | 58% |

A concealed frame costs less than a good one, so loss lowers the mean. Encode is unchanged (197.8k, 249.3k and 387.7k cycles).

**Two channels.** 10 ms frames, each channel with its own encoder and decoder state at the same rate as the mono runs (96 kbps per channel at 48 kHz).

| Rate | Encode, cycles | Encode, share | Against mono | Decode, cycles | Decode, share | Against mono |
|---|---|---|---|---|---|---|
| 16 kHz | 395,387 | 24.7% | 2.00 | 147,328 | 9.2% | 2.01 |
| 24 kHz | 499,033 | 31.2% | 2.00 | 228,639 | 14.3% | 2.01 |
| 48 kHz | 773,448 | 48.3% | 2.00 | 335,941 | 21.0% | 2.00 |

The two channels carry different test signals. The cost is two times mono within 1 percent, as for two independent channels.

Decode cost depends on the signal. The speech-like second half of the test costs more than the sweep half in most runs: 1.25 to 1.6 times at 16 and 24 kHz, and 1.8 and 2.0 times at 48 kHz with 2.5 and 5 ms frames (5 ms: 177.7k against 88.1k cycles). Only the 48 kHz runs at 7.5 ms and at 10 ms stereo are level (0.97). The cause is not isolated. The worst single frame is 34.2% (5 ms) and 43.2% (2.5 ms) at 48 kHz, against a mean of 16.6% and 16.8%.

All 15 runs match the Mac strict build: the CRC-32 of the whole bitstream and of all decoded samples is equal in every one (per-frame CRCs are not recorded for these runs). Two runs of the same image agree within 0.3 percent.

Expectation written first: 7.5 ms about 5 percent above the 10 ms share, 5 ms 15 percent, 2.5 ms 40 percent; concealed frame 0.6 to 0.9 of a good one; stereo 2.0 times mono. The 7.5 and 5 ms encode rows came in at the estimate (13.5, 16.9 and 25.4% against 13, 16.5 and 25 expected at 7.5 ms). The 2.5 ms encode at 16 and 24 kHz was 1.7 to 2.0 times the 10 ms share where 1.4 was expected (the 64 kbps rate is part of the reason); the concealed frame was 0.43 to 0.58, cheaper than expected.

## If the core has no FPU

| 10 ms frame, mono | FPU on (measured) | FPU off, same M33 core (measured) | Cortex-M0+ 80 MHz class, soft float (model) |
|---|---|---|---|
| 16 kHz encode | 197,885 (12.4%) | 1,923,941 (120.2%) | 2,634,185 (329% of a frame at 80 MHz) |
| 16 kHz decode | 73,459 (4.6%) | 824,688 (51.5%) | 1,008,673 (126%) |
| 24 kHz encode | 249,279 (15.6%) | 2,545,258 (159.1%) | 3,504,164 (438%) |
| 24 kHz decode | 114,018 (7.1%) | 1,346,463 (84.2%) | 1,546,762 (193%) |
| 48 kHz encode | 387,683 (24.2%) | 4,198,675 (262.4%) | 5,844,061 (731%) |
| 48 kHz decode | 168,113 (10.5%) | 2,250,324 (140.6%) | 3,290,590 (411%) |

Percentages in the middle column are of 160 MHz; in the right column of 80 MHz. Taking the FPU away costs 9.7 to 13.4 times the cycles. A core without one cannot encode even 16 kHz mono in real time at 80 MHz; it needs about 263 MHz for encode alone and 364 MHz for encode plus decode. Priced with a 32-cycle multiply, the Cortex-M0+ 16 kHz encode is 4.0 million cycles.

The middle column is a measurement: the codec and the bench compiled for the M33 with the FPU not used (`-mcpu=cortex-m33+nofp -mfloat-abi=soft`), so every float operation is a library call. The right column is a model, not an M0+ run. The same source is built with `arm-none-eabi-gcc` 14.3.1 for `-mcpu=cortex-m0plus -msoft-float`, and the real instructions run in the Unicorn emulator with a Cortex-M0+ cycle price per instruction (most 1 cycle, loads and stores 2, taken branches 2, `bl` 3, multiply 1). Flash wait states and bus stalls are left out, so read the model as a floor. The emulator's encoded bytes and decoded PCM match the Mac strict build in all 10 frames it ran per configuration (`tests/emu_m0.py`). The model is 1.15 to 1.46 times the FPU-off M33 figure; the M0+ has no long multiply, which is the likely reason.

## Flash and RAM

| Build (FPU on, strict) | Flash | Of which code | Of which constant tables | Peak stack |
|---|---|---|---|---|
| `-O2` | 157.8 kB | 72.9 kB | 84.4 kB | 5.5 kB |
| `-Os` | 117.8 kB | 32.8 kB | 84.4 kB | 5.5 kB |
| `-O2`, LC3plus left out (`-DLC3_PLUS=0`) | 139.9 kB | 72.8 kB | 66.8 kB | 5.5 kB |
| `-O2`, LC3 only (`-DLC3_PLUS=0 -DLC3_PLUS_HR=0`) | 123.4 kB | 71.9 kB | 51.1 kB | 3.6 kB |
| `-Os`, LC3 only | 83.2 kB | 31.7 kB | 51.1 kB | 3.6 kB |

Flash counts the whole test image: the codec, its tables, the math library, the clock setup and the harness. The LC3-only builds produce the same bytes and the same PCM as the full build (CRC-32 equal to the Mac, three configurations). `-Os` costs encode time: 246.2k, 292.0k and 434.3k cycles (+24%, +17%, +12% against `-O2`), decode within 2% of `-O2`. `-O3` is slower than `-O2` on encode (246.1k, 290.8k and 433.3k) and 10% faster on decode at 16 and 24 kHz (65.9k and 102.5k). `-ffast-math` gives no speed-up on this core: 197.4k, 249.4k and 388.0k cycles for encode.

RAM, from the library's own size functions for one mono channel: the encoder needs 2.5, 3.2 and 5.3 kB and the decoder 3.1, 4.5 and 8.9 kB at the three sample rates. The test image has 32.7 kB of zero-initialized data (the bitstream of a whole run, the per-frame results and the codec state sized for 48 kHz) and reserves a 16 kB stack. The stack was painted before the run; 5.5 kB was used at `-O2` and 3.6 kB in the LC3-only build.

## Method

- Signal: generated with integer arithmetic only, so the Mac and the board make the same samples. One second per configuration: a 0.5 s parabolic-sine sweep from 100 Hz to 0.45 of the sample rate, then 0.5 s of speech-like sound (120 Hz harmonics under a 4 Hz envelope, plus low-passed noise). No recording is used.
- Library: `third-party/liblc3/`, tag v1.1.3, unmodified. Compile flags only.
- Per frame the bench records the cycles inside each codec call, a CRC-32 of the encoded bytes and a CRC-32 of the decoded samples. A run also records the CRC-32 of the whole bitstream and of all decoded samples. The decoder reads the stored bitstream, so encode and decode are timed apart.
- Clock: 160 MHz from the SYSPLL, two flash wait states, caches on (the SysConfig output of TI's `sysctl_mclk_syspll` example, as `ports/llama2c/build.sh` makes it).
- Results sit in RAM and are read over the probe with DSLite over the probe with DSLite. The image touches no pin and makes no sound.
- The extras (7.5, 5 and 2.5 ms, packet loss, two channels) use `common/lc3x.c`: one second per run, a CRC-32 of the whole bitstream and of all decoded samples, cycle sums and minimum and maximum per frame. A second generator with another seed and phase makes the second channel.
- Not covered: other sample rates (8 and 32 kHz), real speech or music, power, and any radio link.

## Ten-minute path

Needs the MSPM33 SDK 1.04.00.00, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0 (the SysConfig output of `ports/llama2c/build.sh`, folder `gen160`), a C compiler for the Mac reference, and for the M0+ model `arm-none-eabi-gcc` and a Python with `unicorn`.

```bash
cd ports/lc3
host/build.sh                                     # Mac: strict and fast-math references, per-frame CRCs
target-m33/build.sh O2 hard strict                # M33 image: $OUT/lc3-O2-hard-strict/lc3.out (flashes nothing)
cd ../.. && python3 ports/lc3/run_board.py <that lc3.out>   # flash, read results, compare CRCs
ports/lc3/target-m33/build.sh O2 soft strict      # FPU off: same core, all float in the library
EXTRA="-U__GNUC__ -D__GNUC__=10" TAG=armpaths ports/lc3/target-m33/build.sh O2 hard strict   # the library's Arm paths
ports/lc3/target-g3519/build.sh -O2               # Cortex-M0+ image for the emulator
ports/lc3/host/buildx.sh                          # Mac strict reference for the extras (frame lengths, loss, stereo)
ports/lc3/target-m33/buildx.sh O2 hard strict     # extras image: $OUT/lc3x-O2-hard-strict/lc3x.out
python3 ports/lc3/run_boardx.py <that lc3x.out>   # flash, read 15 runs, compare CRCs
<python with unicorn> -I ports/lc3/tests/emu_m0.py <dir>/lc3.elf <dir>/lc3.bin [--mul 32]   # about 70 s
```

`run_board.py` leaves the board running whatever the next image is. Add `EXTRA=-DLC3_KEEP_OUT TAG=pcm` to a build and `--pcm` to `run_board.py` to read back the 48 kHz decoded samples and compare them one by one.

## Files

| Path | What |
|---|---|
| `third-party/liblc3/` | liblc3 v1.1.3, unmodified, pinned, license |
| `common/lc3run.c`, `common/lc3run.h` | The signal generator and the encode and decode loop (timing, CRCs), shared by every target |
| `host/` | Mac reference builds |
| `target-m33/` | Board main and build script (FPU on or off, strict or fast-math) |
| `target-g3519/`, `tests/emu_m0.py` | Cortex-M0+ image and the emulator model |
| `common/lc3x.c`, `host/refx.c`, `target-m33/mainx.c`, `run_boardx.py` | The extras: frame durations, packet loss, two channels |
| `tests/quality.c` | Mac signal-to-noise check |
| `RESULTS-ROWS.csv`, `RESULTS-ROWS-EXTRAS.csv` | The measured rows |
| `ports/opus/` | Opus on the same board, with a table beside LC3 at the same bit rates |
