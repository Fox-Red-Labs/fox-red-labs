# Detect a smoke alarm's beep pattern on the LP-MSPM0G5187

It has heard only recorded alarm sounds from a laptop speaker and over USB; a real alarm was not tested.

The page: [site/project-alarm-listener.html](../../site/project-alarm-listener.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Board on USB, notification on the Mac

The tested build: one board, its USB cable, a Mac.

Status: Works. Boards: g5187.

Folders this version uses (in a sparse clone, fetch them first, then read `mspm0g5187/AGENTS.md`):

```bash
git sparse-checkout add mspm0g5187
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM0G5187 LaunchPad development kit | 1 | the listener: microphone, NPU and buttons on one board |
| A smoke or CO alarm with a test button | 1 | the alarm it learns |
| A Mac | 1 | build, flash and the notification |

### Before you start

- CCStudio 21.0.1, which brings TI Arm Clang 5.1.1, the MSPM0 SDK 2.11.00.07 and UniFlash 9.6.0. Install each in its default folder under `/Applications/ti/`, because the build script expects those paths.
- Python 3 with pyserial (`python3 -m pip install --user pyserial`).
- If you fitted a BP-CC1407P radio BoosterPack and wired it to header J20, take the PB10 and PB11 wires off. Those pins are the microphone's clock lines; with them on, the microphone reads zeros. The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`.

### Steps

1. **Mac, terminal.** Install the tools in the list above, then get a copy of this repository. To fetch only the board folder, use the sparse-checkout line in its README. The trained model is already compiled for the NPU in `mspm0g5187/alarm/model/nnc_g5187_npu/`, so no training is needed.

   You should see: `ls mspm0g5187/alarm/model/nnc_g5187_npu` lists `mod.a` and `tvmgen_default.h`.

   If `mod.a` is missing, the clone did not bring the model; it is tracked in the repository, so clone again.

2. **Mac, terminal.** Build the listener. The script copies one of TI's SDK examples for its makefile and puts the listener's code in. It links the NPU model and CMSIS-DSP, and builds in a scratch folder under your home folder.

   ```text
   cd <repository folder>
   APP=detect bash mspm0g5187/alarm/build.sh
   ```

   You should see: A size line with text 80608 and bss 30417, which is 78.7 kB of flash and 29.7 kB of RAM, then `image: mspm0g5187/alarm/out/alarm_detect.out`.

   If it cannot find `spi_controller_echo_interrupts` or `tiarmclang`, the SDK or CCStudio is not in its default folder; the two paths are at the top of `build.sh`.

3. **Bench, then terminal.** Plug the board into the Mac and read its probe serial number: it is the text after `usbmodem` without the last digit.

   ```text
   ls /dev/cu.usbmodem*
   ```

   You should see: Two lines with the same text, ending in 1 and 4, for example `/dev/cu.usbmodem<probe serial>1` and `/dev/cu.usbmodem<probe serial>4`.

   Nothing listed: the cable or adapter is charge-only; swap it.

4. **Mac, terminal.** Flash it. Make a copy of the connection file with your probe serial number in its name and inside it. The command below does both; replace `<probe serial>` with yours from step 3. Then load the image with UniFlash's command line. This is the board README's tested flash command with the listener's image in place of the blink.

   ```text
   PROBE=<probe serial>
   sed "s/<template serial>/$PROBE/g" mspm0g5187/MSPM0G5187_<template serial>.ccxml > mspm0g5187/MSPM0G5187_$PROBE.ccxml
   /Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c mspm0g5187/MSPM0G5187_$PROBE.ccxml -e -f -v -u mspm0g5187/alarm/out/alarm_detect.out
   ```

   You should see: The flash ends without an error in about 5 s, and the green LED gives a short blink every 2 seconds, which means the board is listening. The LED lines are in `detect.h`, in the main loop.

   If DSLite cannot find the probe, the serial number in the `.ccxml` does not match yours.

5. **Mac, terminal.** Start the listener's Mac script on the first serial port, the one ending in 1. It logs every line and turns an alarm into a Mac notification. Leave it running.

   ```text
   PROBE=<probe serial>
   python3 mspm0g5187/alarm/host/listen.py --port /dev/cu.usbmodem${PROBE}1 --log ~/alarm.log
   ```

   You should see: The terminal stays quiet while the room is quiet. `tail -f ~/alarm.log` in a second window shows one `I n=... cls=other` line a second.

   `ModuleNotFoundError: No module named 'serial'`: install pyserial, as in the list above. No lines in the log: check that the port ends in 1 and not 4.

6. **Bench.** Put the board beside your alarm, where it will stay. Press S2 once and the blue LED blinks fast. Then press and hold the alarm's test button until it has sounded for about 10 seconds. Ear protection helps; smoke alarms are loud up close.

   You should see: `LEARNED cls=smoke f0=... L0=... saved` in the terminal and the blue LED stops; the record is kept through a power cut. Not done with a real alarm: in these runs the learn step used alarm sound sent over USB. The 10 seconds is the expected time: four inferences that hear the pattern. It is not a measured time.

   `LEARN FAILED: no smoke or CO pattern heard in 45 s`: hold the test button longer, or move the board closer. To start over, hold S2 for 5 s until `FORGOT: generic mode`.

7. **Bench.** Press the alarm's test button again. Then press S2 once to clear the alarm.

   You should see: `ALARM smoke from the learned alarm` and the red LED on. Expected about 10 s after the sound starts; measured 9.4 s with a laptop speaker and nothing learned. The script also raises a Mac notification; these runs did not record one. S2 prints `CLEARED`.

   `IGNORED ... too quiet` means the board is farther from the alarm than when it learned; learn again in its final place ([FAQ](../../mspm0g5187/FAQ.md)).

### Expected result

Expected from the speaker and USB runs, and not seen with a real alarm: your alarm's test sound is reported as smoke, or as CO for a CO alarm, about 10 s after it starts. Beeping from a TV, a kettle, a whistle, a phone or an oven timer is not reported. Neither is the same alarm pattern played quieter or at another pitch.

### What this shows

- A 13.8k-parameter model runs on the MSPM0G5187's NPU, on its own microphone, once a second. It takes 20.2 ms per inference in the self-test and 32.4 ms median live, and keeps up in real time.
- With nothing learned it reported a smoke alarm pattern from a laptop speaker 9.4 s after it started, and stayed silent through 3.4 hours of a quiet room, 12.1k inferences.
- With an alarm learned, on audio streamed over USB and no sound played, it ignored 18 of 18 look-alike sounds. These were oven timers, backup beepers, whistles, a kettle, ringtones, speech and music. It also ignored the same alarm 31 dB quieter, and the same pattern at 3,600 Hz instead of 3,150 Hz.
- The learned-alarm rules were applied afterwards to the board's logged answers from the speaker run. They silenced all 16 look-alikes and the same alarm about 22 dB quieter, and still passed the alarm. It was a replay of logged answers; nothing was decided live.
- It tells smoke (three beeps) from CO (four short pulses) on the same horn.

### Limits

- That it learns a real smoke alarm's horn: every alarm sound was synthetic, from a laptop speaker or sent over USB. A real horn has harmonics and sometimes a warble that the training never heard.
- False alarms over weeks in a busy house: 3.4 hours of a quiet room and 2 hours of held-out synthetic scenes are what was measured.
- Doorbells: there is no standard doorbell pattern, and none was built.
- That it replaces a connected or monitored alarm. It is a demonstration and is not a listed safety device.
- On this board PB10 and PB11 are both the microphone's clock lines and the BP-CC1407P radio's reset and interrupt wires. **Setup or handling.** The listener and that radio cannot share J20 as wired here.
- The Mac script is the notification path in this version; the board on its own only lights its red LED.
- Placement matters: it learns how loud the alarm is where the board sits, and ignores anything more than 10 dB quieter. Move the board and learn again.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Model | Five-layer 1-D CNN, 13.8k parameters, 8-bit quantization-aware training, compiled with TI's NNC for the TinyEngine NPU; 16 inputs by 128 frames (8.3 s); answers other, smoke, CO once a second | `mspm0g5187/alarm/model/result.json` |
| Inference | 20.2 ms per inference in the self-test (6 of 6 classes matched onnxruntime); 32.4 ms median over 443 inferences in the live run |  |
| Audio | Onboard ICS-40720 microphone into the PCM1809, I2S at 47,619 Hz, filtered to 15,873 Hz; 13 features every 64.5 ms from two 512-point FFTs (CMSIS-DSP) | `mspm0g5187/alarm/README.md` |
| Decision | 6 of the last 8 answers agree, the NFPA 72 timing check passes, and once learned the tone is within 4 % and the level no more than 10 dB below the learned one | `mspm0g5187/alarm/README.md` |
| Image | 78.7 kB of flash of 128 kB, 29.7 kB of RAM of 32 kB, from tiarmsize on the 2026-10-13 build |  |
| Tools | MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, UniFlash 9.6.0, Intel Mac, macOS 15.7 | `docs/experiments/2026-10-13-mspm0g5187-alarm-listener.md` |
