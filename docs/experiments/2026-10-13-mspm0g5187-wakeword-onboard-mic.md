<!-- experiment
id: E22
board: LP-MSPM0G5187
date: 2026-10-13
status: in progress
reached: run
line: TI's wake-word model runs on the board's NPU from a rebuilt I2S microphone input and detected a synthetic "OK Kilby" 120 of 120 times across six macOS voices (replay through the serial port); the board's own microphone has not been heard because two CC1407P wires still sit on the microphone's clock pins.
stop: this repo
-->
# Can TI's "OK Kilby" wake-word model listen to the LP-MSPM0G5187's own microphone and light an LED?

**Short answer.** The model and the rebuilt input path work; the microphone itself is not proven. With the Mac sending synthetic "OK Kilby" to the same code over the serial port, six macOS voices were detected 20 of 20 times each (peak -20 dBFS), Samantha 10 of 10 at each of -40, -30, -10 and -3 dBFS, and the median `WAKE` line came 0.03 to 0.49 s after the end of the phrase. Ten minutes of silence gave 0 false triggers; ten minutes of other sentences gave 0 on 131 sentences without "okay" or "Kilby" and 18 in a set where three of nine contain one of them. TI's example as shipped, with nothing on its microphone pin, gave 0 detections in 11.4k inferences. The live run (the Mac's speaker, the board's microphone) is built as `run_live.sh` and waits for two wires to come off header J20.

Terms: [vocab](../vocab.md#wake-words-and-the-onboard-microphone-lp-mspm0g5187) (wake word, I2S, PCM1809, decimation, LPC, dBFS, false trigger, replay). Build errors, the microphone and the UART: the [board README's FAQ](../../mspm0g5187/wakeword/README.md#faq). Code and patch: [mspm0g5187/wakeword](../../mspm0g5187/wakeword/README.md). Checked on an Intel Mac, macOS 15.7, MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, DSLite from UniFlash 9.6.0, one LP-MSPM0G5187 Rev. A (probe <probe serial>, firmware 3.0.0.38).

**Question.** A builder wants "say the wake word, an LED lights" on the LP-MSPM0G5187, using the model TI ships for its NPU. TI's example reads a microphone this board does not carry. Can it read the one the board has?

**Vendor position.** TI's `wakeword_detection_ai_npu` README: "continuously listens for the wakeword 'OK Kilby' using audio data captured via the ADC peripheral", "Analog microphone connected to PB22 (AIN24)", "Captures audio at 8 kHz from an analog microphone connected to the ADC12 peripheral", and a detection is "6 consecutive positive predictions". Its hardware table lists the LaunchPad and an analog microphone, not a BoosterPack. PB22 is BoosterPack header pin 6, where the BOOSTXL-EDUMKII puts its microphone. The board's own microphone (an ICS-40720) feeds a PCM1809 audio converter that reaches the chip on PB10, PB11 and PB12 over I2S; TI's `device_uac_microphone_i2s` is the only example that reads it, and its README says only "send an I2S signal to the Launchpad via a I2S controller or external peripheral".

**What was tried.** Expectation first: the plan written before the board arrived said, "possible but real work: the model wants 8 kHz samples and LPC features from 20 ms frames; the PCM1809 gives 32-bit I2S at a higher rate, so the input stage needs decimation and scaling". Numeric expectations were not written before the control and replay runs (they ran in one sitting); the live-run expectations in the README table were written after replay and before any live run.

- **Control.** TI's example, three build fixes only, as the pin floats: a serial report added, otherwise as shipped (no optimization), 180 s; then with `-O2`, 118 s.
- **Port.** I2S capture by DMA from TI's `device_uac_microphone_i2s` setup, a multiplier-free decimator (CIC order 4 by 3, half-band FIR by 2, 47.6 kHz to 7,936.5 Hz), TI's model, LPC, window and detection rule unchanged, the loop pipelined, `-O2`. Everything changed is in [`changes.patch`](../../mspm0g5187/wakeword/changes.patch) and the files beside it.
- **Replay.** The same image path with the audio from the Mac over the serial port: `say -v <voice> "OK Kilby"` rendered to 8 kHz, peak scaled, sent at 8 kHz, 20 per voice, six voices, then four levels, then silence and sentences for 10 minutes each. The bytes arrive by DMA into a ring.
- **Live.** `run_live.sh`: the same image, the Mac speaking through its speaker beside the board, serial lines stamped on the Mac clock. Written, not run.

**How far it got.**

| Step | Result | Recorded |
|---|---|---|
| Build TI's example as shipped | Failed three ways, fixed in a copy | `mspm0g5187/wakeword/build.sh` |
| Control, 180 s and 118 s | 0 detections in 11.4k inferences; ADC rate 6.63 kHz; loop 27.5 ms (no optimization) and 24.1 ms (`-O2`) per 20 ms frame | Serial report, 2026-10-13 |
| Build the I2S port | Links: 68.4 kB flash, 25.6 kB RAM | Linker map |
| Run the I2S image | Runs: 7,749 Hz of audio decimated, steady for two minutes; the microphone itself read zeros with the radio wires on | Serial report, 120 s |
| Replay, six voices | 120 of 120 | Replay run |
| Replay, levels | 40 of 40 | Replay run |
| False triggers, replay | 0 in silence; 0 in 131 plain sentences; 18 in 125 sentences that include three near-miss ones | Replay runs, 10 minutes each |
| Live microphone | Not run | needs two wires off J20 |

**The line.** The model's end is clean; the microphone's end is open. What stopped the live run:

- **The wires.** PB10 and PB11 are the audio converter's clocks and, on header J20, the CC1407P radio's reset and interrupt wires. With them on, the capture read all zeros and an earlier one words with the value in the low 16 bits.
- **TI's example input, as shipped.** Even with a microphone it would not keep up: the ADC interrupt ran at 6.63 kHz, not 8 kHz, and LPC (685k cycles, 8.5 ms) then the NPU (899k cycles, 11.2 ms) take 24.1 ms a frame at `-O2` (part of it unaccounted for) against 20 ms between frames. The cause of the 6.63 kHz is not found.
- **A divider comment off by one.** TI's I2S dividers (7 and 48) give about 41.7 kHz on this chip, not the 47.6 kHz its source says; with 6 the count was 7,749 Hz after the by-six decimation, 2.4 percent under the 7,936.5 Hz target.
- **Interrupts and the NPU.** The first replay used a receive interrupt and lost bytes (245 overrun errors in 1,400 frames) while the NPU ran; one lost byte garbles everything after it, and the first set of results was thrown away. DMA into a ring fixed it. The first build used an interrupt. Why the interrupts were late is **Unknown**. The counts were observed 2026-10-13 and not saved.
- **The 60-tap filter.** The first decimator, a 60-tap FIR in the I2S interrupt, took 44k cycles of every 1 ms because a multiply takes about 30 cycles on this core. The CIC and half-band version takes about 8.4k (the 60-tap figure was observed, not saved).

**No vendor example for this input path.** TI ships the model, the example for the analog microphone, and the I2S capture for a USB microphone, nothing that joins them. What existed: the three examples. Method: kept TI's `lpc_float`, `feature_extraction.c`, `buffer_manager.c`, `post_inference.c` and `model.a` unchanged, wrote the input stage and the loop, and checked the result with replayed speech first. What went wrong:

| Problem | Cause tag | Cost | Fix |
|---|---|---|---|
| Example did not build with the installed compiler | Tool or docs | three failed builds | `build.sh` |
| Decimation filter used half the CPU | Our mistake | one measured run | CIC plus half-band |
| Receive interrupt lost bytes, first results invalid | Our mistake | the first replay set | DMA ring |
| I2S rate wrong by one divider | Tool or docs | one measurement | divider 6 |
| Microphone silent with radio wires on | Unknown (suspected: the wires) | the live run | two wires off J20 |

Effort: one sitting at the bench with no hands needed until the wires. Verdict: the model side took one rebuilt input stage and gave 120 of 120 on synthetic speech, which is worth a real-speaker test; TI's example as shipped is not usable from a terminal and not real time at `-O2`; the microphone side is untested, and whether the wires are the whole story is not known.

**Ask.**

- **For readers:** if you run the rebuilt image with a real speaker, send the hit rate and the room.
