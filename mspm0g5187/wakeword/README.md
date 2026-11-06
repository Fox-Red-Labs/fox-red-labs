# "OK Kilby" wake word on the LP-MSPM0G5187 with TI's NPU model and the onboard microphone

The model works on this board; the board's own microphone has not been heard. TI's `wakeword_detection_ai_npu` listens to an analog microphone on header pin 6, which is the BOOSTXL-EDUMKII BoosterPack's and not on this LaunchPad, so as shipped nothing on the LP-MSPM0G5187 can wake it. The audio input of the example was moved to the board's I2S microphone and kept TI's model, features and detection rule. With the Mac speaking "OK Kilby" into the same code through the serial port, six macOS voices got 120 of 120 detections, 10 minutes of silence gave 0 false triggers, and the first `WAKE` line came a median 0.03 to 0.49 s after the end of the phrase by voice (some single hits land slightly before the end). The live microphone run is built and waits for two wires to come off header J20.

Terms: [docs/vocab.md](../../docs/vocab.md). CIC and half-band filters are the two cheap low-pass stages described under "What decimation was done"; the NPU is the chip's neural processing unit; a ring is a buffer that wraps around. Board questions: [FAQ](../FAQ.md). 1 kB = 1,024 bytes.

Checked 2026-10-13 on an Intel Mac, macOS 15.7, MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, DSLite from UniFlash 9.6.0, one LP-MSPM0G5187 Rev. A (probe <probe serial>, firmware 3.0.0.38). Contents: summary of results, what TI's example does, what was changed, the control, the results, limits, FAQ, run it yourself. Written up as an experiment in [docs/experiments](../../docs/experiments/2026-10-13-mspm0g5187-wakeword-onboard-mic.md).

## Summary of results

TI's example expects an analog microphone this board does not have, so the audio input was moved to the board's I2S microphone chain and the input stage decimates the 47.6 kHz stream to about 8 kHz. Everything below used TI's model unchanged.

| Question | Measured |
|---|---|
| Does TI's example, as shipped, react on this board with no microphone? | No: 0 detections in 6.5k inferences (180 s, built as shipped) and 4.9k (118 s, built with `-O2`), the pin floating |
| Does the input stage work at the model's 8 kHz? | The I2S clock chain delivers 7.75 kHz of decimated samples (the microphone itself gave zeros), 2.4 percent under the 7,936.5 Hz target; TI's own ADC input delivers 6.63 kHz |
| Does the unchanged model hear a synthetic "OK Kilby"? | Yes: 20 of 20 for each of six macOS voices at a peak of -20 dBFS, and 10 of 10 for Samantha at each of -40, -30, -10 and -3 dBFS |
| False triggers, 10 minutes of silence (replay) | 0 |
| False triggers, 10 minutes of other sentences (replay) | 0 on 131 sentences without "okay" or "Kilby"; 18 in 125 sentences of which three kinds contain one of them |
| Detection time after the end of the phrase (replay) | Median 0.03 to 0.49 s by voice |
| Image size | 68.4 kB flash, 25.6 kB RAM of 128 kB and 32 kB (TI's shipped image: 61.2 kB and 25.0 kB) |
| Live microphone, 20 utterances per voice | Not run |
| Live microphone, false triggers in silence and other sentences | Not run |

## What TI's example does

- **Wake word:** "OK Kilby", a TCDResNet model compiled for the NPU (`model/model.a`, input 100 x 70 int8, two outputs).
- **Audio in:** an analog microphone on PB22 (ADC channel 24), sampled by a timer-triggered ADC with 8-fold hardware averaging, meant to run at 8 kHz.
- **Features:** every 160 samples (20 ms) a software LPC (linear predictive coding) routine makes 70 numbers; the last 100 frames, 2 seconds, are the model's input.
- **Decision:** the model's class 0 above class 1 for 6 frames in a row (about 120 ms) lights the blue LED on PB13.

## What was changed

The patch is [`changes.patch`](changes.patch); the other files are added or replaced. [`build.sh`](build.sh) copies TI's example out of the SDK, applies everything, and builds it from the terminal.

| Change | Why |
|---|---|
| New [`data_collection.c`](data_collection.c): I2S capture from the microphone chain (PB10 word clock, PB11 bit clock, PB12 data) by DMA, from TI's `device_uac_microphone_i2s` setup; TI's `clockConfig()` kept | The example's input is not on this board |
| [`frl_report.c`](frl_report.c): filter and decimate 47.6 kHz to 7,936.5 Hz (no multiplier in the fast part), scale to the 14-bit code the example's `buffer_manager.c` expects, serial report, held green LED, replay input | The model needs 8 kHz; 7,936.5 is the nearest whole division and is 0.8 percent slow |
| [`wakeword_i2s.syscfg`](wakeword_i2s.syscfg): I2S, DMA, UART, SysTick and LEDs; clocks as the example; I2S bit clock divider 6 | TI's divider 7 gives 41.7 kHz on this chip, not the 47.6 kHz its comment says |
| Main loop pipelined: the NPU starts on the window as it stands and the CPU computes the next frame's LPC while it runs | TI's loop does LPC (8.5 ms) then NPU (11.2 ms) and a frame takes 24.1 ms (the other 4.4 ms is not broken down) but arrives every 20 ms; the microphone does not wait |
| `-O2` | The shipped makefile sets no optimization |
| Three build fixes: pointer-type warning, the `npu.c` path, the CMSIS-DSP library | The example does not build as shipped with this toolchain |

The model, `lpc_float/`, `feature_extraction.c`, `buffer_manager.c` and `post_inference.c` are TI's files, unchanged. The window shift in the pipelined loop is two block copies in place of TI's byte loops; same result.

**What decimation was done.** The I2S words come at 47.6 kHz (one 32-bit slot, 18 bits kept). Stage 1 is a 4th-order CIC filter (running sums, adds only) that keeps every third sample, 15.9 kHz. Stage 2 is a 31-tap half-band FIR that keeps every second, 7.94 kHz. A 60-tap FIR in the interrupt used 44k cycles of every 1 ms (observed 2026-10-13, not saved), because the chip's Cortex-M0+ multiplies in about 30 cycles. [`make_fir2.py`](make_fir2.py) prints the response: -0.2 dB at 1 kHz, -2 dB at 3 kHz, -4 dB at 3.5 kHz, and at least 20 dB down in the bands that fold into 0 to 3.5 kHz. The model was trained on TI's analog microphone and ADC, whose response is not known.

## Control: TI's example as shipped, nothing on the microphone pin

`MODE=shipped` builds TI's files with only the three build fixes; `MODE=adc` adds the serial report. Both listen to pin PB22, left floating.

| Run | Result |
|---|---|
| `adc`, no optimization (as the makefile ships), 180 s | 0 detections in 6.5k inferences; largest class margin -138; 36.3 frames a second, 27.5 ms a frame |
| `adc` with `-O2`, 118 s | 0 detections in 4.9k inferences; ADC interrupt 6.63 kHz; 41.4 frames a second, 24.1 ms a frame; NPU 899k cycles (11.2 ms at the measured 80.4 MHz), LPC, quantize and window shift 685k cycles (8.5 ms) |

The floating pin reads a constant offset (mean level 29.4k of 32.8k), so the model sees a steady value, not noise. The LED stayed off. `MODE=shipped` (TI's files, three build fixes, no serial output) was built, not flashed. The README says the ADC interrupt fires at 8 kHz; 6.63 kHz was counted against the Mac clock. Cause not found.

## Results: the Mac speaks, the board hears

Two inputs, one image path. **Replay** sends 8 kHz samples from the Mac to the board's serial port (`wakeword_replay`): the model code is the same, the audio is exact and repeatable, and the Mac's speaker, the room and the microphone are out of it. **Live** is the Mac's speaker next to the board and the board's own microphone (`run_live.sh`). Replay is done; live waits for the wires.

Replay, `say -v <voice> "OK Kilby"` rendered to 8 kHz, peak scaled to -20 dBFS, 20 per voice, 3 s apart, a hit being a `WAKE` line from the word's start to 1.5 s after its end:

| Voice | Hits | Median `WAKE` time after the end of the phrase |
|---|--:|--:|
| Samantha | 20 of 20 | 0.18 s |
| Daniel | 20 of 20 | 0.03 s |
| Fred | 20 of 20 | 0.36 s |
| Karen | 20 of 20 | 0.49 s |
| Kathy | 20 of 20 | 0.44 s |
| Ralph | 20 of 20 | 0.38 s |

Level, Samantha, 10 each: 10 of 10 at -40, -30, -10 and -3 dBFS. Each voice also produced extra `WAKE` lines outside the hit window (Daniel 20, Samantha 12, Kathy 9, Fred 5, Ralph 2, Karen 0 over the 20 utterances). The extra lines are second `WAKE` lines 1 to 2 s after the first (all six of the Daniel utterances logged with times had one): the detection fires again as the word moves through the two-second window. A device that acts on `WAKE` should ignore repeats for 2 s.

False triggers, replay, 10 minutes each at a peak of -20 dBFS:

| Sound | `WAKE` lines |
|---|--:|
| Silence plus a -60 dBFS noise floor | 0 |
| 131 sentences that contain neither "okay" nor "Kilby", three voices | 0 |
| Nine sentences including "Okay, let's get started with the meeting" and "Kilby was an engineer": 125 sentences, three voices | 18 lines, about 9 detections (lines come in pairs 5 s apart), all in the three near-miss sentences ("Okay, let's get started", "Hey, okay, what time", "Kilby was an engineer"); each line was matched to its sentence by time (table not saved) |

Size and speed: 68.4 kB flash and 25.6 kB RAM for the microphone image (TI's example as shipped: 61.2 kB and 25.0 kB). Serial lines are `WAKE n=<count> inf=<frame> margin=<class0 minus class1>` on detection and a status line a second.

## Limits

- **No live microphone result.** The microphone path is built and unproven on this board. With the CC1407P radio's wires on J20 the I2S capture read all zeros (this run) or garbled words (an earlier run); PB10 and PB11 are the converter's clocks and the radio's reset and interrupt lines. The run is `run_live.sh`; it needs those two wires off and the J6 jumper on (it ships fitted).
- **Synthetic speech.** The model is 100 percent on six macOS voices and was trained on people. A hit rate on real speakers, in a room, from a speaker, was not measured.
- **Level sensitivity not shown.** Replay held its hits from -40 to -3 dBFS; a real microphone's level is `MIC_GAIN` in the build and has not been set.
- **Sample rate.** The I2S clock chain delivers 7,749 Hz (counted against the Mac clock over 120 s, with the microphone silent) where the model expects 8,000 Hz: 3 percent under. TI's own ADC input runs at 6.63 kHz, 17 percent under 8,000 Hz, and the shipped loop is slower than its stream.
- **LED.** The green LED is set in the same function that prints `WAKE`, and TI's code drives the blue one; that is a reading of the code, not a test. Neither LED was seen.
- **Headroom.** The pipelined loop keeps up with the microphone's stream (a steady backlog of 120 samples over two minutes, 5,800 frames); its largest frame rate was not measured, because the serial replay link tops out first.
- **One board, one compiler.** Rev. A, TI Arm Clang 5.1.1.LTS.

## FAQ

### I flashed TI's `wakeword_detection_ai_npu` and "OK Kilby" never lights the LED. Is the model broken?

No. The example reads an analog microphone on PB22, which the LP-MSPM0G5187 does not have; the BOOSTXL-EDUMKII BoosterPack does. With the pin floating the model sees a constant value and says nothing. Stack the BoosterPack, or build the I2S version here.

### The example fails to build: `incompatible pointer types passing 'int8_t[1][2]'`, then `No rule to make target '/home/developer/.cache/bazel/...npu.c'`, then `undefined symbol arm_dot_prod_q15`

Three separate breaks in the shipped makefile and source with TI Arm Clang 5.1.1.LTS. [`build.sh`](build.sh) applies the three fixes to a copy: `-Wno-incompatible-pointer-types`, the `npu.c` rule pointed at the SDK, and `arm_cortexM0l_math.a` added to the link.

### Does the onboard microphone work if the CC1407P radio is wired to J20?

No audio came through in the capture. PB10 and PB11 are shared with the converter's clocks. Take the radio's orange (PB10) and purple (PB11) wires off J20 before using the microphone.

### My UART receive loses bytes while the NPU runs

Use DMA. A receive interrupt at 230,400 baud raised 245 overrun errors in 1,400 frames (observed 2026-10-13, not saved); with DMA into a ring, none.

## Run it yourself

All from `mspm0g5187/wakeword/`. The first two build and flash.

```text
MODE=replay bash build.sh
```

```bash
<UniFlash>/dslite.sh --mode flash -c <your .ccxml> -e -f -v -u out/wakeword_replay_O2.out
```

```bash
./run_replay_all.sh
```

`run_replay_all.sh` takes about 45 minutes (six voices, four levels, two 10-minute false-trigger runs). For the microphone, take the radio wires off J20, then:

```bash
bash run_live.sh
```

`run_live.sh` builds `MODE=i2s`, checks that the microphone is not silent, then has the Mac speak "OK Kilby" 20 times in each of two voices at volume 70, and runs 10 minutes of silence and 10 of other sentences. Other builds: `MODE=adc` (TI's input plus the report), `MODE=shipped` (TI's example with only the build fixes); `OPT=-O2` for optimization on those.

Files: `build.sh`, `data_collection.c`, `frl_report.c`, `frl_report.h`, `wakeword_detection_ai_npu.c` (TI's main with the changes), `wakeword_i2s.syscfg`, `wakeword_adc.syscfg`, `replay_uart_dma.syscfg`, `fir_hb.h`, `make_fir2.py`, `changes.patch`; test tools `replay.py`, `replay_long.py`, `live.py`, `ser.py`, `stats.py`, `summary.py`.
