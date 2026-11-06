# Jack-o'-Lantern Jukebox FAQ: fixes for the BOOSTXL-AUDIO Halloween show on the LP-MSPM33C321A and LP-AM13E230

The show plays on the LP-MSPM33C321A with the BOOSTXL-AUDIO and, without recordings, on the LP-AM13E230. Each answer starts with the fix. Checked 2026-10-25 with TI Arm Clang 5.1.1.LTS and SysConfig 1.28.0 on an Intel Mac. Terms: [docs/vocab.md](../../../../docs/vocab.md).

## Sound

### If you see the banner on the console but hear nothing, what is wrong?

Move jumper J5 on the pack to SPI DAC and push the volume slider up, then hold S1 for a second to step the volume. J5 picks what drives the pack's amplifier: the SPI DAC, the LaunchPad kit's DAC on pin 30, or PWM on pin 19. The show writes only the SPI DAC, and only that position was tested. Source: `portability/boosterpacks/audio/AGENTS.md`.

### If the pack only clicks, what is wrong?

Press the pack down evenly so it sits flat on J1 to J4. A pack that is not seated on all four headers loses some of its DAC lines, and the speaker clicks instead of playing. Source: `halloween/README.md`.

### If you hear a different board's demo, or nothing at all, after swapping packs, what is wrong?

Check that the BOOSTXL-AUDIO is the pack on the headers, its pin 1 on J1 pin 1 (3V3), and that you swapped with the USB cable out. The show drives BoosterPack pins 7, 12, 13 and 15 for the DAC and amplifier; another pack on those rows gets those signals instead. The pack goes on one way only. Source: `portability/boosterpacks/audio/README.md`, "Pins".

### If the sound stops getting louder above volume step 10, is the pack faulty?

No. Use volume steps 4 to 10 (S1 held 0.7 s steps the volume) and the pack's slider for the rest. A 1 kHz tone through a laptop microphone rose 8.6 dB from step 4 to 10 and only 0.7 dB from 10 to 16, with the 3rd harmonic 20 dB down at 16. The amplifier, the speaker and the microphone were not separated, and the slider position was not recorded. At step 16 the show's soft limiter bent 7.1 percent of samples.

### Is it legal to play the recordings at a party?

The five shipped recordings are public domain. Each track's `.json` file in `tracks/` keeps its Wikimedia Commons source page, license and cut. The Funeral March page gives no recording date. A track you add with `tools/add_track.py` is yours to clear. Source: `halloween/README.md`, "The recordings".

## Starting up

### If the board does not start after you plugged it in with a button held, how do you get the show back?

Unplug the USB cable and plug it in again without touching the buttons. S1 on the LP-MSPM33C321A (PA18) and S5 on the LP-AM13E230 (PA6) are each chip's bootloader-invoke line. Held at reset, the chip starts its bootloader instead of the show. The show reads them as buttons only once it runs. Source: `boards/mspm33c321a/main_halloween.c`, `boards/am13e230/main_halloween.c`.

### If nothing happens at all on a power bank, what is wrong?

Use a power bank with an always-on or low-current mode. Many banks switch off after 30 s to a few minutes under about 50 to 100 mA. The show never sleeps, so its load stays steady; its current was not measured. Source: `halloween/README.md`, "Battery".

### If the LED stays dark in one light theme, is it broken?

No. Press S1 (S5 on the LP-AM13E230) for the next theme. The lightning theme stays dark and strobes only on drums and thunder. Source: `halloween/README.md`, "What it does".

## Microphone and listener

### If the listener never reacts on the LP-MSPM33C321A, what is wrong?

Put one female-to-female jumper wire on the pack's top header from pin 8 to pin 5; the show drives pin 8 (PB30) high to power the microphone. On this LaunchPad, BoosterPack pin 5 is not wired to the chip: the isolation resistor R21 is not fitted, and PA21 is the VREF- pin, tied to ground through R10 (SLAU969 schematic). The microphone read the same 1 to 1.5 codes in silence and during a full-volume sweep. No jumper on the LaunchPad changes it. The wire itself has not been tried. Source: `halloween/README.md`, "Microphone".

## LP-AM13E230

### If the LP-AM13E230 prints "silent mode" and plays nothing, what is stacked on it?

Take off the DRV8376EVM and stack the BOOSTXL-AUDIO. Before it drives pins 7, 13 and 15, the image reads J3 pins 23 to 26 with pull-ups. The DRV8376EVM's voltage-sense dividers pull them low, and the image then prints `REASON DRV8376EVM sense dividers found on J3 23 to 26: silent mode, pins 7, 13, 15 left as inputs` and leaves the pins alone, so the show cannot turn on the motor board's gate drivers. With the pack or a bare board it prints `REASON J3 23 to 26 all high: no DRV8376EVM sense dividers, Audio pack assumed: play`. The silent path is written from the code and was not run with the EVM stacked. Source: `boards/am13e230/main_halloween.c`.

### If S5 does nothing on the LP-AM13E230, what is wrong?

Fit jumper J11. S5 reaches the chip's PA6 only through J11. With J11 open the show reads S5 as not pressed. Source: `boards/am13e230/main_halloween.c`.

## Flashing

### If DSLite says "Device Connection Error ... low power mode" when you flash the show, how do you get the board back?

Pulse the reset line from the probe: `xds110reset -s <serial> -a toggle -d 200`, then flash again. The image already on the board let the chip sleep, so the debug port stops answering until a reset. The show itself never sleeps (`tools/preflight_image.py` prints SAFE for it). Source: `mspm33c321a/FAQ.md`.

### If the build stops at once with exit code 2, what is missing?

Install the MSPM33 SDK 1.04.00.00 and TI Arm Clang 5.1.1.LTS in the default `/Applications/ti` folders, and SysConfig 1.28.0 at `/Applications/ti/sysconfig_1.28.0`. The build script finds the SDK and compiler through `tools/frl/toolpaths.sh`, which exits with code 2 and no message when either is missing. The SysConfig path is fixed in `build.sh`. Source: `tools/frl/toolpaths.sh` line 12; `boards/mspm33c321a/build.sh`.

### If the build says the flash is full after you added a track, what do you do?

Shorten the track with `--length`, or delete one of the shipped tracks (its `.ima` and `.json` in `tracks/`). The five shipped recordings use 174 s and 849.6 kB at 5 kB a second; the image leaves about 111 kB of the 1,024 kB flash free, room for about 22 s more. Source: `tools/add_track.py`.

### If `add_track.py` stops with `No such file or directory: 'ffmpeg'`, what is missing?

Install ffmpeg: `brew install ffmpeg`. The tool calls `ffmpeg` to cut, filter and resample the file. Source: `tools/add_track.py`.

### How do you put the board's original program back?

Write the backup you made before the first flash with DSLite and the `-u` option, then read it back. The commands for both boards are under "Restore the previous image" in the README; the backup recipe is in `mspm33c321a/AGENTS.md`. Both boards' flash was read twice and the reads matched before the first write.

### Can you hear the show before you have a board?

Yes. Run `portability/boosterpacks/audio/host/run_halloween.sh` on a Mac: it builds the same player for the Mac and writes `halloween-preview.wav` and a 165 s video with sound (the video needs ffmpeg). The clip on the project page has no sound. Source: `host/run_halloween.sh`.

### Does the show run on another LaunchPad?

No. The recordings need about 850 kB of flash, so only the LP-MSPM33C321A holds them; the LP-AM13E230 plays the synth songs only. The audio layer builds for the LP-MSPM0G3519, but the show has not been built there. Another board supplies the functions in `app/board_audio.h` and `app/board_show.h`. Source: `halloween/README.md`, "Other boards".
