# Jack-o'-Lantern Jukebox

Halloween music and color-changing light from an LP-MSPM33C321A LaunchPad, the BOOSTXL-AUDIO and a power bank. No wiring.

The page: [site/project-jack-o-lantern-jukebox.html](../../site/project-jack-o-lantern-jukebox.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## The show on the LP-MSPM33C321A with the BOOSTXL-AUDIO

One LaunchPad, one BoosterPack and a USB power bank. Build and flash once from a Mac.

Status: Works. Boards: mspm33c321a.

Folders this version uses (in a sparse clone, fetch them first, then read `portability/boosterpacks/audio/AGENTS.md`):

```bash
git sparse-checkout add portability/boosterpacks/audio mspm33c321a tools mac-setup
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM33C321A LaunchPad development kit | 1 | plays the show: 1 MB of flash holds the recordings, and its RGB LED is the light |
| BOOSTXL-AUDIO audio BoosterPack plug-in module | 1 | the sound: a DAC8311 converter, a TPA301 amplifier and a small speaker |
| USB power bank with a USB-C cable | 1 | powers the show with no computer attached; pick one with an always-on or low-current mode |

### Before you start

- A Mac with the MSPM33 SDK 1.04.00.00, TI Arm Clang 5.1.1.LTS and SysConfig 1.28.0 in the default `/Applications/ti` folders, and CCStudio 21.0.1 for its `DSLite` flasher (`mac-setup/README.md`). Only an Intel Mac with macOS 15.7 was tried.
- Your board's probe serial in the target file: copy `mspm33c321a/MSPM33C321A_<probe serial>.ccxml` and put your serial in place of `<probe serial>` (`xdsdfu -e` lists it).
- The code is in this repository: `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git`, then `cd fox-red-labs` and `git sparse-checkout add portability/boosterpacks/audio mspm33c321a tools mac-setup`. Every command on this page runs from that folder.
- The board ships with a demo in its flash. To keep it, back it up first with the command in `mspm33c321a/AGENTS.md`.

### Steps

1. **Mac, terminal.** Build the 16 kHz show image with the five recordings from `portability/boosterpacks/audio/tracks/`. The script uses TI's SDK makefile and only builds; it never flashes.

   ```text
   OUT=build/halloween bash portability/boosterpacks/audio/boards/mspm33c321a/build.sh halloween
   ```

   You should see: The last lines are `built build/halloween/gpio_toggle_output/ticlang/gpio_toggle_output.out` and the flash use from the map, about 913 kB of 1 MB.

   If it stops at once with exit code 2, the script did not find the MSPM33 SDK or TI Arm Clang: install them in the default `/Applications/ti` folders.

2. **Mac, terminal.** Plug the LaunchPad into the Mac by USB-C with no BoosterPack on it. Check the image never puts the chip to sleep, then flash it with DSLite. Copy `mspm33c321a/MSPM33C321A_<probe serial>.ccxml` to `mspm33c321a/MSPM33C321A_<serial>.ccxml` and put your probe serial in place of `<probe serial>`. The flash line is assembled from the board's flash command and this build's output path.

   ```text
   python3 tools/preflight_image.py mspm33c321a build/halloween/gpio_toggle_output/ticlang/gpio_toggle_output.out
   /Applications/ti/ccs2101/ccs/ccs_base/DebugServer/bin/DSLite flash -c mspm33c321a/MSPM33C321A_<serial>.ccxml -e -f -v build/halloween/gpio_toggle_output/ticlang/gpio_toggle_output.out
   ```

   You should see: The check prints `SAFE`, DSLite finishes with the verify passing, and a serial terminal at 115200 shows `HALLOWEEN SHOW, BOOSTXL-AUDIO app, ... sample rate 16000 Hz, recorded tracks 5`.

   If DSLite reports `Device Connection Error ... low power mode`, reset the probe with `xds110reset -s <serial> -a toggle -d 200` (it sits beside `xdsdfu`) and flash again.

3. **Bench.** Unplug the USB cable first: the swap happens with the board unpowered. Pull off any other BoosterPack. Plug the BOOSTXL-AUDIO onto the same rows, its pin 1 on J1 pin 1 (3V3). Check jumper J5 on the pack sits on SPI DAC, as shipped, and push the pack's volume slider to the top.

   You should see: The pack sits flat on J1 to J4 with its speaker facing up.

   If the pack does not seat, check both header rows line up; it goes on one way only, pin 1 to 3V3.

4. **Bench.** Plug the power bank into the LaunchPad kit's USB-C socket. Nothing else is connected.

   You should see: The Mountain King recording opens the show and the RGB LED swaps orange and purple on the beat. Songs shuffle and loop, with a scare sound between them.

   If you hear nothing, check J5 sits on SPI DAC and the slider is up. If nothing happens at all, the bank switched off at low load: use one with an always-on or low-current mode.

5. **Bench.** S2 short: next song. S1 short: next light theme (pumpkin, fire, lightning, ghost, heartbeat, witch, music). S2 held 0.7 s: next sound mode (clean, haunted, graveyard, mad lab). S1 held 0.7 s: volume step. Both together, released within 2 s: shuffle on or off. Both held 2 to 5 s: speaker EQ on or off, to compare.

   You should see: Every action beeps, blinks the LED and prints a line on the serial console at 115200. Settings start fresh at each power-up.

   If the board does not start after you held S1 while plugging in, that button starts the chip's bootloader: unplug and plug in again without touching it.

6. **Mac, terminal.** Install `ffmpeg` first (`brew install ffmpeg`). The tool cuts, filters and encodes any audio file into the show; then run steps 1 and 2 again. The five shipped recordings use 174 s and leave room for about 22 s more: to add a longer song, delete one of them from `portability/boosterpacks/audio/tracks/` first.

   ```text
   python3 portability/boosterpacks/audio/tools/add_track.py "<any audio file>" --start 30 --length 35 --title "My spooky song" --no-build
   ```

   You should see: The tool prints the track it wrote; after steps 1 and 2, your song joins the shuffle.

   If the build reports the flash is full, shorten the new track with `--length` or drop another one from the folder.

### Expected result

After flashing and stacking the pack, the LaunchPad plays the show from a power bank with no computer: recordings and synth songs on shuffle, scare sounds between them, and the RGB LED following the music.

### What this shows

- One LaunchPad with 1 MB of flash plays recorded orchestra and organ music: five 30 to 40 s cuts at 10 kHz in IMA ADPCM, 849.6 kB together, played back at 16 kHz.
- The sample interrupt keeps time with room to spare: 15,999 frames a second, 0 late, with the worst interrupt at 5.8k of its 10k-cycle budget.
- A speaker EQ fitted from a measured tone sweep: 300 Hz high-pass, five peaking filters and a bass enhancer for a speaker whose response spreads 15.5 dB from 500 Hz to 4 kHz.

### Limits

- The power bank's current draw and run time were not measured.
- The pack's microphone gets no power on this LaunchPad (BoosterPack pin 5 is not wired to the chip), so the clap and knock listener is off here.
- Loudness stops rising above about volume step 10 of 16: a 1 kHz tone through a laptop microphone rose 8.6 dB from step 4 to 10 and 0.7 dB from 10 to 16. Whether the amplifier, the speaker or the microphone limits it was not separated.
- Some power banks switch off under about 50 to 100 mA; the show has no low-power mode, so pick a bank that stays on.
- The speaker EQ was fitted from a laptop's built-in microphone, taken as flat.

### Numbers and sources

| | Value | Source |
|---|---|---|
| DAC frames a second | 15,999, 0 late ticks over 480 s |  |
| Worst sample interrupt | 5.8k of 10k cycles at 160 MHz |  |
| Speaker EQ | 300 Hz high-pass, five peaking filters and a bass enhancer, fitted to a stepped tone sweep heard by a laptop microphone; the measured spread from 500 Hz to 4 kHz is 15.5 dB, the fit predicts 6.4 dB with the EQ |  |
| Image size | 913.0 kB of 1 MB flash; 19.3 kB RAM |  |
| Software | MSPM33 SDK 1.04.00.00, TI Arm Clang 5.1.1.LTS -O2, SysConfig 1.28.0, Intel Mac, macOS 15.7 | `portability/boosterpacks/audio/halloween/README.md` |
