# BOOSTXL-AUDIO: a tone and a four-voice tune from a TI BoosterPack, on a plain Arm LaunchPad

Code for the BOOSTXL-AUDIO (a DAC8311 digital-to-analog converter, a TPA301 speaker amplifier, a speaker and a microphone on one plug-on board). TI's own software for it targets the MSP430FR5994 and no current part. Status: **Works** on the LP-AM13E230 (1 kHz tone and a tune, checked with the pack's own microphone); **Should work** (builds, not run on the board) for the LP-MSPM0G3519 and LP-MSPM33C321A.

Terms: [docs/vocab.md](../../../docs/vocab.md). 1 kB = 1,024 bytes. **DAC** is a digital-to-analog converter. **SYNC** is the DAC8311's chip-select input. **Frame** is one complete write to the DAC: 16 clock pulses with SYNC held low.

## What is in the folder

| File | What it does |
|---|---|
| `core/ba_dac8311.c`, `.h` | The DAC8311 frame: 2 power-down bits then a 14-bit code, SYNC low for all 16 clocks. Also the wrong frame (two 8-bit transfers with SYNC raised between them) used to test why a pack only clicks. |
| `core/ba_chip.c`, `.h` | A four-voice synth: two pulse voices, a triangle and a noise voice for drums. A song is rows of four notes. |
| `core/ba_tone.c`, `.h`, `ba_sine_table.h` | A sine tone from a 256-entry table. At 8 kHz a phase step of 8,192 is exactly 1 kHz. |
| `core/ba_mic.h` | Mean, RMS and range of a block of microphone samples. |
| `songs/fox_run.tune`, `tools/tune2h.py`, `songs/fox_run.h` | An original 12.8 second tune in a plain text note format, and the converter that turns it into a C table. |
| `app/audio_app.c`, `app/board_audio.h` | The test app. One image, driven by single keys over the serial port. A board supplies eleven small functions (pins, a sample tick, the microphone ADC, the console). |
| `boards/am13e230/` | The LP-AM13E230 port: `audio.syscfg`, `main.c`, `build.sh`. Ran. |
| `boards/mspm33c321a/`, `boards/mspm0g3519/` | The same layer for the other two kits. Build only. |
| `halloween/` | The Halloween show: four songs, shuffle, sound effects and modes, light themes, buttons; runs on the LP-MSPM33C321A and LP-AM13E230. See its README. |
| `algos/` | Portable integer C for the pack's microphone: `ba_yin` (guitar tuner), `ba_fft` (128-point spectrum, 16 bars), `ba_dtmf` (Goertzel keypad decoder), `ba_gate` (noise gate and echo). No floating point, no library calls. |
| `app/algos_app.c` | The same UART-driven app as `audio_app.c`, with the algorithms on the live microphone and a `b` command that prints cycle counts. Build with `boards/<board>/build.sh algos`. |
| `host/test_algos.c`, `host/run_algos.sh` | The Mac test for the algorithms against double-precision references, on synthetic and recorded signals. |
| `host/test_host.c`, `host/run.sh` | The Mac test: a model of the DAC8311 input stage checks every frame, then renders `out/tone_1khz.wav` and `out/fox_run.wav`. |
| `tools/mic_analyze.py` | Reads the microphone captures the app prints and reports mean, RMS, dominant frequency and the share of energy at it. |
| `tools/am13_session.py` | The whole bench run on the LP-AM13E230: back up flash, flash, run the test matrix, restore, read back. |

## Pins (BoosterPack numbers, SLAU670A Figure 12)

| Pack signal | BoosterPack pin | LP-AM13E230 | LP-MSPM0G3519 | LP-MSPM33C321A |
|---|---|---|---|---|
| DAC8311 SCLK | 7 | PB5 | PB31 | PB31 |
| DAC8311 SYNC | 12 | PB12 | PB11 | PB29 |
| DAC8311 DIN | 15 | PB10 | PB8 | PB8 |
| Amplifier shutdown line | 13 | PB18 | PB12 | PB28 |
| Microphone power | 5 | PA3 | PB29 | PA21 |
| Microphone output (ADC) | 6 | PA31 (AIN33) | PA25 (ADC0 channel 2) | PA22 (HSADC0 channel 7) |

Jumper J5 on the pack picks what drives the amplifier: the SPI DAC (default, used here), the LaunchPad kit's DAC on pin 30, or PWM on pin 19. The SPI lines are driven as plain GPIO so the frame is exactly what the core writes.

## Try it in ten minutes (a Mac, no board)

```bash
portability/boosterpacks/audio/host/run.sh      # builds and runs the test, writes host/out/*.wav
open portability/boosterpacks/audio/host/out/fox_run.wav
```

## Build for a board (nothing is flashed)

```bash
portability/boosterpacks/audio/boards/am13e230/build.sh [autoplay]
portability/boosterpacks/audio/boards/mspm0g3519/build.sh
portability/boosterpacks/audio/boards/mspm33c321a/build.sh
```

Flash and RAM, TI Arm Clang 5.1.1.LTS at -O2: LP-AM13E230 16.9 kB and 13.3 kB (RAM as the linker map reports it; the 2k-sample capture buffer alone is 4 kB), LP-MSPM0G3519 8.9 kB and 4.4 kB, LP-MSPM33C321A 10.3 kB and 4.4 kB.

## Use it

Open the board's serial port at 115200 and press one key at a time.

| Key | Does |
|---|---|
| `1` | 1 kHz tone, correct 16-bit frames |
| `2` | the tune "Fox Run", looping |
| `8` | 1 kHz tone with the wrong frame (the DAC never updates) |
| `0` | silence |
| `a`, `A`, `z` | amplifier line driven low, driven high, left floating |
| `m`, `M` | microphone power on, off |
| `c` | capture 2k microphone samples at 8 kHz and print them |
| `v`, `V` | volume down, up (0 to 16) |
| `p` | print the state |

The amplifier only plays with the line driven low (key `a`). Floating or high leaves it in shutdown. Details and the measurements: [the frame and amplifier experiment](../../../docs/experiments/2026-10-20-boostxl-audio-frame-and-amp.md).

## The note file

`songs/fox_run.tune` is plain text. `tempo N` sets beats per minute (one row is an eighth note); `vol a b c d` sets the four voice volumes, 0 to 15. Every other line is one bar: four columns separated by `|` (lead pulse, harmony pulse, triangle bass, drums). Notes are names from C2 to B7 with `#` for sharp (A4 is 440 Hz), `-` holds the note, `.` is silence. Drums are `k` kick, `s` snare, `h` hat. `python3 tools/tune2h.py songs/your.tune songs/your.h your_name` writes the C table.

## Algorithms on the microphone (host-tested and built; no board run)

Four pieces of signal processing sit on the 8 kHz microphone stream and the DAC path. All are integer C, tested on a Mac against a double-precision reference (`host/run_algos.sh`) and cross-built for the LP-AM13E230 and LP-MSPM33C321A. A board run needs only a flash of the `algos` image.

| Piece | What it does | Host result | Flash code | Work per call |
|---|---|---|---|---|
| Guitar tuner (`ba_yin`) | YIN pitch detection on a 384-sample frame (48 ms), nearest note and error in tenths of a cent | Clean sine 82 to 500 Hz within 0.6 cent, 500 to 1,000 Hz within 2.3 cents; with 2nd and 3rd harmonics within 1.6 cents to 500 Hz and 9.5 cents to 1 kHz; with 20 dB noise within 5.8 cents; no octave errors in 456 tests; within 1 cent of the double reference | 2.4 kB, 2 kB of RAM | 32,512 multiply-accumulates |
| Spectrum (`ba_fft`) | 128-point FFT, Hann window, 16 log-spaced bars of 0 to 127 pixels (8 pixels an octave) for a 128x128 screen | Within 1.3 pixels of a double DFT for bars above 30 pixels; the 16-bit rounding floor shows below that (up to 10.7 pixels error above 10 pixels); the recorded 1 kHz tone peaks in the bar for bins 16 to 19 | 4.4 kB | 1,792 multiplies |
| DTMF (`ba_dtmf`) | Eight Goertzel filters, 205-sample blocks, debounced digit stream | All 16 keys decode; a 16-digit sequence decodes exactly down to 2 dB SNR and breaks at 1 dB; relaxing the energy rule to a quarter moves the break to -1 dB (half wrong at -3 dB); no false digit from noise alone in 20,000 blocks | 1.8 kB | 1,640 multiplies a block |
| Gate and echo (`ba_gate`) | Peak-follower gate with hold and a click-free gain ramp; one delay line with feedback | Closed gate reaches digital silence, open gate +0.00 dB, opens within 1.2 ms; echo repeats equal the double result; through a DAC8311 model the output is within 3 of the digital value | 0.3 kB, echo line 4 kB for 250 ms | under 10 operations a sample |

SNR here is signal power over white-noise power across 0 to 4 kHz. Cycle counts on the host are multiply counts and microseconds on a Mac (22 us for a tuner frame); they are not Cortex-M33 cycles. The `b` command of the algos image prints DWT cycle counts per call once the image runs on a board.

Build sizes (variant `algos`, -O2, SysConfig 1.28.0, TI Arm Clang 5.1.1.LTS):

| Board | Flash | RAM |
|---|---|---|
| LP-AM13E230 | 29.6 kB | 22.1 kB |
| LP-MSPM33C321A | 23.1 kB | 13.4 kB |

To run it on a board (microphone on pin 6, amplifier enable pin 13 low, as for the tone test): build, flash, open the serial port at 115200, then `m` (microphone power), `a` (amplifier on), `1` (440 Hz tone), `t` (tuner reads about A4), `d` (DTMF digit sent and decoded), `s` (spectrum bars), `g` (live gate and echo), `b` (cycle counts). Keys are listed at the top of `app/algos_app.c`.

Limits: the tuner needs a 48 ms frame, so it follows a held note, not a pluck attack; the spectrum bars below about 30 pixels are rounding noise; the DTMF decoder was tested with white noise only, not speech or music; the gate and echo have not been heard on the speaker.

## Limits

- The synth runs at 8 kHz, so nothing above 4 kHz plays; the top note in the tune is G6 (1,568 Hz) and the pulse voices' harmonics above 4 kHz fold back.
- With the speaker beside the microphone the ADC clips from volume 8 up (30 of 2k samples at volume 4, 942 at volume 8); use volume 4 or lower for a level measurement.
- The SPI lines are bit-banged; a hardware SPI port with 16-bit frames and a software SYNC line would also work and is not built.
- Pin 30 (LaunchPad DAC) and pin 19 (PWM) positions of J5 are not tested.
