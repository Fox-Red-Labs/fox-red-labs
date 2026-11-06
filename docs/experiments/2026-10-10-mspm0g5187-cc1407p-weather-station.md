<!-- experiment
id: E180
board: LP-MSPM0G5187
date: 2026-10-10
status: works
reached: verified
line: TI's CC1407P host driver runs on the MSPM0G5187 with its driver files unchanged and its two Zephyr-only files replaced by this repo's bare-metal layer. It needs eight wires, because the LaunchPad has pins only on J1 and J3, which the Sensors BoosterPack uses. 21,658 of 21,658 packets over about 6 h 15 min reached an LP-CC1354P10-1 at 915 MHz, with 0 checksum errors and -14 dBm on one desk. Readings reach Home Assistant.
stop: none
-->
# Can an MSPM0 host TI's CC1407P radio, the BP-CC1407P-9-FE, and send sensor readings to another board?

**Short answer.** Yes. TI ships no MSPM0 code for the BP-CC1407P-9-FE radio BoosterPack plug-in module, but TI's host driver from its Zephyr fork runs on an LP-MSPM0G5187 with its driver files unchanged; only its two Zephyr-only files are replaced by this repo's bare-metal layer. The G5187 reads temperature, humidity and light from a Sensors BoosterPack and sends them every 1.04 s at 915 MHz. An LP-CC1354P10-1 running TI's own receive example, moved to 915 MHz, received 21,658 of 21,658 packets over about 6 h 15 min, ending 2026-10-11 06:00, with no checksum errors, at -14 dBm on the same desk. A bridge on the Mac posts the readings to Home Assistant. Range and a comparison with TI's supported host are not done.

Terms: [vocab](../vocab.md) (SPI, CRC, RSSI, BoosterPack, HAL). 1 kB = 1,024 bytes. Code and wiring: [mspm0g5187/radio](../../mspm0g5187/radio/README.md); receiver: [cc1354p10/radio-rx](../../cc1354p10/radio-rx/README.md). Checked on an Intel Mac, macOS 15.7.9, CCStudio 21.0.1, TI Arm Clang 5.1.1.LTS, UniFlash 9.6.0, MSPM0 SDK 2.11.00.07 with SysConfig 1.26.3 (sender), F2 SDK 8.33.00.16 with SysConfig 1.28.0 (receiver), TI's driver from `simplelink-zephyr` tag `v3.7.0-ti-1.00.01_ga`.

**Question.** The CC1407P is a radio that a host drives over SPI. Can a plain MSPM0, the part a sensor-node designer might pick, be the host, and can it talk to the radio inside a TI CC13xx?

**Vendor position.** TI's BoosterPack page, read 2026-10-07, says the plug-in's host driver "can be ported to other MCUs". TI's examples cover seven CC13xx and CC26xx LaunchPad variants under its TRX plug-in 1.00.00.52. They also cover the LP-EM-CC2745R10-Q1 under its Zephyr fork. No MSPM0 host is listed. A board has to supply eight functions to the driver: start an SPI transfer, set chip select, read the radio's data-out line, set reset, turn the interrupt on or off, wait some microseconds, and start and stop a one-shot timer.

**What was tried.**

- **Host layer.** TI's driver has two files that call Zephyr's kernel, SPI and GPIO. `portability/cc1407p-host/hal_bare.c` (written 2026-10-09) replaces them with the same logic and the eight functions above. `mspm0g5187/radio/hooks_mspm0g5187.c` supplies those eight in DriverLib, with SysTick as the timer and polled SPI at 1 MHz.
- **Wiring.** The LP-MSPM0G5187 has header pins on J1 and J3 only, and the Sensors BoosterPack uses them, so the radio sits beside the stack. Six signals go to J20, the 2x4 header by the microSD slot: SPI port UC3 on PB9, PB8 and PB7, chip select PB6, reset PB10, interrupt PB11. Power and ground come from pins that pass up through the sensor pack.
- **Sender.** TI's `ti_cc140xp_genfsk_tx` sample's sequence and its 50 kbps 2-GFSK settings, 915 MHz, the high-power amplifier at its top level. The payload is text, `FRL n=12 t=24.80C rh=36.9% lux=207.20`, 37 to 39 bytes.
- **Receiver.** TI's `rfPacketRx` for the LP-CC1354P10-1 (prop_rf examples for F2 SDK 8.33), changed to 915 MHz, keeping packets that fail the checksum, appending RSSI and printing each packet. Its default settings match the CC1407P's in symbol rate, deviation, preamble, sync word 0x930B51DE, length field and whitening.

**How far it got.** Verified on the boards:

| Step | Result |
|---|---|
| Driver open and version read | Firmware 3.0.0.25, hash 0x8352f9e3, 5 of 5 reads, after two wiring fixes (below) |
| Sender | Every packet reported finished by the radio (`status=16`) |
| Receiver, 30 s with both ends logged | 28 of the 29 live packets printed; the missing one is covered by the six-hour count below; the counter rose 14 in the 14 s between its two count lines |
| Receiver, unattended | 21,658 good packets by 06:00:48 against sender sequence 291 to 21,948 (21,658 sent), about 6 h 15 min at 1.04 s each; 0 checksum errors; 0 lost, plus or minus one at the start |
| One hour, both ends logged | 3,508 of 3,508 from 05:58:56 to 06:59:37, 0 checksum errors |
| Signal | -14 dBm median, range -15 to -14; distance on the desk not measured |
| Home Assistant | Four sensors posted every 10 s, 253 posts logged from 06:03 |
| Image sizes | Sender 20.4 kB of flash and 5.4 kB of RAM with TI's default 24 SPI buffers; receiver 29.2 kB of flash |

**The line.** None reached. The closest call was the wiring, not the software. If the radio answers with a valid checksum on everything it sends and a checksum error (`04 02 f0 00`) on everything it receives, its data-in wire is open: holding the reset and interrupt pins low in turn did not stop it answering, and forcing the data-in pin high or low did not change its reply. Here the two inner-row wires were missing and the data wire sat in pin 16 instead of 15. Check all six signals on J20 before debugging the driver.

**Ask.**

- **Open question:** Has anyone used an MSPM0 as the host for the CC140xP radios? The port needed only the eight board functions above and the two replaced Zephyr files.
- **For readers:** if you run this link outdoors, report the distance at which packets start to drop.
