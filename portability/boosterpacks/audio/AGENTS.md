# BOOSTXL-AUDIO from a LaunchPad: what an AI coding assistant needs to know

That the pack's DAC8311 needs one 16-bit frame with SYNC held low for all 16 clocks, that its amplifier is in shutdown unless pin 13 is driven low, and that TI's guide text lists the wrong pins for SYNC and SCLK. This file is the short list; each line came from a run on one LP-AM13E230 unless it says otherwise.

Terms: [docs/vocab.md](../../../docs/vocab.md). In short: **SYNC** is the DAC8311's chip-select input; a **frame** is one complete 16-clock write; **TPA301** is the pack's speaker amplifier.

**How to use it.** Copy this file into your project as `AGENTS.md` (Claude Code reads `CLAUDE.md`: copy it under that name, or add the line `@AGENTS.md`). Paths in backticks starting `portability/boosterpacks/audio/` are in the Fox Red Labs repository, where the evidence lives.

## The pack

| Part | Fact |
|---|---|
| Parts | DAC8311 (14-bit, 16-bit frame), TPA301 amplifier, 8 ohm speaker, electret microphone with a preamp, TS3A225E headset detection (SLAU670A) |
| SPI pins | SCLK pin 7, SYNC pin 12, DIN pin 15 (SLAU670A Figure 12). The guide text in 2.1.2.1 says J1.8 and J1.9; the figure and the bench say pins 12 and 7 |
| Amplifier | Shutdown line on pin 13, active high at the TPA301. Driven low it plays; floating or driven high it is silent |
| Microphone | Power on pin 5 (drive high), output on pin 6. With the speaker beside it the 12-bit ADC clips from volume 8 up |
| J5 | Picks the DAC source: SPI DAC (default), LaunchPad DAC on pin 30, PWM on pin 19. Only the SPI DAC position is tested |
| Alternates | 0 ohm resistors move the microphone to pins 26 and 31, the amplifier to 38 and the PWM to 39 (needed with the Sharp memory LCD pack, which uses pin 6) |

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| Silence, the DAC output stays at zero | SYNC is raised between two 8-bit transfers, so the DAC cancels every frame | Hold SYNC low across all 16 clocks: `ba_dac_write16` | A 1 kHz peak on the pack's microphone |
| Silence with a correct frame | Pin 13 floating or high: amplifier in shutdown | Drive pin 13 low | Microphone RMS rises from about 25 mV to over 1 V |
| A 60 Hz peak in a silent capture | The microphone floor (cause not traced; mains pickup is the likely one) | Ignore it: about 25 mV RMS | Same floor with the amplifier off |
| Microphone samples pinned at 1 and 4095 | The preamp is clipping | Lower the volume to 4 | The 1 kHz share stays above 99 percent |

## Check it without ears

Capture microphone samples while the tone plays (`c` in the test app) and run `python3 portability/boosterpacks/audio/tools/mic_analyze.py console.log`. A correct 1 kHz tone reads 1000.0 Hz with over 99 percent of the energy within 50 Hz, measured on the LP-AM13E230 at volume 4.

## Where each board stands

| Board | Status |
|---|---|
| LP-AM13E230 | Ran: tone, tune and wrong-frame test measured |
| LP-MSPM0G3519, LP-MSPM33C321A | Built, not run |
| LP-MSPM0G5187 | Not usable as shipped: its J2 and J4 headers are not fitted, and the pack needs pins 12, 13 and 15 |
