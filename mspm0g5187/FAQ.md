# TI LP-MSPM0G5187 LaunchPad on a Mac: errors and fixes

Checked on the bench board 2026-10-08 and 2026-10-09 (MSPM0 SDK 2.11.00.07, UniFlash 9.6.0, probe firmware 3.0.0.38). Terms: [docs/vocab.md](../docs/vocab.md). 1 kB = 1,024 bytes.

**Contents**

**TI examples and tools**

- [TI's wake-word example never lights the LED when you say "OK Kilby". Is the onboard microphone broken?](#tis-wake-word-example-never-lights-the-led-when-you-say-ok-kilby-is-the-onboard-microphone-broken)
- [DSLite prints "A firmware update is required for the XDS110 probe" on every command. Is the update required?](#dslite-prints-a-firmware-update-is-required-for-the-xds110-probe-on-every-command-is-the-update-required)
- [TI's `adc12_monitor_supply` example reads values that drift and are nowhere near the supply voltage. Is the board faulty?](#tis-adc12_monitor_supply-example-reads-values-that-drift-and-are-nowhere-near-the-supply-voltage-is-the-board-faulty)
- [TI's `adc12_max_freq_dma` example captures 1k samples in 1.87 ms, which is 0.547 Msps. The data sheet says 1.6 Msps](#tis-adc12_max_freq_dma-example-captures-1k-samples-in-187-ms-which-is-0547-msps-the-data-sheet-says-16-msps)
- [A TI example flashes and verifies on the LP-MSPM0G5187, then nothing happens. The LEDs do not move and the debugger shows the chip stuck in `SYSCFG_DL_SYSCTL_CLK_init`. What is wrong?](#a-ti-example-flashes-and-verifies-on-the-lp-mspm0g5187-then-nothing-happens-the-leds-do-not-move-and-the-debugger-shows-the-chip-stuck-in-syscfg_dl_sysctl_clk_init-what-is-wrong)

**NPU and the occupancy model**

- [An NPU program hangs after the first test vector and never reaches the end. What is wrong?](#an-npu-program-hangs-after-the-first-test-vector-and-never-reaches-the-end-what-is-wrong)
- [The occupancy model scores 90.9 percent on its test set but much less on a new day. Why?](#the-occupancy-model-scores-909-percent-on-its-test-set-but-much-less-on-a-new-day-why)
- [What does the LP-MSPM0G5187's NPU not accelerate, and why is the 30k model only 2.3 times faster?](#what-does-the-lp-mspm0g5187s-npu-not-accelerate-and-why-is-the-30k-model-only-23-times-faster)
- [The occupancy demo on the NPU prints `people=1` in a lit room with nobody in it. Is the model wrong?](#the-occupancy-demo-on-the-npu-prints-people1-in-a-lit-room-with-nobody-in-it-is-the-model-wrong)

**Sensors BoosterPack**

- [The Sensors BoosterPack (BP-BASSENSORSMKII) is stacked on the LP-MSPM0G5187 and the demo prints `scan=none`. Is the pin map wrong?](#the-sensors-boosterpack-bp-bassensorsmkii-is-stacked-on-the-lp-mspm0g5187-and-the-demo-prints-scannone-is-the-pin-map-wrong)
- [The Sensors BoosterPack's TMP117 prints `tmp117=none` on the LP-MSPM0G5187, with the ribbon cable in. The other sensors read](#the-sensors-boosterpacks-tmp117-prints-tmp117none-on-the-lp-mspm0g5187-with-the-ribbon-cable-in-the-other-sensors-read)
- [After the Sensors BoosterPack's flat cable is plugged in, an I2C scan shows every address from 08 to 77, and the humidity and light sensors no longer answer](#after-the-sensors-boosterpacks-flat-cable-is-plugged-in-an-i2c-scan-shows-every-address-from-08-to-77-and-the-humidity-and-light-sensors-no-longer-answer)
- [The Sensors BoosterPack's motion sensor (BMI160) prints `bmi=none` on the LP-MSPM0G5187, with the humidity and light sensors reading on the same bus](#the-sensors-boosterpacks-motion-sensor-bmi160-prints-bminone-on-the-lp-mspm0g5187-with-the-humidity-and-light-sensors-reading-on-the-same-bus)
- [The humidity, light or magnet sensor, or the TMP117, on the Sensors BoosterPack never answers on the LP-MSPM0G5187. Do the power pins matter?](#the-humidity-light-or-magnet-sensor-or-the-tmp117-on-the-sensors-boosterpack-never-answers-on-the-lp-mspm0g5187-do-the-power-pins-matter)
- [The magnet sensor on the Sensors BoosterPack reads 1.64 V with no magnet, not exactly 1.65 V. Is it broken?](#the-magnet-sensor-on-the-sensors-boosterpack-reads-164-v-with-no-magnet-not-exactly-165-v-is-it-broken)
- [Which accuracy figures apply to the sensors on the Sensors BoosterPack?](#which-accuracy-figures-apply-to-the-sensors-on-the-sensors-boosterpack)

**CC1407P sub-GHz radio**

- [The CC1407P radio answers the LP-MSPM0G5187 but every reply is a checksum error (`04 02 f0 00`). What is wrong?](#the-cc1407p-radio-answers-the-lp-mspm0g5187-but-every-reply-is-a-checksum-error-04-02-f0-00-what-is-wrong)
- [Holding the CC1407P's reset pin low does not stop it answering, and its interrupt never fires. Why?](#holding-the-cc1407ps-reset-pin-low-does-not-stop-it-answering-and-its-interrupt-never-fires-why)
- [Why can't a BoosterPack with SPI stack on the LP-MSPM0G5187?](#why-cant-a-boosterpack-with-spi-stack-on-the-lp-mspm0g5187)
- [What antenna does the BP-CC1407P-9-FE radio need, and can it be used outside North America?](#what-antenna-does-the-bp-cc1407p-9-fe-radio-need-and-can-it-be-used-outside-north-america)
- [The CC1407P sends on one protocol, then every transmit on another ends with status 0x8A. Why?](#the-cc1407p-sends-on-one-protocol-then-every-transmit-on-another-ends-with-status-0x8a-why)
- [After a few profile changes, the CC1407P's `storeConfig` never completes. What stopped it?](#after-a-few-profile-changes-the-cc1407ps-storeconfig-never-completes-what-stopped-it)
- [200-byte packets to the CC1407P come back `TRX_Host_No_Resources`, but 20-byte ones go. Why?](#200-byte-packets-to-the-cc1407p-come-back-trx_host_no_resources-but-20-byte-ones-go-why)
- [TI's driver header says carrier sense is "not functional". Does listen-before-talk work on the CC1407P?](#tis-driver-header-says-carrier-sense-is-not-functional-does-listen-before-talk-work-on-the-cc1407p)
- [Wi-SUN FSK mode 4a or 5 between the CC1407P and a CC1354P10 loses packets. Is the link bad?](#wi-sun-fsk-mode-4a-or-5-between-the-cc1407p-and-a-cc1354p10-loses-packets-is-the-link-bad)
- [The CC1407P's XLR and a CC1354P10's SimpleLink Long Range do not exchange packets. Should they?](#the-cc1407ps-xlr-and-a-cc1354p10s-simplelink-long-range-do-not-exchange-packets-should-they)
- [Pings from the CC1407P to a CC1354P10 come back at 50 kbps but not at faster modes. Why?](#pings-from-the-cc1407p-to-a-cc1354p10-come-back-at-50-kbps-but-not-at-faster-modes-why)

**Walkie-talkie**

- [One message sent to the radio board did not show up. Why?](#one-message-sent-to-the-radio-board-did-not-show-up-why)
- [The walkie-talkie recording has a short gap about 2.4 seconds in. Was it the radio?](#the-walkie-talkie-recording-has-a-short-gap-about-24-seconds-in-was-it-the-radio)
- [Can the walkie-talkie work both ways on this board?](#can-the-walkie-talkie-work-both-ways-on-this-board)

**Alarm listener**

- [The alarm listener's capture or detect image hears nothing: the microphone level stays at the floor. Is the microphone dead?](#the-alarm-listeners-capture-or-detect-image-hears-nothing-the-microphone-level-stays-at-the-floor-is-the-microphone-dead)
- [The alarm listener prints `LEARN FAILED: no smoke or CO pattern heard in 45 s`. What now?](#the-alarm-listener-prints-learn-failed-no-smoke-or-co-pattern-heard-in-45-s-what-now)
- [The alarm listener prints `IGNORED ... too quiet to be the alarm beside me` when an alarm sounds. Why?](#the-alarm-listener-prints-ignored--too-quiet-to-be-the-alarm-beside-me-when-an-alarm-sounds-why)
- [The alarm listener printed `LEARNED ... saved`, but after a reset it is not learned, or the board stops at boot. What happened?](#the-alarm-listener-printed-learned--saved-but-after-a-reset-it-is-not-learned-or-the-board-stops-at-boot-what-happened)

**Mistakes to avoid**

- [Mistakes to avoid](#mistakes-to-avoid)

## TI's wake-word example never lights the LED when you say "OK Kilby". Is the onboard microphone broken?

Probably not: the example does not listen to the onboard microphone. `wakeword_detection_ai_npu` samples an analog microphone on pin PB22 (ADC channel 24) with the chip's ADC; its README says "Analog microphone connected to PB22 (AIN24)" and its code mentions a "BOOSTXL audio microphone". PB22 is BoosterPack header pin 6, where the BOOSTXL-EDUMKII BoosterPack puts its microphone. The board's own microphone feeds the PCM1809 audio ADC, which reaches the chip over I2S on PB10, PB11 and PB12, and only `device_uac_microphone_i2s` reads it. Stack a BOOSTXL-EDUMKII, or use this repo's port to the I2S microphone, [`mspm0g5187/wakeword`](wakeword/README.md): with the Mac's synthetic "OK Kilby" sent in over the serial port it detects 120 of 120, and its live microphone run waits for the CC1407P's two wires to come off J20 (PB10 and PB11 are the microphone's clock lines). Jumper J6 powers the onboard microphone and audio ADC; on the Rev. A board it ships with its shunt in place, although the user guide's jumper table lists it as unpopulated.

## DSLite prints "A firmware update is required for the XDS110 probe" on every command. Is the update required?

Not on this board. The full text is `Warning: A firmware update is required for the XDS110 probe. The current firmware is version 3.0.0.38. The probe must be upgraded to firmware version 3.0.0.43 to be compatible with this software. Click the "Update" button to update the firmware.` The board ships with probe firmware 3.0.0.38, and with that firmware DSLite still identified the chip, read and wrote flash and verified it. There is no Update button on the command line. If a command fails instead of only warning (other LaunchPad development kits on 3.0.0.31 do), update the probe: in `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/` run `./xdsdfu -I <serial> -m`, wait 3 seconds, then `./xdsdfu -I 00000000 -f firmware_3.0.0.43.bin -s <serial> -r`, wait 5 seconds, and check with `./xdsdfu -e`. The update writes the probe, not the chip.

## An NPU program hangs after the first test vector and never reaches the end. What is wrong?

A debugger reset while an earlier NPU run was in progress leaves the NPU stuck. The symptom: the program waits forever on `tvmgen_default_finished`, the first test vector passes, and the NVIC shows the NPU interrupt (IRQ 12) pending but never taken. Reading the chip after `reset halt` followed by `resume` caused it five times out of six. Fix: start the image by flashing with DSLite, which runs it when the flash finishes, wait, then halt and read the results without a reset. With that sequence every NPU image ran and matched all 20 vectors.

## TI's `adc12_monitor_supply` example reads values that drift and are nowhere near the supply voltage. Is the board faulty?

No. On the LP-MSPM0G5187 the example's ADC channel 15 is the header pin PA15 (J3 pin 27, also an I2S pin), not the internal supply divider the code comments describe: its README table lists "ADC0 | ADC12 Channel 15 Pin | PA15", while the same example for the LP-MSPM0G3507 lists no pin. Raw codes read 540 to 674 and moved while watched, where the G3507 reads a steady 1,796 to 1,797. The channel that carries the supply on this chip is not found. Until it is, treat the example's voltage as a reading of the PA15 pin.

## TI's `adc12_max_freq_dma` example captures 1k samples in 1.87 ms, which is 0.547 Msps. The data sheet says 1.6 Msps

The example as shipped is not a speed test of the chip. With TI's own settings (ADC clock SYSOSC 32 MHz setting, 62.5 ns sample time, 12-bit, DMA) 60k CPU cycles were counted at 32 MHz from start of conversion to the DMA-done interrupt, three runs the same. The G3507 with the same settings took 8.3k cycles for 1k samples. The reason the G5187 is slower with identical settings is unknown; other ADC clock and sample-time settings were not run.

## The occupancy model scores 90.9 percent on its test set but much less on a new day. Why?

Because the published test set shares days with the training data. The 1,001 test windows were cut in 48-minute blocks from the same room and days the model trained on, so neighbouring conditions sit on both sides; with whole days held out a fresh 8k model scores 87.7 percent overall but 40.2 percent balanced, calling 159 of 170 three-person windows empty (2026-10-09). It is the same model recipe; what changed is the question asked of it. Retrain on your own room's recordings with the day-held-out protocol (`tools/dataset/eval_occupancy.py`) before trusting a figure.

## What does the LP-MSPM0G5187's NPU not accelerate, and why is the 30k model only 2.3 times faster?

Layers outside the NPU's rules stay on the CPU. TI's NPU guide lists the limits (channel counts in multiples of 4, pooling kernels up to 4, fully connected inputs of 16 or more), and NNC's compile log for the 30k model says `GCONV layer NOT offloaded to TI-NPU`, so that convolution runs in software and the gain drops from 21.6 times on the 8k model to 2.3 times. The reason for that layer's refusal is not in the log; the suspected cause is its weights overflowing the NPU's 18-bit accumulator, not checked. Source: `docs/experiments/2026-10-09-mspm0g5187-npu-vs-cpu.md`.

## A TI example flashes and verifies on the LP-MSPM0G5187, then nothing happens. The LEDs do not move and the debugger shows the chip stuck in `SYSCFG_DL_SYSCTL_CLK_init`. What is wrong?

The example's generated start-up code waits forever for a clock-ready bit that this board never sets. Seven SDK examples do it (`sysctl_hfxt_run`, `sysctl_mclk_syspll`, `ecg_anomaly_detection_ai` and its `_npu` twin, and the three `motor_bearing_fault` examples). Reading the clock status register (0x400B0204) showed the PLL and the high-speed clock good and the low-frequency oscillator bit (0x800) clear. If you see this, open the generated `ti_msp_dl_config.c`, find the `while` loop in `SYSCFG_DL_SYSCTL_CLK_init` and take `DL_SYSCTL_CLK_STATUS_LFOSC_GOOD` out of its mask, or pick a different example. Why the bit stays clear is not confirmed.

## The Sensors BoosterPack (BP-BASSENSORSMKII) is stacked on the LP-MSPM0G5187 and the demo prints `scan=none`. Is the pin map wrong?

No. The kit's I2C is on header pins 9 (PA9, clock) and 10 (PA8, data), as TI's board file for the LP-MSPM0G5187 says. `scan=none` with every sensor `none` was the stack on 2026-10-09: the same board found 0x40 and 0x44 on 2026-10-10 after the BoosterPack was reseated, with the same pins and bus settings, so the seating is the inferred cause. First unplug the USB cable, press the BoosterPack straight down until its pin 1 (marked 3V3 on the BoosterPack) sits in the kit's J1 pin 1, and plug in again. A seated stack prints `scan=40,44` within a second of starting the demo (the output is on `/dev/cu.usbmodem<probe serial>1` at 115200). The demo programs switch the sensors on for you; in your own code set PB22 (pin 6) low for the humidity sensor and PA15 (pin 27) high for the light sensor. Only header rows J1 and J3 have pins on this LaunchPad, so BoosterPack pins 1 to 10 and 21 to 30 are connected and the two LEDs (pins 39 and 40) and the motion sensor's second interrupt (pin 13) are not. The `err=` count at the end of each line grows by one for every failed read: 3 a second with the TMP117 and the motion sensor missing, 5 a second when nothing answers. One more way to get the same `scan=none`, with `drv=0.000V`: a second call to `SYSCFG_DL_GPIO_init` (run of 2026-10-10), which writes the pins' initial values and switches every sensor off again; set the power pins after the last init call. Seen 2026-10-10, board Rev. A.

## The Sensors BoosterPack's TMP117 prints `tmp117=none` on the LP-MSPM0G5187, with the ribbon cable in. The other sensors read

The TMP117 sits on the break-away strip and is joined to the rest of the board only by the flat cable that ships loose in the box. TI's user guide (SLAU829, section 2.1.4) says the metal contacts face down in the connector and the black tab faces up; the demo already drives J3 pin 28 (PA12) high to power it. On 2026-10-10 every fit that could be tried left 0x48 silent: one way round broke the bus (next entry), the other left the bus healthy and silent, also with 3.3 V wired from pin 1 to pin 28. The cause was not found. A second cable, or the BoosterPack on another LaunchPad, would tell a bad cable from a bad strip.

## After the Sensors BoosterPack's flat cable is plugged in, an I2C scan shows every address from 08 to 77, and the humidity and light sensors no longer answer

The cable is in end for end. The two sockets number their pins in opposite order (SLAU829 Figure 9: J5 has V+, GND, SDA, SCL, ALERT, J6 has ALERT, SCL, SDA, GND, V+), and the cable is symmetric, so one way round connects the clock line to ground. Unplug USB, take the cable out, and the scan returns to `40,44`. Turn it end for end, push both ends in until they stop and press both latches flat before plugging USB back in. The reader keeps driving the sensor power pin until it is reset, so halt it first if you see this. Seen twice on 2026-10-10; the program recovered each time with no damage.

## The Sensors BoosterPack's motion sensor (BMI160) prints `bmi=none` on the LP-MSPM0G5187, with the humidity and light sensors reading on the same bus

Cause not found. The schematic in SLAU829 puts the BMI160 straight on 3V3 with its chip-select tied high, so it should answer at I2C address 0x69 whenever the humidity sensor at 0x40 does. On 2026-10-10 it answered on no scan in three runs, with the I2C controller at 100 kHz and with the lines driven by software; the third run had an I2C bus recovery at startup (nine clock pulses, then a stop) and read the same. A 2024 TI forum thread (sensors forum 1430282) reports the same silence at 0x69 on another LaunchPad and closed without a cause. A photo of the bench board on 2026-10-10 shows both the BMI160 and the BMM150 fitted in their marked boxes, so a missing part is ruled out there. Things to check: whether the BMI160 answers on another LaunchPad, and whether the ribbon cable or the break-away strip changes anything (the cable was out in every run). The light-driven occupancy demo (`occ_live.c`) is not wired to the motion sensor.

## The occupancy demo on the NPU prints `people=1` in a lit room with nobody in it. Is the model wrong?

Probably not, but it is untested. The model wants four inputs: light, sound, CO2 and motion. The Sensors BoosterPack supplies only the light, so the demo holds the other three at empty-room values. In the model's 20 test windows every lit window (115 to 157 lux) had people in it, and the bench light gave 317 lux, twice that, so the model may be reading the light as people; the other candidate is the three channels the demo holds fixed, and neither is tested. The count flickers until about sample 18 (18 seconds here) and the logits settle at sample 32: with the bench lit (317 lux) it printed 0 for 15 samples, 1 at 16, 0 at 17, then 1. Covering the sensor should bring it back to 0, which has not been tried. The run and the numbers: `docs/experiments/2026-10-10-mspm0g5187-npu-live-light.md`.

## The humidity, light or magnet sensor, or the TMP117, on the Sensors BoosterPack never answers on the LP-MSPM0G5187. Do the power pins matter?

Four sensors on the BP-BASSENSORSMKII are powered through header pins, so they stay silent until your program sets them. Set pin 6 (PB22) low for the HDC2080 humidity sensor, pin 24 (PB15) low for the DRV5055 magnet sensor, pin 27 (PA15) high for the OPT3001 light sensor and pin 28 (PA12) high for the TMP117. The motion sensor and compass are always powered, so they do not need these pins. Wait about 10 ms after setting the pins before the first scan. The demo programs do all four.

## The magnet sensor on the Sensors BoosterPack reads 1.64 V with no magnet, not exactly 1.65 V. Is it broken?

No. The DRV5055A4's data sheet (SBAS640C) lets its resting output sit anywhere from 1.59 to 1.71 V at 3.3 V, about 8 mT either side of center at 7.5 mV per mT, so a magnet test must be read against the same chip's own resting value. The computed voltage also assumes the ADC's reference is exactly 3.3 V, which was not measured.

## Which accuracy figures apply to the sensors on the Sensors BoosterPack?

The data sheets', at the conditions they name. No reference instrument was used, so none of the readings tests a sensor's accuracy. Each figure is in the sheet for that part, with its conditions. The light sensor's sheet gives no accuracy figure at room-light levels (about 300 lux), only at 2000 lux.

## The CC1407P radio answers the LP-MSPM0G5187 but every reply is a checksum error (`04 02 f0 00`). What is wrong?

The data line into the radio is not connected, so the radio reads every message as noise. The radio's own replies arrive fine, which is why it looks half-working. Check that the wire from J20 PB8 sits in radio pin 15 (PICO), the 6th socket from the top of the radio's right outer row; on the bench it was one socket high, in pin 16. A quick test: `mspm0g5187/radio/mosi.c` forces PB8 high, then low; if the radio's reply does not change, the line is open. Wiring, found 2026-10-10. Details: [radio README](radio/README.md).

## Holding the CC1407P's reset pin low does not stop it answering, and its interrupt never fires. Why?

The reset and interrupt wires are not in radio pins 36 and 23. Both sit in the radio's inner rows, which are easy to miss when the other six wires use the outer rows. `mspm0g5187/radio/diag.c` holds each candidate pin low for 200 ms and talks to the radio while it is held: a radio truly in reset cannot answer. Wiring, 2026-10-10.

## Why can't a BoosterPack with SPI stack on the LP-MSPM0G5187?

Wire it instead: the 2x4 header J20 by the microSD slot carries a whole SPI port. The LaunchPad has header pins on its J1 and J3 rows only (BoosterPack pins 1 to 10 and 21 to 30). SPI data, chip select and the pins above 30 are on J2 and J4, which have no pins fitted. The CC1407P radio goes on eight wires instead: six to the 2x4 header J20 by the microSD slot, which carries a whole SPI (UC3 on PB6 to PB9), and power and ground to J1 and J3 pins that pass up through the Sensors BoosterPack. The user guide's header drawing (SLAU967, Figure 2-3) shows all four rows; the board ships with pins on two. Wiring table: [radio README](radio/README.md#wiring).

## What antenna does the BP-CC1407P-9-FE radio need, and can it be used outside North America?

A 915 MHz antenna with an SMA plug, screwed onto the radio's SMA socket before it transmits. TI's user guide (SWRU633) lists none in the kit; the bench unit's came in the box. The -9 board is the 902 to 928 MHz variant (US FCC and Japan ARIB), with a filter for that band on the receive side (SWRU633 section 1.2 and its parts list); TI also lists a -8 variant (BP-CC1407P-8-FE); check its product page for the band before buying for another region. Check your local rules before transmitting.

## The CC1407P sends on one protocol, then every transmit on another ends with status 0x8A. Why?

Reset the radio between protocols: close and reopen TI's host driver, which pulses RESET_N and takes about 50 ms. The radio appears to keep the image of its first transmit since its last reset (suspected; the logs show only that a reset cures it), so a change of modem (FSK, OFDM, O-QPSK) or RF mode fails with 0x8A, "unknown operation". Storing the device config with the new RF mode, or calling `TRX_Host_utilSetRfMode`, did not switch it on firmware 3.0.0.25. In the run without resets, the five profiles that needed one were the first OFDM, the first 2-GFSK, the 9.6 kbps 2-GFSK, O-QPSK and XLR. With a reset before each one, all 23 of TI's profiles sent 345 of 345 packets ([radio README](radio/README.md#every-protocol-the-radio-has)).

## After a few profile changes, the CC1407P's `storeConfig` never completes. What stopped it?

Probably its configuration storage filling up. It took four persisted copies of a 1,776-byte Wi-SUN config. The fifth persist returned in 1 ms instead of 13; from the sixth on, the store's completion event had not arrived after 5 s, while packets still went out on the last config. Its capacity in bytes is not measured, so smaller configs may fit more. Call `TRX_Host_eraseNv` before storing a new set, then store the front-end config again: that worked in the 23-profile run, which erases at every reset; erase alone was not tested.

## 200-byte packets to the CC1407P come back `TRX_Host_No_Resources`, but 20-byte ones go. Why?

Suspected: a config that is stored but not persisted keeps holding the radio's buffer space. The run shows refusals without the persist and none with it. Persist each config right after storing it; then 200-byte packets went on the first try on all 23 profiles.

## TI's driver header says carrier sense is "not functional". Does listen-before-talk work on the CC1407P?

At low thresholds it answered correctly here. On radio firmware 3.0.0.25, `TRX_RadioCommand_CarrierSense` in energy mode returned idle (0x11) and busy (0x12) as expected at noise-floor thresholds, and busy at -60 dBm while a nearby CC1354P10 transmitted, idle again after. At -10 dBm it read idle, as expected for a signal near -19 dBm; at -30 dBm it also read idle, which is not explained. Test it on your board before you rely on it. `radio_lab.c` has the command set-up (`C`).

## Wi-SUN FSK mode 4a or 5 between the CC1407P and a CC1354P10 loses packets. Is the link bad?

Not as far as the runs show. In two runs the CC1354P10 received 20 of 20 from the CC1407P in every Wi-SUN FSK mode, but the CC1407P received 14 and 16 of 20 in mode 4a and 6 of 20 twice in mode 5 from the CC1354P10, which reported every send as done. Packets were 20 of 20 both ways in modes 1a, 1b, 2a, 2b, 3 and 4b; pings were partial at 2a, 2b, 4a and 5. Use 1a, 1b, 3 or 4b between these two chips until the cause is known.

## The CC1407P's XLR and a CC1354P10's SimpleLink Long Range do not exchange packets. Should they?

No TI document pairs them. The CC1407P decoded the header 7 and 16 times from the CC1354P10's 5 and 2.5 kbps settings but passed no checksum, and the CC1354P10 decoded nothing from XLR. For long range between these two chips, use a pair that interoperated, such as Wi-SUN FSK 1a at 50 kbps.

## Pings from the CC1407P to a CC1354P10 come back at 50 kbps but not at faster modes. Why?

Most likely the answer arrives before the CC1407P's receive is running. When `radio_lab.c` started the receive with a host command after each ping, only the 50 kbps modes got answers. With TI's command chaining (the transmit in slot 0 chains to the receive in slot 1 on the radio), every mode answered, though 2a, 2b, 4a and 5 only partly. `radio_lab.c`'s `G` command shows the chaining.

## One message sent to the radio board did not show up. Why?

Not known for sure. In the house demo, 7 of 8 parent messages reached the LP-MSPM0G5187 ([radio-demos.json](../site/data/radio-demos.json)). The board listens for 80 ms between its own sends (sensor checks and a heartbeat every second), so a message that arrives while it is sending can be missed. Send important messages twice, or have the board answer each one so the sender can retry.

## The walkie-talkie recording has a short gap about 2.4 seconds in. Was it the radio?

No: the radio delivered all 122 voice frames with good checksums. One frame was cut to 123 of its 205 bytes on the USB serial line into the sending board, before it was sent, so 40 ms of audio is missing. Pace the frames into the sender, or have it check each frame's length before sending.

## Can the walkie-talkie work both ways on this board?

Not as wired here. The LP-MSPM0G5187's microphone sends its audio on PB10 and PB11, which carry the radio's reset and interrupt wires, and neither board has a speaker. Two-way voice needs the radio on other pins, a speaker amplifier, and a second CC1407P.

## The alarm listener's capture or detect image hears nothing: the microphone level stays at the floor. Is the microphone dead?

Probably not: check what is on PB10 and PB11. Those pins are the microphone's I2S clock lines (the PCM1809's WCLK and BCLK). If the BP-CC1407P radio is wired to header J20, its reset and interrupt wires sit on the same two pins and the microphone reads zeros. With them off, a 995 Hz tone came through clean and a quiet room read -79.6 dBFS. Put the wires back to use the radio again.

## The alarm listener prints `LEARN FAILED: no smoke or CO pattern heard in 45 s`. What now?

Sound the alarm longer, or move the board closer. After S2 the board waits up to 45 s for four inferences in a row that hear the smoke (temporal-3) or CO (temporal-4) pattern ([detect.h](alarm/detect.h)). One inference covers the last 8.3 s, so a test button held for under about 10 s may not be enough. An alarm whose pattern is not the US standard one (NFPA 72) will not be learned at all. The learn runs used alarm sound sent over USB.

## The alarm listener prints `IGNORED ... too quiet to be the alarm beside me` when an alarm sounds. Why?

The board is farther from the alarm, or more muffled, than when it learned. It ignores a learned pattern more than 10 dB quieter than the level it stored, so that the same alarm on a TV across the room does not count. Learn again with the board where it will stay: press S2, then the alarm's test button. Hold S2 for 5 s to forget (`FORGOT: generic mode`).

## The alarm listener printed `LEARNED ... saved`, but after a reset it is not learned, or the board stops at boot. What happened?

That was a bug in an early build, now fixed. The record was written to the data flash at 0x41D00000 as two 64-bit writes; the second write fails, and a later 128-bit write left a half-written record that stopped the board at boot. The current code writes one 64-bit word. If an old image left a bad record, erase that sector (OpenOCD or UniFlash) and flash the current image.

## A DriverLib example retargeted from the LP-MSPM0G3519 fails with `use of undeclared identifier 'UART0'` (or `SPI1`, `I2C2`, `GPIOC` in the generated file). Is the example unportable?

Not for that reason. Delete `ti_msp_dl_config.c` and `ti_msp_dl_config.h` from the example folder (the copies next to the `.c`, not the ones in `ticlang/`) and build again. TI's examples ship a generated config beside the source; the source includes that G3519 copy before the file SysConfig writes for the G5187. With the copies deleted 79 of 122 `driverlib` examples built, against 64 with them, and `uart_rw_multibyte_fifo_poll` and the SPI examples among the new ones (SysConfig 1.28.0, 2026-10-24).

## A retargeted DMA example (`dma_block_transfer`, `dma_fill_data`, `dma_table_transfer`) never reaches its breakpoint, though `gDstData` holds the right words. Why?

SysConfig put the transfer on DMA channel 1 and the example's interrupt handler only handles channel 0, so the done flag never sets. Keep `DMA.associatedChannels[0].peripheral.$assign = "DMA_CH0"` in the `.syscfg` when you unlock pin assignments, or check `DMA_CH0_CHAN_ID` in the generated header is 0. The native G5187 build of the same example passes (`gVerifyResult` 1).

## After flashing a TI Drivers example retargeted from another chip the debugger says "Connection to MSPM0 core failed". How do I get the board back?

The example's `device_linker.cmd` still describes the G3519 (512 kB flash, RAM at 0x20200000 and 0x20210000), so the stack top is outside the G5187's 32 kB and the chip faults in a reset loop. Hold the core in reset with TI's wait-for-debug sequence on the power access port, catch the first instruction, then program your backup (`program backup-flash.bin 0x0 verify`). The sequence is the same one as in the [LP-MSPM0C1104 FAQ](../mspm0c1104/FAQ.md), with this chip's addresses; it brought the board back twice with main and data flash read back equal to the backup (2026-10-24). Check the stack top (`SP` in the vector table) of any retargeted image before you flash it.

## Mistakes to avoid

- Do not call SysConfig's generated `SYSCFG_DL_GPIO_init` a second time after setting the sensor power pins. The generated function writes every pin's initial value, so it switches the sensors off again (one Sensors BoosterPack run read `none` for everything). Set power pins after the last init call.
- Do not reset the chip from the debugger while an NPU inference is in progress; it leaves the NPU stuck for the next run. Flash with DSLite and let the image run; halt and read without a reset.
- Do not write the alarm listener's learned record to the data flash as two 64-bit writes; the second one fails. One write per record.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-24. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- A retargeted example whose linker file is the G3519 map puts the stack top outside RAM; the chip faults in a reset loop (DSLite: Connection to MSPM0 core failed) (`mspm0g5187/FAQ.md`, 2026-10-24)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| sweep/results-g3519-port/tools/recover.sh with recover_halt_g5187.tcl (wait-for-debug hold), then program the backup (twice; main and data flash read back equal to the backup) | remote | proven | `mspm0g5187/sweep/results-g3519-port/tools/recover.sh`, `mspm0g5187/FAQ.md`, 2026-10-24 |

Restore: OpenOCD prog.sh: program backup-flash.bin 0x0 verify. Regions: main and data flash. (`mspm0g5187/FAQ.md`)

<!-- recovery:end -->

## Can the board run the MLPerf Tiny keyword-spotting model, and why did my first run score 3 of 20?

Yes: 18.70 ms on the NPU, 91.0 percent on the full test split (`docs/experiments/2026-11-03-mspm0g5187-mlperf-tiny-kws.md`). Three traps, each from that run. (1) NNC's `skip_normalize=true` only drops the input scaling when it is the model's first operation; this model pads first, so the model still takes float input and an int8 buffer is read as floats (3 of 20 right; feeding floats gave 18 of 20). (2) NNC's CPU build of the same model needs 43.7 kB of RAM and does not link into 32 kB, and ResNet-8 needs 86.5 kB even on the NPU. (3) A board that reads its own start-up text back (`m-inad` in the receive buffer) has a loop wire between PA10 and PA11; take it off and fit the J101 shunts before using the backchannel serial port, or drive the image through the debugger as `mspm0g5187/mlperf-tiny/host/swd_run.py` does.
