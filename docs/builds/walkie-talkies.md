# A walkie-talkie over a 915 MHz radio

The Mac speaks a line and compresses it to 4-bit ADPCM. The LP-CC1354P10-1 sends it over a 915 MHz radio, the CC1407P on an LP-MSPM0G5187 receives it, and the Mac plays it back.

The page: [site/project-walkie-talkies.html](../../site/project-walkie-talkies.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## One-way voice, CC1354P10 to CC1407P

Wire the radio, build both ends, flash, and run the voice demo from the Mac.

Status: Works. Boards: g5187, cc1354p10.

Folders this version uses (in a sparse clone, fetch them first, then read `mspm0g5187/AGENTS.md`):

```bash
git sparse-checkout add mspm0g5187 cc1354p10 third-party portability tools site
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM0G5187 LaunchPad development kit | 1 | the CC1407P's host |
| BP-CC1407P-9-FE sub-1 GHz radio BoosterPack plug-in module | 1 | the sub-1 GHz radio |
| LP-CC1354P10-1 LaunchPad development kit | 1 | the other radio |
| ELEGOO jumper wires (120-piece pack) | 8 | radio to the LaunchPad kit's J20 header |

### Before you start

- This ran at +14 dBm, above the US single-channel limit for this band. It is for hobby bench use. Settle output power before turning it into a product.
- A Mac with CCStudio 21.0.1 (TI Arm Clang 5.1.1.LTS), MSPM0 SDK 2.11.00.07 with SysConfig 1.26.3, UniFlash 9.6.0, and for the CC1354P10 SysConfig 1.28.0 with the F2 SDK sources (`cc1354p10/radio-rx/README.md` shows how to fetch them). Checked on one Intel Mac.
- The code: `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git`, then `git sparse-checkout add mspm0g5187 cc1354p10 third-party portability tools mac-setup site`. Every command on this page runs from the repository folder.
- Back up both boards before the first flash with the command in each board's `AGENTS.md`.
- The speech is made on the Mac by `demos.py`. The board's microphone is not used: it shares PB10 and PB11 with the radio.

### Wiring

Radio pins 1 to 10 run down the left outer row and 11 to 20 up the right outer row (20 at the top). Pins 23 and 36 are in the inner rows: 23 is the third socket down the left inner row, 36 the fifth down the right inner row. J20 is the 2x4 header by the microSD slot, printed PB12 PB10 PB8 PB6 over PB13 PB11 PB9 PB7. PB10 and PB11 also reach the board's audio converter, so the board's microphone cannot be used while the radio is wired.

| Module pin | Board pin | What it is | Needed |
|---|---|---|---|
| Radio pin 1 (3V3) | J1 pin 1 (3V3) | power | required |
| Radio pin 20 (GND) | J3 pin 22 (GND) | ground | required |
| Radio pin 7 (SCLK) | J20 PB9 | SPI clock | required |
| Radio pin 15 (PICO) | J20 PB8 | data into the radio | required |
| Radio pin 14 (POCI) | J20 PB7 | data out of the radio | required |
| Radio pin 18 (CS_N) | J20 PB6 | chip select | required |
| Radio pin 36 (RESET_N) | J20 PB10 | radio reset | required |
| Radio pin 23 (SPI_INT) | J20 PB11 | radio says it has data | required |

### Steps

1. **Bench.** With USB unplugged, screw the 915 MHz antenna onto the radio board's SMA socket and join the radio to the LP-MSPM0G5187's J20 header with the eight wires in the table. Plug the other LaunchPad into the Mac.

   You should see: Each wire's other end is on the pin in the table.

   If the radio later answers with a checksum error on everything it receives, the inner-row wires (pins 23 and 36) are the usual missing ones.

2. **Mac, terminal.** Build `radio_lab`, which turns the LP-MSPM0G5187 into a radio driven by text commands on its serial port.

   ```text
   RADIO_APP=radio_lab mspm0g5187/radio/build.sh
   ```

   You should see: The script ends with `image: mspm0g5187/radio/out/radio_lab.out`.

   If the SDK is not found, set the paths the script prints.

3. **Mac, terminal.** Follow the Build section of `cc1354p10/radio-rx/README.md` to get the F2 SDK libraries and the `rfPacketRx` example, then build `partner.c` as that README describes, with the Wi-SUN FSK mode 2b setting (`phyType868`, `#define RX_ADV`).

   You should see: The build leaves `rfPacketRx.hex`, about 29 kB.

   If the make cannot find SysConfig, point `SYSCONFIG_TOOL` at `sysconfig_cli.sh` in the 1.28.0 folder, as the README shows.

4. **Mac, terminal.** Put each probe's serial in its `.ccxml` (`xdsdfu -e` lists them). Flash `radio_lab.out` to the LP-MSPM0G5187 and `rfPacketRx.hex` to the CC1354P10, then start the CC1354P10 with `xds110reset`.

   ```text
   /Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c mspm0g5187/MSPM0G5187_<serial>.ccxml -e -f -v mspm0g5187/radio/out/radio_lab.out
   ```

   You should see: Both boards answer on their serial ports.

   If DSLite fails with `Error -242`, unplug the board's USB for 3 seconds and flash again.

5. **Mac, terminal.** Set the two serial ports and run the demo. It speaks "Hi Mom! This is Fox Red Labs, talking over a nine hundred fifteen megahertz radio. Over.", sends it in 50 ms frames, receives it and writes both recordings. The result is added to `site/data/radio-demos.json`.

   ```text
   LAB_G=/dev/cu.usbmodem<G5187 serial>1 LAB_P=/dev/cu.usbmodem<partner probe serial>1 python3 mspm0g5187/radio/demos.py voice
   ```

   You should see: The run reports 122 frames received. `site/media/radio-demos/voice-sent.wav` and `voice-received.wav` play the same message.

   If a frame is cut short, the CC1354P10's serial input dropped characters while it was sending; run it again.

### Expected result

The received recording plays the same message as the sent one.

### What this shows

- Speech compressed to 32 kbps survives a 100 kbps radio link: 122 of 122 frames arrived with a good checksum.
- A frame takes about 99 ms from computer to computer.

### Limits

- One way only. Two-way voice needs a speaker amplifier and a second CC1407P, and the board's microphone shares pins with the radio.
- Range was not measured: both boards were on one desk.
- One frame of 122 reached the radio already cut to 123 of its 205 bytes; the radio's checksum passed, so the air link did not corrupt it.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Codec and radio | IMA ADPCM, 4 bits, 8 kHz (32 kbps); Wi-SUN FSK mode 2b, 100 kbps, 915 MHz | `site/data/radio-demos.json` |
| Frames | 122 of 50 ms, 205 bytes each; delay median 99.2 ms, maximum 135.0 ms | `site/data/radio-demos.json` |
