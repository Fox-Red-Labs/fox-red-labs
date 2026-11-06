# Audio BoosterPack tone and tune on a LaunchPad

Plug a BOOSTXL-AUDIO onto an LP-AM13E230. A key press plays a 1 kHz tone or a four-voice tune through the pack's DAC and speaker, and the pack's microphone reads the pitch back.

The page: [site/project-audio-boosterpack.html](../../site/project-audio-boosterpack.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Tone and tune on the LP-AM13E230

Test the DAC code on the Mac, build the app, plug the pack on, flash, and press keys over the serial port.

Status: Works. Boards: am13e230.

Folders this version uses (in a sparse clone, fetch them first, then read `portability/boosterpacks/AGENTS.md`):

```bash
git sparse-checkout add portability/boosterpacks am13e230 tools mac-setup
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-AM13E230 LaunchPad development kit | 1 | runs the app and drives the pack's DAC |
| BOOSTXL-AUDIO audio BoosterPack plug-in module | 1 | the DAC8311, amplifier, speaker and microphone |

### Before you start

- A Mac with CCStudio 21.0.1 (for `DSLite`), TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0 and the AM13E2x SDK 26.01.00.03 in the default `/Applications/ti` folders (`mac-setup/README.md`).
- The code: `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git`, then `git sparse-checkout add portability/boosterpacks am13e230 tools mac-setup`. Every command on this page runs from the repository folder.
- The board ships with empty flash. If it holds your own program, back it up first with the command in `am13e230/AGENTS.md`.

### Steps

1. **Mac, terminal.** Build the DAC and synth code for the Mac. A model of the DAC8311 input stage checks every frame, then the test renders the tone and the tune to WAV files. No board is needed.

   ```text
   portability/boosterpacks/audio/host/run.sh
   open portability/boosterpacks/audio/host/out/fox_run.wav
   ```

   You should see: A `host/out` folder holds `tone_1khz.wav` and `fox_run.wav`.

   If the compiler is missing, install the Xcode command line tools and run the line again.

2. **Mac, terminal.** Build at -O2. Nothing is flashed. The image is 16.9 kB of flash.

   ```text
   portability/boosterpacks/audio/boards/am13e230/build.sh
   ```

   You should see: The script leaves `audio.out` under `out/boosterpack-speedrun/am13-audio-default/`.

   If the SDK is not found, install it as `mac-setup/README.md` says and run the line again.

3. **Bench, then Mac terminal.** Plug the BOOSTXL-AUDIO onto the LaunchPad. Copy `am13e230/AM13E23019_<probe serial>.ccxml` to `am13e230/AM13E23019_<serial>.ccxml` with your probe serial, then flash.

   ```text
   /Applications/ti/uniflash_9.6.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite flash -c am13e230/AM13E23019_<serial>.ccxml -e -f -v out/boosterpack-speedrun/am13-audio-default/audio.out
   ```

   You should see: DSLite ends with `System Reset is issued.` and `Running...`.

   If it then prints `Failed: Target is not halted`, that is the normal end of a flash on this board: the program is running.

4. **Mac, terminal.** Open the board's serial port at 115200 baud (`/dev/cu.usbmodem<serial>1`). Press `a` to drive the amplifier line low, then `1` for the 1 kHz tone. `0` is silence.

   You should see: A clear 1 kHz tone from the pack's speaker.

   If it stays silent, press `a` first: the amplifier plays only with its line driven low. Key `8` sends the wrong frame on purpose and is silent in every amplifier state.

5. **Mac, terminal.** Press `2` for the four-voice tune "Fox Run", which loops. Press `m` to power the microphone, then `c` to capture 2k samples at 8 kHz. Keep the volume at 4 or lower (`v` lowers it) for a level reading. `tools/mic_analyze.py` reads the printed capture and reports its dominant frequency.

   You should see: The tune plays. A capture taken during the tone reads a dominant frequency of 1000.0 Hz.

   If the capture is pinned at its top values, the microphone clipped: lower the volume. With the speaker beside the microphone it clips from volume 8 up.

6. **Mac, terminal.** Run the tuner, spectrum bars, DTMF decoder and noise gate against double-precision references, on synthetic and recorded signals.

   ```text
   portability/boosterpacks/audio/host/run_algos.sh
   ```

   You should see: The run ends with every check passing; the log is `results/host-algos-20261020.log`.

   If a check reports a difference larger than the README's table, do not build the apps image until it is understood.

7. **Mac, terminal.** Build the image with the tuner, spectrum, DTMF decoder and gate. It builds and sizes at 29.6 kB of flash and 22.1 kB of RAM; it was not run on the board.

   ```text
   portability/boosterpacks/audio/boards/am13e230/build.sh algos
   ```

   You should see: The script leaves `audio.out` under `out/boosterpack-speedrun/am13-audio-algos/`.

   If the build stops on the SDK path, set it as in step 2.

### Expected result

The pack plays a clean 1 kHz tone and a four-voice tune, and its microphone reads the tone back at 1000.0 Hz.

### What this shows

- A plain GPIO driver sends 16-bit frames to the pack's DAC8311 from a motor-control chip, with 16 clock pulses and the chip-select held low for the whole frame.
- The pack's microphone reads 1000.0 Hz with 99.9 percent of the energy within 50 Hz.
- The tuner holds within 0.6 cent on clean tones from 82 to 500 Hz, the 16 spectrum bars within 1.3 pixels of a double-precision DFT above 30 pixels, and the DTMF decoder decodes a 16-digit sequence exactly down to 2 dB signal to noise, all on the Mac.

### Limits

- The tuner, spectrum, DTMF and gate code did not run on a board: they were checked on the Mac and built for the LP-AM13E230 and LP-MSPM33C321A.
- The gate and echo were not heard on the speaker. The DTMF decoder was tested with white noise only, not speech or music.
- The synth runs at 8 kHz, so nothing above 4 kHz plays.
- The SPI lines are bit-banged. The DAC is on SPI only: the pack's jumper positions for the LaunchPad DAC and for PWM are not tested.
- The LP-MSPM33C321A and LP-MSPM0G3519 build the same app and were not run with the pack on. The pack cannot be driven from the LP-MSPM0G5187 without wires.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Tone | microphone reads 1000.0 Hz, 99.9 percent of energy within 50 Hz |  |
| Build size | LP-AM13E230 16.9 kB flash and 13.3 kB RAM; LP-MSPM33C321A 10.3 and 4.4; LP-MSPM0G3519 8.9 and 4.4 (default app, -O2) | `portability/boosterpacks/audio/README.md` |
| Apps image size | LP-AM13E230 29.6 kB flash and 22.1 kB RAM; LP-MSPM33C321A 23.1 and 13.4 | `portability/boosterpacks/audio/README.md` |
| Tuner (Mac) | within 0.6 cent for clean tones 82 to 500 Hz; no octave errors in 456 tests |  |
