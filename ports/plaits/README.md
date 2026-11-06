# Plaits synthesis engines on the LP-MSPM33C321A: what 24 voices cost on a Cortex-M33

The Plaits synthesis code (MIT, Emilie Gillet's STM32 firmware for the Plaits eurorack module from Mutable Instruments) renders 24 synth engines on a Cortex-M33 with a floating-point unit. It uses the module's own 12-sample blocks at 48 kHz. The rendered samples are bit-exact against a Mac build. This is the synthesis code only: the sound goes to a RAM buffer, no DAC, no MIDI, no pot or CV input.

Contents: Result, Bit-exactness, Does it use the FPU, Build level, Flash and RAM, Method, Ten-minute path, Files.

Terms are in [docs/vocab.md](../../docs/vocab.md). In short: **DWT** is the Cortex-M cycle counter, **CRC-32** is a 32-bit checksum, and the **low-pass gate** (LPG) is the voice's volume-and-brightness envelope stage. 1 kB = 1,024 bytes.

Status: **Works** on the LP-MSPM33C321A. 24 of 24 engines match the Mac build in all 256 blocks, and 23 of 24 stay under 37% of real time in every block. The speech engine has three blocks that run over real time (below).

## Result

TI Arm Clang 5.1.1.LTS `-O2`, FPU on, 160 MHz, code and tables in flash. A 12-sample block at 48 kHz lasts 250 microseconds, which is 40k cycles at 160 MHz. Cycles are DWT counter reads around each `Voice::Render` call, which includes the engine, the low-pass gate and the limiter. Each engine plays 256 blocks (64 ms): four notes, the gate high for 48 blocks of each 64, internal envelope on, so the low-pass gate is working. "Mean" leaves out block 1, which holds the engine switch and its data load. Two runs of the same image gave identical cycle counts.

| Engine | Mean cycles per block | Share of real time | Worst block | Share | Block 1 |
|---|---|---|---|---|---|
| 0 VA with filter | 10,010 | 25.0% | 10,135 | 25.3% | 10,110 |
| 1 phase distortion | 11,477 | 28.7% | 11,735 | 29.3% | 11,482 |
| 2 six-operator FM, bank A | 11,546 | 28.9% | 13,417 | 33.5% | 41,667 |
| 3 six-operator FM, bank B | 11,449 | 28.6% | 13,476 | 33.7% | 41,667 |
| 4 six-operator FM, bank C | 11,457 | 28.6% | 13,352 | 33.4% | 41,667 |
| 5 wave terrain | 8,640 | 21.6% | 9,008 | 22.5% | 9,111 |
| 6 string machine | 13,285 | 33.2% | 13,443 | 33.6% | 39,498 |
| 7 chiptune | 4,269 | 10.7% | 4,872 | 12.2% | 5,941 |
| 8 virtual analog | 10,209 | 25.5% | 10,652 | 26.6% | 10,108 |
| 9 waveshaping | 7,117 | 17.8% | 7,407 | 18.5% | 7,335 |
| 10 two-operator FM | 10,346 | 25.9% | 10,498 | 26.2% | 10,341 |
| 11 grain | 10,373 | 25.9% | 11,676 | 29.2% | 10,095 |
| 12 additive | 13,233 | 33.1% | 13,327 | 33.3% | 14,135 |
| 13 wavetable | 8,760 | 21.9% | 8,860 | 22.1% | 15,997 |
| 14 chord | 11,648 | 29.1% | 13,171 | 32.9% | 12,962 |
| 15 speech | 6,710 | 16.8% | 248,267 | 620.7% | 8,006 |
| 16 swarm | 10,595 | 26.5% | 11,069 | 27.7% | 11,757 |
| 17 noise | 9,090 | 22.7% | 9,365 | 23.4% | 9,124 |
| 18 particle | 13,650 | 34.1% | 14,501 | 36.3% | 112,046 |
| 19 string | 13,345 | 33.4% | 14,524 | 36.3% | 107,401 |
| 20 modal | 13,806 | 34.5% | 13,904 | 34.8% | 15,395 |
| 21 bass drum | 9,611 | 24.0% | 9,776 | 24.4% | 9,640 |
| 22 snare drum | 10,188 | 25.5% | 10,294 | 25.7% | 10,317 |
| 23 hi-hat | 10,951 | 27.4% | 11,074 | 27.7% | 10,948 |

What the table says:

- Twenty-one engines sit between 21.6% and 34.5% of real time; the lightest is chiptune at 10.7%. The cost is flat because a floor of roughly 4k cycles per block is suspected to be the voice itself (low-pass gate, limiter, envelopes), not the oscillator; it was not isolated. The heaviest steady block in any engine other than speech is 36.3% (string and particle), so one voice of any of them needs a clock of about 58 MHz for real time. Two voices of the heaviest engine would be 73% and three 109% (worst-block basis); that is arithmetic, not a run.
- The speech engine has three blocks over real time: blocks 65, 129 and 193 (counting from 1) cost 66.5k, 105.3k and 248.3k cycles (166%, 263% and 621%). Each is the first block of a new note, after the harmonics control jumps from 0.3 to 0.5, 0.7 and 0.9 (a new word bank or synthesis path). All other 253 blocks cost 3.9k to 8.0k cycles. A product needs about 7 blocks (84 samples, 1.75 ms) of output buffer to ride through the worst one. Which part of the speech code owns the spike was not isolated.
- Switching to an engine costs one slow block in six engines: 41.7k cycles (104%) for the three six-operator FM banks, 39.5k (99%) for the string machine, 112.0k (280%) for particle and 107.4k (269%) for string. Do the switch while the output is muted or crossfaded.

Expectation written before the first run: 3k to 8k cycles for most engines and 10k to 25k for the heaviest, 24 of 24 under real time, 18 to 24 of 24 bit-exact. Twenty-one engines landed above the "most engines" range because of the 4k-cycle floor; none reached the top of the heavy range; the speech spike and the 280% particle switch were not expected; all 24 matched the Mac, the top of the bit-exact range.

## Bit-exactness

The Mac reference is the same code built with `-ffp-contract=off` and no fast-math (`host/build.sh`, `-DTEST` as upstream's own test makefile does). Each block's 12 stereo frames are CRC-32'd and compared.

| Build on the board | Engines equal to the Mac strict build (all 256 blocks) |
|---|---|
| `-O2`, FPU on, `-ffp-contract=off` | 24 of 24 |
| `-O3`, FPU on | 24 of 24 |
| `-Os`, FPU on | 24 of 24 |
| `-O2`, FPU on, `-ffp-contract=on` | 24 of 24 (the compiler fuses nothing in Plaits; the one multiply-accumulate in either image is inside the C library's `sqrtf`) |
| `-O2`, FPU off (soft float) | 4 of 24 (six-operator bank C, chiptune, grain, speech); 373 of 6.1k blocks differ in the other 20 |

The speech engine's 6.1k output samples were also read back and compared one by one: 0 differ. The Mac build gives the same CRCs at `-O0` and `-O2`, so the reference does not depend on the Mac optimizer. The soft-float build differs by small amounts in a few blocks per engine; the cause was not isolated (candidates are float-to-integer conversion and the library `sinf`, `cosf` and `tanf` that stmlib's filters call).

## Does it use the FPU

Yes. The FPU build has 10,147 single-precision FPU arithmetic and conversion instructions in the image. The FPU-off build has 8,599 call sites into the soft-float routines and none of those instructions. Taking the FPU away costs 11 to 39 times the cycles: every engine would need 149% to 1,140% of real time at 160 MHz, and the modal engine would need about 1.8 GHz. The part's FPU is what makes this run at all.

## Build level

| Build | Flash | Median share | Heaviest engine |
|---|---|---|---|
| `-O2` | 202.2 kB | 26.2% | modal 34.5% |
| `-O3` | 211.7 kB | 25.4% | string 33.0%, modal and additive 32.5% |
| `-Os` | 164.2 kB | 26.4% | modal 53.7%, additive 46.2% |

`-O3` is 3% faster in total and 4.7% bigger. `-Os` is 19% smaller and costs 1.4 times on additive and 1.6 times on modal; the other engines are within 15% of `-O2`.

## Flash and RAM

Flash at `-O2` is 202.2 kB for the whole test image: 113.0 kB of code and 88.8 kB of constant tables (`resources.cc`: wavetables, FM patches, speech data, filter and pitch tables), with no engine left out. RAM for Plaits itself is the `Voice` object, 8.7 kB, plus the 16 kB shared buffer that `plaits.cc` gives it; every engine reuses that buffer. The test image has 93.8 kB of RAM in all (76.0 kB zero-initialised data, 9.8 kB library data, an 8 kB stack reserve); 48 kB of it holds the per-block results and 12 kB the PCM buffer. Stack use peaked at 0.8 kB. There is no heap.

## Method

- Code: `third-party/plaits/`, from `pichenettes/eurorack` commit `f4cfa76` and its `stmlib` commit `e3bd7c9`, unmodified. Compile flags only. `-DTEST` is upstream's own switch for building off the module; it turns off the STM32 flash-writing code and makes user data empty.
- The note sequence: MIDI notes 48, 52, 55 and 60, harmonics 0.3, 0.5, 0.7 and 0.9, timbre 0.5, 0.6, 0.4 and 0.7, morph 0.5, 0.3, 0.6 and 0.5, decay 0.5, LPG color 0.5, timbre and morph modulation amounts 0.5, trigger patched. A fresh `Voice` is initialised for every engine.
- Clock: 160 MHz from the SYSPLL, two flash wait states, caches on (the SysConfig output of `ports/llama2c/build.sh`).
- Results sit in RAM and are read over the probe with DSLite. The image touches no pin and makes no sound.
- Not covered: sound output, the module's real parameter and CV scaling, MIDI, several voices at once, user-data (custom wavetable and speech) loading, power, and a listening test. The CRC says the board renders what the Mac renders, not how it sounds.

## Ten-minute path

Needs the MSPM33 SDK 1.04.00.00, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0 (the SysConfig output of `ports/llama2c/build.sh`, folder `gen160`) and `clang++` for the Mac reference.

```bash
cd ports/plaits
host/build.sh /tmp/plaits-host && /tmp/plaits-host/ref v > /tmp/plaits-host/ref.txt   # Mac: per-block CRCs
target-m33/build.sh O2 hard                       # board image under $OUT/plaits-O2-hard/plaits.out (flashes nothing)
cd ../.. && python3 ports/plaits/run_board.py <that plaits.out> --ref /tmp/plaits-host/ref.txt   # flash, read, compare
ports/plaits/target-m33/build.sh O2 soft          # FPU off
EXTRA=-DPL_KEEP_ENGINE=15 TAG=keep15 ports/plaits/target-m33/build.sh O2 hard   # keep one engine's PCM (also run `ref v 15` for keep.raw, then pass --keep 15 --keep-raw)
```

`run_board.py` leaves the board running whatever the next image is.

## Files

| Path | What |
|---|---|
| `third-party/plaits/` | Plaits DSP and stmlib, unmodified, pinned, license, checksums |
| `common/plaitsrun.cc`, `common/plaitsrun.h` | The note sequence and render loop (timing, CRCs), shared by the Mac and the board |
| `host/` | Mac reference build |
| `target-m33/` | Board main and build script (optimisation level, FPU on or off) |
| `run_board.py` | Flash, read back, compare every block CRC |
