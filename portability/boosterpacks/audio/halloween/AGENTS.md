# Jack-o'-Lantern Jukebox on the LP-MSPM33C321A and LP-AM13E230: what an AI coding assistant needs to know

The show runs on the LP-MSPM33C321A with recorded tracks at 16 kHz and on the LP-AM13E230 with synth songs only at 8 kHz. The pack works only with jumper J5 on SPI DAC, and one button on each board is also the chip's bootloader line. This file is the short list; each line names the file or run it came from, all from 2026-10-25 unless it says otherwise.

Terms: [docs/vocab.md](../../../../docs/vocab.md). In short: **DAC** is the pack's DAC8311 digital-to-analog converter; a **frame** is one 16-clock write to it; **ADPCM** stores sound in 4 bits a sample; **ISR** is the sample interrupt. 1 kB = 1,024 bytes.

**How to use it.** Copy this file into your project as `AGENTS.md` (Claude Code reads `CLAUDE.md`: copy it under that name, or add the line `@AGENTS.md`). Paths in backticks are relative to the repository root, or to `portability/boosterpacks/audio/` when they start `boards/`, `tools/` or `halloween/`. The pack itself (DAC frame, amplifier pin, pins per board) is in `portability/boosterpacks/audio/AGENTS.md`.

## The two builds

| Fact | LP-MSPM33C321A | LP-AM13E230 |
|---|---|---|
| Build | `portability/boosterpacks/audio/boards/mspm33c321a/build.sh halloween` (MSPM33 SDK 1.04.00.00) | `portability/boosterpacks/audio/boards/am13e230/build.sh halloween` (AM13E2x SDK 26.01.00.03) |
| Compiler | TI Arm Clang 5.1.1.LTS -O2, SysConfig 1.28.0, from the linker maps | same |
| Image | 913.0 kB flash of 1,024 kB, 19.3 kB RAM | 70.7 kB flash, 24.4 kB RAM |
| Sample rate | 16 kHz, SysTick period 10,000 at 160 MHz (`boards/mspm33c321a/build.sh`) | 8 kHz, period 25,000 at 200 MHz (set in the board's `main_halloween.c`) |
| Recorded tracks | Five, 849.6 kB of IMA ADPCM from `tracks/` | None: they do not fit (`boards/am13e230/build.sh` line 18) |
| DAC frames a second | 15,999 to 16,000, 0 bad, 0 late over 480 s with the pack stacked | 7,999 to 8,000, 0 bad in 5.76 million frames over 720 s, no pack |
| Worst sample interrupt | 5.75k of 10k cycles | 4.73k of 25k cycles (`data/results/am13e230.csv`, test `audio_isr_max_cycles`) |
| Buttons | S2 (PC17) and S1 (PA18) (`boards/mspm33c321a/main_halloween.c`) | S2 wake (PB13) and S5 (PA6, only with J11 fitted) (`boards/am13e230/main_halloween.c`) |
| Light | RGB LED, 32-level software PWM; duty read back per theme | Red and green LEDs |
| Heard | Yes, from a power bank inside a carved pumpkin, filmed (`site/media/jack-o-lantern-jukebox-demo.mp4`, no sound track) | Not with the pack: the 720 s run was a bare board |

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| No sound, banner prints | J5 on the pack is not on SPI DAC, or the slider is down | Move J5 to SPI DAC, push the slider up (`portability/boosterpacks/audio/AGENTS.md`: only the SPI DAC position is tested) | The show plays |
| Only clicks | The pack is not seated on all four headers | Press it down evenly on J1 to J4 (`halloween/README.md`) | Music |
| No louder above volume step 10 of 16 | Not separated: the amplifier, the speaker or the measuring microphone. A 1 kHz tone rose 8.6 dB from step 4 to 10 and 0.7 dB from 10 to 16 | Use steps 4 to 10; at 16 the soft limiter bent 7.1 percent of samples | The level changes with each step |
| Board does not start after plugging in with a button held | S1 (MSPM33C321A) or S5 (AM13E230) is the bootloader-invoke line, read at reset | Unplug, plug in again without touching it. Written from the code and the board's button wiring, not run (`boards/mspm33c321a/main_halloween.c` line 3, `boards/am13e230/main_halloween.c` line 156) | Banner at 115200 |
| Listener never fires on the LP-MSPM33C321A | BoosterPack pin 5 is not wired to the chip (R21 not fitted), so the microphone has no power; 1 to 1.5 codes in silence and at full volume | Jumper wire from pack pin 8 to pin 5; the show drives pin 8 high. The wire is not run (`halloween/README.md`, "Microphone") | Microphone RMS rises above the floor |
| AM13E230 prints `REASON DRV8376EVM sense dividers found on J3 23 to 26: silent mode` and plays nothing | A DRV8376EVM is stacked: its sense dividers pull J3 pins 23 to 26 low, and the guard leaves pins 7, 13 and 15 as inputs. Written from the code; not run with the EVM stacked | Stack the BOOSTXL-AUDIO instead (`boards/am13e230/main_halloween.c` lines 3 to 16 and 123) | `REASON J3 23 to 26 all high ... play` |
| DSLite `Device Connection Error ... low power mode` | The previous image let the chip sleep | `xds110reset -s <serial> -a toggle -d 200`, then flash again (`mspm33c321a/FAQ.md`) | The flash verifies |

## Rules for the assistant

1. **Run `python3 tools/preflight_image.py <board> <image.out>` before the first flash** of a show image; it prints SAFE because the show never sleeps (`halloween/README.md`).
2. **Back up the board's flash before the first write**, read twice and compared (`mspm33c321a/AGENTS.md` has the backup recipe).
3. **Keep the sample interrupt under its budget** after any change: read `isr_max` and `late` from the `STAT` line every 10 s on the console at 115200.
4. **Count flash before adding tracks**: the five shipped recordings use 174 s at 5 kB a second and leave about 111 kB, roughly 22 s, free.
5. **Settings live in RAM only**; nothing is written to flash, so there is nothing to erase between runs (`halloween/README.md`).
