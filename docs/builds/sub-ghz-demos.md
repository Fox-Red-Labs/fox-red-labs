# Sub-1 GHz home radio demos

A beacon you hunt by signal strength, a key finder you ring, and a house node for the mailbox, doorbell, panic button and keep-close tag. One pair of radios runs all three.

The page: [site/project-sub-ghz-demos.html](../../site/project-sub-ghz-demos.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Fox hunt, hot and cold

A beacon steps its power down and the hunter board reads hot, warm, cool or cold.

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

- Radio rules: the CC1407P transmits at +14 dBm unless you lower it. Check the 915 MHz limit in your country before running it.
- A Mac with CCStudio 21.0.1 (TI Arm Clang 5.1.1.LTS), MSPM0 SDK 2.11.00.07 with SysConfig 1.26.3, UniFlash 9.6.0, and for the CC1354P10 SysConfig 1.28.0 with the F2 SDK sources (`cc1354p10/radio-rx/README.md` shows how to fetch them). Checked on one Intel Mac.
- The code: `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git`, then `git sparse-checkout add mspm0g5187 cc1354p10 third-party portability tools mac-setup site`. Every command on this page runs from the repository folder.
- Back up both boards before the first flash with the command in each board's `AGENTS.md`.

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

3. **Mac, terminal.** Follow the Build section of `cc1354p10/radio-rx/README.md` to get the F2 SDK libraries and the `rfPacketRx` example, then build `partner.c` as that README describes, with the 2-GFSK 50 kbps setting.

   You should see: The build leaves `rfPacketRx.hex`, about 29 kB.

   If the make cannot find SysConfig, point `SYSCONFIG_TOOL` at `sysconfig_cli.sh` in the 1.28.0 folder, as the README shows.

4. **Mac, terminal.** Put each probe's serial in its `.ccxml` (`xdsdfu -e` lists them). Flash `radio_lab.out` to the LP-MSPM0G5187 and `rfPacketRx.hex` to the CC1354P10, then start the CC1354P10 with `xds110reset`.

   ```text
   /Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c mspm0g5187/MSPM0G5187_<serial>.ccxml -e -f -v mspm0g5187/radio/out/radio_lab.out
   ```

   You should see: Both boards answer on their serial ports.

   If DSLite fails with `Error -242`, unplug the board's USB for 3 seconds and flash again.

5. **Mac, terminal.** Set the two serial ports, then run the demo. The CC1407P beacon steps its power from +14 to -30 dBm in 8 steps, standing in for walking away; the CC1354P10 reports each step as hot, warm, cool or cold.

   ```text
   LAB_G=/dev/cu.usbmodem<G5187 serial>1 LAB_P=/dev/cu.usbmodem<partner probe serial>1 python3 mspm0g5187/radio/demos.py fox
   ```

   You should see: All 8 steps are heard, and the hunter's call moves from hot to cool.

   If a step shows no answers, check the wires and that the partner image matches the protocol.

### Expected result

All 8 steps are heard, and the hunter's call moves from hot to cool.

### What this shows

- The signal at the hunter falls from -34 dBm to -78 dBm as the beacon's power steps from +14 to -30 dBm, and the hunter calls it hot, warm, cool or cold on a fixed scale.

### Limits

- No one walked: the power steps stand in for distance, on one desk. Range was not measured.
- The scale is hot above -40 dBm, warm -60 to -40, cool -80 to -60 and cold below -80; nothing reached cold.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Steps | +14, +10, +4, 0, -6, -12, -20, -30 dBm; -34, -35, -41, -46, -56, -62, -65, -78 dBm at the hunter | `site/data/radio-demos.json` |

## Key finder: ring on demand

The base sends a ring command and the handset blinks and answers.

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

- Radio rules: the CC1407P transmits at +14 dBm unless you lower it. Check the 915 MHz limit in your country before running it.
- A Mac with CCStudio 21.0.1 (TI Arm Clang 5.1.1.LTS), MSPM0 SDK 2.11.00.07 with SysConfig 1.26.3, UniFlash 9.6.0, and for the CC1354P10 SysConfig 1.28.0 with the F2 SDK sources (`cc1354p10/radio-rx/README.md` shows how to fetch them). Checked on one Intel Mac.
- The code: `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git`, then `git sparse-checkout add mspm0g5187 cc1354p10 third-party portability tools mac-setup site`. Every command on this page runs from the repository folder.
- Back up both boards before the first flash with the command in each board's `AGENTS.md`.

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

3. **Mac, terminal.** Follow the Build section of `cc1354p10/radio-rx/README.md` to get the F2 SDK libraries and the `rfPacketRx` example, then build `partner.c` as that README describes, with the 2-GFSK 50 kbps setting.

   You should see: The build leaves `rfPacketRx.hex`, about 29 kB.

   If the make cannot find SysConfig, point `SYSCONFIG_TOOL` at `sysconfig_cli.sh` in the 1.28.0 folder, as the README shows.

4. **Mac, terminal.** Put each probe's serial in its `.ccxml` (`xdsdfu -e` lists them). Flash `radio_lab.out` to the LP-MSPM0G5187 and `rfPacketRx.hex` to the CC1354P10, then start the CC1354P10 with `xds110reset`.

   ```text
   /Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c mspm0g5187/MSPM0G5187_<serial>.ccxml -e -f -v mspm0g5187/radio/out/radio_lab.out
   ```

   You should see: Both boards answer on their serial ports.

   If DSLite fails with `Error -242`, unplug the board's USB for 3 seconds and flash again.

5. **Mac, terminal.** Set the two serial ports, then run the demo. The base sends "ring" 10 times; the CC1407P answers with the signal it heard and blinks its LEDs.

   ```text
   LAB_G=/dev/cu.usbmodem<G5187 serial>1 LAB_P=/dev/cu.usbmodem<partner probe serial>1 python3 mspm0g5187/radio/demos.py ring
   ```

   You should see: Ten rings, ten answers, and the handset's LEDs blink each time.

   If a step shows no answers, check the wires and that the partner image matches the protocol.

### Expected result

Ten rings, ten answers, and the handset's LEDs blink each time.

### What this shows

- A ring command reaches the handset and its answer returns in about 21 ms on the first send, 10 of 10 times.

### Limits

- On one desk at about -19 dBm; no walk and no obstacles.
- The timing is from the ring command written to the base's port to the handset's answer arriving on it.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Answers | 10 of 10, median 21 ms, maximum 21 ms; -19 dBm at both ends | `site/data/radio-demos.json` |

## Around the house: mailbox, doorbell, panic, messages, tag

One board is a mailbox sensor, doorbell, panic button, kid messenger and keep-close tag, and each event reaches Home Assistant.

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
| BP-BASSENSORSMKII Sensors BoosterPack plug-in module | 1 | the light sensor (mailbox) and the buttons |

### Before you start

- Radio rules: the CC1407P transmits at +14 dBm unless you lower it. Check the 915 MHz limit in your country before running it.
- A Mac with CCStudio 21.0.1 (TI Arm Clang 5.1.1.LTS), MSPM0 SDK 2.11.00.07 with SysConfig 1.26.3, UniFlash 9.6.0, and for the CC1354P10 SysConfig 1.28.0 with the F2 SDK sources (`cc1354p10/radio-rx/README.md` shows how to fetch them). Checked on one Intel Mac.
- The code: `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git`, then `git sparse-checkout add mspm0g5187 cc1354p10 third-party portability tools mac-setup site`. Every command on this page runs from the repository folder.
- Back up both boards before the first flash with the command in each board's `AGENTS.md`.
- A Home Assistant you can reach, with a long-lived access token, as for `range_ha.py` in `mspm0g5187/radio/README.md`.

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

3. **Mac, terminal.** Follow the Build section of `cc1354p10/radio-rx/README.md` to get the F2 SDK libraries and the `rfPacketRx` example, then build `partner.c` as that README describes, with the 2-GFSK 50 kbps setting.

   You should see: The build leaves `rfPacketRx.hex`, about 29 kB.

   If the make cannot find SysConfig, point `SYSCONFIG_TOOL` at `sysconfig_cli.sh` in the 1.28.0 folder, as the README shows.

4. **Mac, terminal.** Put each probe's serial in its `.ccxml` (`xdsdfu -e` lists them). Flash `radio_lab.out` to the LP-MSPM0G5187 and `rfPacketRx.hex` to the CC1354P10, then start the CC1354P10 with `xds110reset`.

   ```text
   /Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c mspm0g5187/MSPM0G5187_<serial>.ccxml -e -f -v mspm0g5187/radio/out/radio_lab.out
   ```

   You should see: Both boards answer on their serial ports.

   If DSLite fails with `Error -242`, unplug the board's USB for 3 seconds and flash again.

5. **Mac, terminal.** Set the two serial ports and the Home Assistant address and token as for `range_ha.py`, then run the demo. The Sensors BoosterPack's light sensor is the mailbox, S1 is the doorbell (held 2 s it sends panic) and S2 sends canned messages. A heartbeat every second is the keep-close tag, and the Mac sends parent replies. Each event is posted to Home Assistant.

   ```text
   HA_URL=http://homeassistant.local:8123 HA_TOKEN=<long-lived token> LAB_G=/dev/cu.usbmodem<G5187 serial>1 LAB_P=/dev/cu.usbmodem<partner probe serial>1 python3 mspm0g5187/radio/demos.py node --minutes 6 --reply
   ```

   You should see: Events show in Home Assistant as you trigger them: light the mailbox sensor, press S1, press S2.

   If a step shows no answers, check the wires and that the partner image matches the protocol.

### Expected result

Events show in Home Assistant as you trigger them: light the mailbox sensor, press S1, press S2.

### What this shows

- Radio events from a small board reach Home Assistant within 30 ms: mailbox, doorbell, panic and messages.
- A keep-close tag is a 1 s heartbeat; losing it for about 3 s raises an alarm.

### Limits

- One of 8 parent messages did not reach the board. The suspected cause is that the board listens in 80 ms windows between its own sends; it was not checked.
- On one desk; range was not measured.
- The 4-minute window closed 2 s after the panic button, so later S2 presses were not captured; the messages were rerun in a separate 90-second window.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Home Assistant delay | 12 to 28 ms per event | `site/data/radio-demos.json` |
| Tag | 235 heard of 237 sent | `site/data/radio-demos.json` |
