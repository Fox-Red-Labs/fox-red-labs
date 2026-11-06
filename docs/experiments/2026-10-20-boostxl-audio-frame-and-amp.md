<!-- experiment
id: E237
board: LP-AM13E230
date: 2026-10-20
status: works
reached: verified
line: The BOOSTXL-AUDIO plays a clean 1 kHz tone and a four-voice tune from an MSPM0-style driver when each DAC8311 write is one 16-bit frame with SYNC low for all 16 clocks and the amplifier line is driven low. With two 8-bit transfers and SYNC raised between them the DAC never updates and the pack is silent; with the amplifier line floating or high it is also silent. Run on the LP-AM13E230; the LP-MSPM0G3519 and LP-MSPM33C321A builds are not run.
stop: none
-->
# Why does the BOOSTXL-AUDIO only click on some LaunchPad kits, and what makes it play on the LP-AM13E230?

**Short answer.** Two things must both be right. Each sample is one 16-bit frame with SYNC low for all 16 clocks, and pin 13 is driven low to take the TPA301 out of shutdown. With both, the pack's own microphone hears 1000.0 Hz with 99.9 percent of its energy within 50 Hz of the peak at volume 4. Raise SYNC between two 8-bit transfers and the DAC cancels every write: the output stays at zero and nothing plays. Leave pin 13 floating or high and the amplifier is off. A silent DAC behind a working amplifier gives one click when the amplifier turns on and nothing after, which matches the click-and-silence a CCStudio board note reports on the LP-MSPM0G3519 and LP-CC1354P10.

Terms: [vocab](../vocab.md). LaunchPad notes: [LP-AM13E230 FAQ](../../am13e230/FAQ.md). Code and tests: [BOOSTXL-AUDIO README](../../portability/boosterpacks/audio/README.md). **SYNC** is the DAC8311's chip-select input; a **frame** is one complete write.

**Question.** CCStudio 21.0.1's board note says its tests of this pack on the LP-MSPM0G3519 and LP-CC1354P10 gave a click or chirp and no sustained audio, cause unresolved (as of 2026-06-18). What makes it play?

**Vendor position.** The user guide (SLAU670A) gives pins and the three DAC sources and says nothing about frame length or the amplifier line. Its text lists J1.8 and J1.9 for SYNC and SCLK; the schematic in the same guide (Figure 12) puts SYNC on pin 12 and SCLK on pin 7, and the bench agrees with the figure. The DAC8311 data sheet says the DAC updates after the 16th falling clock edge and that SYNC rising before it cancels the write. The TPA301 data sheet says SHUTDOWN high turns the amplifier off. TI's only software for the pack is for the MSP430FR5994 (2016).

**What was tried.** One LP-AM13E230 with the pack stacked, jumper J5 on the SPI DAC, TI Arm Clang 5.1.1.LTS -O2, 8 kHz samples from SysTick, the three DAC lines driven as plain GPIO. The pack's microphone (pin 6, ADC) captured 2k samples at 8 kHz while each variant played; a Hann-windowed DFT gave the dominant frequency. Variants: wrong frame (two 8-bit transfers), correct frame, and each against the amplifier line floating, driven low and driven high.

**How far it got.**

- Built and ran: 16.9 kB of flash, one image driven by serial keys.
- Microphone floor (silent, amplifier off): 25 to 34 mV RMS, a 60 Hz peak.
- Wrong frame, amplifier floating, low or high: 22.7, 27.5 and 25.4 mV RMS. Same as the floor; no 1 kHz peak.
- Correct frame, amplifier floating or high: 27.0 and 25.9 mV RMS. Silent.
- Correct frame, amplifier low, volume 4: 1127 mV RMS, 1000.0 Hz, 99.9 percent of the energy within 50 Hz of the peak, 30 of 2k samples at the ADC rails. At volume 8 and 12 the microphone clips (942 and 959 samples at the rails) and the share is 95 to 96 percent.
- Tune, amplifier low: dominant 1046.5 Hz (C6, a lead note) with 78 to 82 percent of the energy within 50 Hz, 200 mV RMS at volume 4 and 562 mV at volume 12, no clipping.
- Tune, amplifier high: floor.
- Heard on a demo run: the 1 kHz tone and then the tune, both clean.
- The raw microphone logs stay in the lab notebook; the numbers above come from them.

**The line.** Not reached for the cause of the original note: the LP-MSPM0G3519 and LP-CC1354P10 were not tested and TI's test code is not available, so the match to the click is by mechanism. The click itself was not captured: each variant's microphone capture started 0.6 s after the amplifier line changed. The 8-bit-frame variant is an emulation (bit-banged, SYNC raised after the first byte), not a hardware SPI peripheral run.

- Run the same image on the LP-MSPM0G3519 and check the same two variants there.
- Capture the click: start the microphone capture before the amplifier line moves.
- Hardware SPI with 16-bit frames and a software SYNC line, and a hardware SPI with 8-bit frames and chip select handled by the peripheral.
- J5 on the LaunchPad DAC (pin 30) and on PWM (pin 19); PWM is expected to sound worse and how much is the measurement.

**Ask.** Has anyone run this pack on the LP-MSPM0G3519 or LP-CC1354P10 with 16-bit frames and pin 13 low? A result there would settle whether the click in CCStudio's note has this cause.
