# Sensor readings over a sub-1 GHz radio

A plain MSPM0 with a CC1407P radio plug-in sends temperature, humidity and light at 915 MHz; a CC1354P10 LaunchPad receives them. Tested on one desk; range not measured.

The page: [site/project-sub-ghz-link.html](../../site/project-sub-ghz-link.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Sensor readings from an MSPM0 to a CC1354P10

An LP-MSPM0G5187 with the Sensors BoosterPack and the CC1407P radio sends; an LP-CC1354P10-1 receives. 915 MHz, 50 kbps.

Status: Works. Boards: g5187, cc1354p10.

Folders this version uses (in a sparse clone, fetch them first, then read `mspm0g5187/AGENTS.md`):

```bash
git sparse-checkout add mspm0g5187 cc1354p10 portability tools
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM0G5187 LaunchPad development kit | 1 | the sender's host |
| BP-BASSENSORSMKII Sensors BoosterPack plug-in module | 1 | temperature, humidity and light |
| BP-CC1407P-9-FE sub-1 GHz radio BoosterPack plug-in module | 1 | the sub-1 GHz radio |
| LP-CC1354P10-1 LaunchPad development kit | 1 | the receiver |
| ELEGOO jumper wires (120-piece pack) | 8 | radio to the LaunchPad kit's J20 header |

### Before you start

- A Mac with CCStudio 21.0.1 (TI Arm Clang 5.1.1.LTS), MSPM0 SDK 2.11.00.07 with SysConfig 1.26.3, UniFlash 9.6.0, and for the receiver SysConfig 1.28.0 with the F2 SDK sources (the steps fetch them). Checked on one Intel Mac.
- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version.
- Radio rules: the sender's amplifier is at its top setting. Check the power limit for 915 MHz in your country before running it outdoors, and lower it first.

### Wiring

Radio pins 1 to 10 run down the left outer row and 11 to 20 up the right outer row (20 at the top). Pins 23 and 36 are in the inner rows: 23 is the third socket down the left inner row, 36 the fifth down the right inner row. J20 is the 2x4 header by the microSD slot, printed PB12 PB10 PB8 PB6 over PB13 PB11 PB9 PB7. PB10 and PB11 also reach the board's audio converter; do not run the microphone examples while the radio is wired. Radio rules: the sender's amplifier is at its top setting, so check your country's 915 MHz power limit before powering it.

| Module pin | Board pin | What it is | Needed |
|---|---|---|---|
| Radio pin 1 (3V3) | J1 pin 1 (3V3), through the sensor pack | power | required |
| Radio pin 20 (GND) | J3 pin 22 (GND), through the sensor pack | ground | required |
| Radio pin 7 (SCLK) | J20 PB9 | SPI clock | required |
| Radio pin 15 (PICO) | J20 PB8 | data into the radio | required |
| Radio pin 14 (POCI) | J20 PB7 | data out of the radio | required |
| Radio pin 18 (CS_N) | J20 PB6 | chip select | required |
| Radio pin 36 (RESET_N) | J20 PB10 | radio reset | required |
| Radio pin 23 (SPI_INT) | J20 PB11 | radio says it has data | required |

### Steps

1. **Bench.** With USB unplugged, plug the Sensors BoosterPack onto the LP-MSPM0G5187. Screw the 915 MHz antenna onto the radio board's SMA socket. Join the radio to the LaunchPad kit's J20 header with the eight wires in the table: the LaunchPad has pins only on J1 and J3, which the sensor pack uses.

   You should see: Each wire's other end is on the pin in the table (the picture tags both ends).

   If the radio later answers with a checksum error on everything it receives, the inner-row wires (pins 23 and 36) are the usual missing ones.

2. **Terminal (Mac).** Check the power limit for 915 MHz in your country first: the sender runs at the amplifier's top setting, above what a license-free link normally allows; lower it before any outdoor run. Then, from the repository root, with the MSPM0 SDK 2.11, SysConfig and TI Arm Clang installed:

   ```text
   RADIO_APP=radio_tx RADIO_CFLAGS=-DHB_INT_POLL mspm0g5187/radio/build.sh
   ```

   You should see: The image `mspm0g5187/radio/out/radio_tx.out` (20.4 kB of flash).

   The script copies TI's SDK SPI example and patches the copy; if it cannot find the SDK, set the paths it prints.

3. **Terminal (Mac).** Put your probe serial in `mspm0g5187/MSPM0G5187_<probe serial>.ccxml` (`xdsdfu -e` lists it), flash, then open the board's first serial port at 115200 baud.

   ```text
   /Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c mspm0g5187/MSPM0G5187_<probe serial>.ccxml -e -f -v -u mspm0g5187/radio/out/radio_tx.out
   ```

   You should see: A banner `radio_tx: G5187 + Sensors BoosterPack -> CC1407P, 915 MHz 50 kbps 2-GFSK`, then one line a second: `tx FRL n=12 t=24.80C rh=36.9% lux=207.20 -> sent status=16 ...`.

   No `sent status=16` and a version error: check the wires; `diag.c` and `mosi.c` in the same folder test which wire is open.

4. **Terminal (Mac).** TI's receive example for the LP-CC1354P10-1, moved to 915 MHz with two files changed. Fetch the F2 SDK sources, build its libraries, then build the example; the full command list (about 5 minutes and 110 MB) is in `cc1354p10/radio-rx/README.md` under Build.

   ```text
   python3 cc1354p10/radio-rx/build_f2_libs.py f2sdk
   ```

   You should see: Libraries built (43 of 43 driverlib files); then `make` in the example's `ticlang` folder leaves `rfPacketRx.hex` (29.2 kB).

   CAN, SD card and microphone driver files fail to build; the receiver does not use them.

5. **Terminal (Mac).** Flash the hex with DSLite and the CC1354P10 connection file (your probe serial in it), then start it with the reset command; the flash does not reset the board.

   ```text
   /Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c cc1354p10/CC1354P10.ccxml -e -f -v -u rfPacketRx.hex
   xds110reset -s <probe serial>
   ```

   You should see: No output until you run the reset command; then open the receiver's first serial port at 115200 baud.

   No output: start it with the reset command; the flash does not reset the board.

6. **Terminal (Mac).** Watch the receiver's port. Each packet prints as hex; the text is the reading.

   You should see: Lines like `pkt 2 len=38 rssi=-14 status=0x00 data=46524c20...`, which reads `FRL n=292 t=25.21C rh=36.0% lux=208.48`, and every 14 s `count ok=350 crc_bad=0`.

   `crc_bad` rising or silence means the two settings disagree or the sender is not running.

7. **Terminal (Mac).** Set `HA_URL` and `HA_TOKEN` (a long-lived token from your profile), then run the bridge on the receiver's port.

   ```text
   python3 cc1354p10/radio-rx/ha_bridge.py --port /dev/cu.usbmodem<probe serial>1
   ```

   You should see: Four sensors appear: `sensor.frl_radio_temperature`, `sensor.frl_radio_humidity`, `sensor.frl_radio_light` and `sensor.frl_radio_signal`.

   Plain http is accepted only for a local or private address; otherwise use https.

### Expected result

The sender prints one `sent status=16` line a second. The receiver prints each packet with the readings as text and a signal near -14 dBm on one desk, and its checksum-error count stays at 0.

### What this shows

- A plain MSPM0 can host this radio: 21,658 of 21,658 packets over about 6 h 15 min, 0 checksum errors, at 915 MHz, 50 kbps, one desk.
- The same host sent all 23 of the radio's PHY profiles (345 of 345 transmit commands finished), and a CC1354P10 received 2-GFSK and the Wi-SUN FSK modes listed in mspm0g5187/radio/README.md.
- Readings reached Home Assistant through a bridge on the computer.

### Limits

- Range through a house or outdoors: not measured. Distance on the desk was not recorded.
- A Wi-SUN or other network stack: none is on the sender. The packets are plain text lines.
- Legal power: the sender runs at the amplifier's top setting, above what a license-free 915 MHz link normally allows.
- Battery life: not measured.
- TI ships no MSPM0 code for this radio; this host layer is this repository's own, around TI's driver.
- Checked on one Intel Mac only.
- The radio is a preview part, and the setup interface can change at release.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Link | 915 MHz, 50 kbps 2-GFSK, 37 to 39 byte text packets every 1.04 s | `mspm0g5187/radio/README.md` |
| Sender size | 20.4 kB of flash, 5.4 kB of RAM (TI's 24 SPI buffers) |  |
| Receiver size | 29.2 kB of flash |  |
| Radio firmware | 3.0.0.25, read 5 of 5 times |  |
| SPI clock | 1 MHz for this link; 1 to 8 MHz tested |  |
