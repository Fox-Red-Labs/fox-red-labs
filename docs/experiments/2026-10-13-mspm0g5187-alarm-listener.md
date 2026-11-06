<!-- experiment
id: E194
board: LP-MSPM0G5187
date: 2026-10-13
status: in progress
reached: run
line: A 13.8k-parameter model on the LP-MSPM0G5187's NPU, listening through the onboard microphone, reported a smoke alarm pattern from a laptop speaker 9.4 s after it started, kept 14 of 16 look-alike sounds silent with nothing learned, and gave 0 alarms in 3.4 hours of a quiet room; learning has run only on audio over USB, and a real smoke alarm's horn has not been tried.
stop: none
-->
# Can the LP-MSPM0G5187's NPU learn your smoke or CO alarm by ear and ignore every other sound?

**Short answer.** Partly. Through its own microphone, the board reported a smoke alarm pattern played from a laptop speaker 9.4 s after it started, kept 14 of 16 look-alike sounds silent with nothing learned, and gave 0 alarms in 3.4 hours of a quiet room. Learning your alarm (S2, then its test button) has run only on audio streamed over USB, where the board then ignored 18 of 18 look-alikes, a 31 dB quieter copy and another pitch. A real smoke alarm's horn has not been tried.

Terms: [vocab](../vocab.md) (NPU, I2S, PCM1809, dBFS, false trip). Code, build and file list: [mspm0g5187/alarm](../../mspm0g5187/alarm/README.md). Errors and fixes: [LP-MSPM0G5187 FAQ, alarm listener](../../mspm0g5187/FAQ.md). Checked 2026-10-12 and 2026-10-13 on an Intel Mac, macOS 15.7, MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, UniFlash 9.6.0 (DSLite), TI's neural network compiler (NNC) for MSPM0, on one LP-MSPM0G5187 Rev. A (probe firmware 3.0.0.38). 1 kB = 1,024 bytes.

**Question.** The board checks a sound against the alarm it learned: the same tone (within 4 %) and nearly the same loudness (no more than 10 dB quieter). You do not want to replace a working smoke or CO alarm with a connected one. Can a small board sit beside it, learn it with one press of the alarm's test button, and tell you when it goes off, without tripping on a whistle, a kettle, a ringtone or the same alarm on TV?

**Vendor position.** TI ships no alarm-sound example. The SDK's nearest example is `glass_break_detection_ai_npu`: "both feature extraction (using filterbanks) and classification are performed using AI models. The inference runs on the TinyEngine hardware accelerator". It does not listen. Its README says "The pre-loaded test vector containing vibration data is fed to the AI model" (MSPM0 SDK 2.11.00.07, read 2026-10-13). The only SDK example that reads the board's microphone is `device_uac_microphone_i2s`, which sends the audio to a computer over USB. So the microphone path, the features, the model and the learn step were written by us, built from those two examples and TI's NNC.

The patterns come from NFPA 72, the US fire alarm code: smoke is temporal-3 (three 0.5 s beeps 0.5 s apart, then 1.5 s quiet) and CO is temporal-4 (four 0.1 s pulses 0.1 s apart, then 5 s quiet). Each timing is allowed plus or minus 10 %.

**What was tried.** Expectations were written first. Two of them: under 3 ms per inference, and detection 8 to 12 s after an alarm starts.

- **Microphone.** The onboard ICS-40720 into the PCM1809 converter, I2S on PB10, PB11 and PB12 at 47.6 kHz, using TI's USB-microphone settings. The DMA interrupt filters the audio and keeps every third sample (15,873 a second).
- **Features.** Every 64.5 ms, two 512-point FFTs give 13 numbers: nine band levels, the total level, the loudest tone's frequency, how tonal it is and its level. The C code on the board and a Python twin on the Mac agree to 0.0 dB median on the same audio.
- **Model.** A five-layer 1-D CNN, 13.8k parameters, 8-bit, compiled for the NPU with TI's NNC. Every 1.03 s it looks at the last 8.3 s and answers other, smoke or CO. It was trained on 2,000 synthetic scenes: real ringtones, alert sounds, speech and music from a Mac, plus look-alike beeping (oven timers, backup beepers, microwaves, alarm clocks, alarm patterns with the timing 35 % off, chirps, whistles).
- **Decision.** An alarm needs the same answer in 6 of the last 8 inferences and a timing check against the NFPA 72 pattern. Once an alarm is learned, it also needs the learned tone and level. The learned record is 16 bytes in data flash, so it survives a power cut.
- **Three test rigs,** each closer to real life: audio streamed over USB into the same code (no sound played), a laptop speaker beside the board, and a quiet room overnight.

**How far it got.**

| Step | Result | Recorded |
|---|---|---|
| Build | `alarm_detect` links: 78.7 kB of flash (of 128 kB), 29.7 kB of RAM (of 32 kB) | Linker size, 2026-10-13 |
| NPU against the Mac | Same class as onnxruntime on 6 of 6 test windows | Self-test, 2026-10-12 |
| Microphone | Works: a 995 Hz tone came through clean; a quiet room reads -79.6 dBFS | Mic check, 2026-10-13 |
| Held-out synthetic scenes, on the Mac | 0 false trips in 2 hours; 162 of 167 alarms beside the board caught | Mac replay, 2026-10-13 |
| Audio over USB, on the board | Learned a smoke alarm and saved it; alarmed on it; 0 of 18 decoys; ignored it 31 dB quieter, at 3,600 Hz, and a CO pattern; with CO added, the learned horn reports smoke or CO and names which | USB inject runs, 2026-10-13 |
| Laptop speaker, real microphone | Smoke pattern: ALARM 9.4 s after it started. 14 of 16 look-alikes silent with nothing learned. With the alarm learned, applied afterwards to the logged answers (a replay, not a live decision), 0 of 16, and the quiet copy (about 22 dB down) and the other tone ignored | Live run, 2026-10-13 |
| Overnight, quiet room, nothing learned | 12.1k inferences in 3.4 hours, 0 alarms | Live run, 2026-10-13 |
| A real smoke alarm's horn | Not run | needs a real alarm beside the board |

The two look-alikes that alarmed with nothing learned were CO-like patterns (three pulses, and four pulses with a 1.5 s gap). **Our mistake.** The CO timing check accepted any pulse group. It now needs two groups 5.8 s apart, and both look-alikes stay silent on the board with nothing learned.

**The line.** Nothing is blocked, but learning has not run on the real microphone, and a real horn has not been tried. That is the open question. A smoke alarm is 85 dB or more at 3 m and has harmonics and sometimes a warble. The test tones were synthetic and came from a laptop speaker. If the model never calls the real horn smoke, the fix is to record it with the capture image and add it to training. That has not been needed, and it will not be known until the test runs.

**This is a first on this part.** TI has no alarm-sound example for the MSPM0G5187, so the whole path is a port of two of its examples, as the vendor position explains.

| What went wrong | Tag | Cost | Fix |
|---|---|---|---|
| The microphone read zeros | **Setup or handling** | One evening | Two radio wires on PB10 and PB11 (header J20) were holding the microphone's clock lines; taken off |
| The learned record did not save, then stopped the board at boot | **Our mistake** | About an hour, three flashes | One 64-bit write to the data flash instead of two; the sector erased back with OpenOCD |
| CO look-alikes alarmed | **Our mistake** | Half a morning, no retraining | The CO timing check now needs two groups 5.8 s apart |
| The learn press over USB landed during an alarm, which clears it | **Our mistake** | One live slot | A separate learn command |
| 20.2 ms per inference in the self-test and 32.4 ms median live, against 1 to 3 ms expected | **Unknown (suspected: Our mistake)** | None: one inference a second keeps up | Suspected: most of the time is the CPU scaling 2,048 inputs to 8 bits before the NPU, not the NPU; not profiled. Feeding 8-bit input directly is the next idea |

Effort: an evening, a night and a morning (2026-10-12 to 13); flashes were not counted; one hands-on step (two wires off J20). Verdict: good. The NPU part was the easy part. The work was in the features, the decision rule and the false-trip testing.

- Record a real smoke alarm and a real CO alarm with the capture image and add them to training.
- Doorbells: the same learn step would work for a two-tone chime, but it needs a third pattern (no standard cadence) and was not built.
- Feed the NPU 8-bit input directly, as TI's glass-break model does, to cut the 20 to 32 ms.
- Battery: the listener ran from USB. Current was not measured.
- One chip: run the model in software on the radio chip and drop the MSPM0G5187. Estimated, not built.

**Telling a phone.** The board prints `ALARM smoke` or `ALARM co` on its USB serial port. A Mac script (`host/listen.py`) turns that into a Mac notification and a Home Assistant sensor (`sensor.alarm_listener`). For a house without a Mac, the same lines go to a radio board as `SMOKE 1`, `SMOKE 0`, `CO 1` and `CO 0`:

- **Zigbee:** an LP-EM-CC2340R5 shows up in Home Assistant as a smoke sensor and a CO sensor, with no custom code on the Home Assistant side. Serial line to Home Assistant took 38 ms median for smoke and 56 ms for CO, 20 of 20 each (run).
- **Matter over Thread:** an LP-EM-CC2755P10 builds and boots as a Matter smoke and CO alarm. It has not been paired to a phone (run).
- **Matter over Wi-Fi:** not built.

None of the three radio chips has an NPU, so the listener stays on the MSPM0G5187 and talks to the radio over two wires.

**Ask.** Do you have a smoke or CO alarm whose horn sounds unlike the standard? Report which one it is. The next test is a real horn beside the board, and more kinds of alarm make it a better test.
