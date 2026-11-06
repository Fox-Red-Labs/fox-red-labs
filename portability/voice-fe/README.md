# Voice front end: four-mic beamformer, echo canceller and noise suppressor on a Cortex-M33

A four-mic delay-and-sum beamformer followed by SpeexDSP's echo canceller (200 ms tail, 3,200 taps) and noise suppressor needs about 1.5 million cycles per 10 ms frame on a Cortex-M33: 9.2 ms of every 10 ms at 160 MHz on the LP-MSPM33C321A and 13.3 ms on the LP-EM-CC35X1, where code runs from external flash. The output matches a Mac build of the same code bit for bit on both boards.

Terms: [docs/vocab.md](../../docs/vocab.md). ERLE (echo return loss enhancement) is how much quieter the echo is after the canceller, in dB. 1 kB = 1,024 bytes.

## Results, 2026-10-26

32 s of 16 kHz audio (a 4 s clip played 8 times), 10 ms frames, TI Arm Clang 5.1.1.LTS, `-mcpu=cortex-m33 -mfloat-abi=hard -mfpu=fpv5-sp-d16 -ffp-contract=off`, SpeexDSP 1.2.1 fixed point. Rows: `data/results/mspm33c321a.csv` and `data/results/cc35x1.csv`, tests `voice_fe_*`.

| Board and build | Mean ms per frame | Core load | Worst ms | Beamformer, canceller, suppressor (ms) |
|---|--:|--:|--:|---|
| LP-MSPM33C321A, -O2, internal flash | 9.42 | 94 % | 9.91 | 0.42, 6.84, 2.16 |
| LP-MSPM33C321A, -O3, internal flash | 9.21 | 92 % | 9.66 | 0.46, 6.64, 2.11 |
| LP-MSPM33C321A, -O3, 100 ms tail (1,600 taps) | 7.70 | 77 % | 7.97 | 0.46, 5.13, 2.11 |
| LP-EM-CC35X1, -O2, external flash | 13.31 | 133 % | 14.12 | 0.34, 9.86, 3.12 |
| LP-EM-CC35X1, -O3, external flash | 13.28 | 133 % | 14.04 | 0.41, 9.84, 3.03 |

The CC35X1 row with the hot code copied into the chip's 32 kB code RAM is in `cc35x1/RESULTS-cc35x1.md`.

Memory, both boards: SpeexDSP heap high-water 85.6 kB (67.8 kB of it the echo canceller), 2.4 kB of beamformer and frame buffers, 1.6 to 1.9 kB of stack: about 90 kB. Code and constants 48.9 kB at -O2 and 54.1 kB at -O3 (linker map; the 640 kB test signal and the harness are not counted).

Output check: every pass's CRC-32 of the canceller output and of the final output, and three energies, equal the Mac build of the same sources (8 of 8 passes, every run on both boards). ERLE on passes 2 to 8: 26.3 dB from the canceller, 38.6 dB with the suppressor; the near-end noise caps it at 31.3 dB (echo-to-noise ratio at the beamformer output).

## The test signal

`gen_signal.py` (numpy, seed 20261026) makes a 4 s clip: far-end speech-like noise (formant-filtered noise in syllable bursts, RMS 3,000 of 32,767), its echo through a fixed room impulse response (2 ms direct path, 30 ms decay, 190 ms long, echo RMS 1,500 at the mics), reaching the four mics with 0.2 to 0.8 sample delays, plus a low-passed noise source from the look direction (0, 1.25, 2.5 and 3.75 samples) and independent noise at each mic. The clip is built circularly, so it loops with no seam. SHA-256 of the generated `fe_data.c`: `0257ba010bb9d3f7ab3ff476ba8639e4c6ef095d81da5ce6ae7e7419254ddb4a`.

## Files

| File | What it does |
|---|---|
| `beamformer.c`, `.h` | Delay-and-sum over 4 mics, a 16-tap Q15 fractional-delay filter per mic |
| `fe_run.c`, `.h` | The harness: per frame, beamformer, `speex_echo_cancellation`, `speex_preprocess_run` (noise suppression on, residual echo suppression from the canceller), each stage timed; CRCs, energies, histogram |
| `fe_arena.c`, `speex_cfg/` | SpeexDSP settings (fixed point, KISS FFT) and a static arena behind its allocator, so the heap high-water is exact |
| `gen_signal.py` | The test signal as C arrays |
| `build_host.sh`, `host_main.c` | The Mac reference run |
| `report.py` | Board log against the Mac run: bit-exact check, ms per frame, ERLE |

Board glue: `cc35x1/bench/voicefe/` (FreeRTOS thread, SysTick timing, report on UART2) and `mspm33c321a/bench/voicefe/` (bare metal, DWT timing, report read back over the probe). Library: `third-party/speexdsp/`.

## Run it

```bash
portability/voice-fe/build_host.sh <out>/host
mspm33c321a/bench/voicefe/build.sh O3
python3 mspm33c321a/tools/run_image.py <out>/voicefe-O3/voicefe.out 240 fe_log:6000 --text fe_log --serial <probe> --until fe_done=0x46454430 > m33.txt
python3 -I portability/voice-fe/report.py m33.txt --host <out>/host/host.log --sig <out>/host/sig.npz
cc35x1/bench/voicefe/build.sh O2 && cc35x1/bench/voicefe/run.sh O2 3
```
