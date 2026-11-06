# MSPM0G5187 sending sensor readings over TI's CC1407P sub-1 GHz radio

Terms: [docs/vocab.md](../../docs/vocab.md). 1 kB = 1,024 bytes.

**Short answer.** It works. An LP-CC1354P10-1 received 21,658 of 21,658 packets from this LP-MSPM0G5187 and its BP-CC1407P-9-FE radio over about 6 h 15 min, with no checksum errors, at -14 dBm on the same desk. Each packet carries temperature, humidity and light from a Sensors BoosterPack (BP-BASSENSORSMKII), every 1.04 s at 915 MHz. Range is not measured.

TI ships no MSPM0 code for this radio; TI's host driver from its Zephyr fork runs here with its driver files unchanged and its two Zephyr-only files replaced by this repo's bare-metal layer. Run 2026-10-10 and 11. The same host also runs all 23 PHY profiles in TI's `rf_diagnostics` sample for this radio: 345 of 345 packets sent. Each measured rate is within 3.4 percent of the rate TI states, for the 13 profiles where it states one ([below](#every-protocol-the-radio-has)).

Checked on an Intel Mac, macOS 15.7.9, with CCStudio 21.0.1, TI Arm Clang 5.1.1.LTS, MSPM0 SDK 2.11.00.07, SysConfig 1.26.3, UniFlash 9.6.0, and TI's driver from `simplelink-zephyr` tag `v3.7.0-ti-1.00.01_ga`.

What arrives at the receiver, decoded from its hex printout:

```text
FRL n=292 t=25.21C rh=36.0% lux=208.48
FRL n=293 t=25.17C rh=36.0% lux=208.56
```

The link in one line: sender LP-MSPM0G5187 plus BP-CC1407P-9-FE, receiver LP-CC1354P10-1 running TI's `rfPacketRx` example, 915 MHz, 50 kbps 2-GFSK (two-level frequency shift keying), text packets of 37 to 39 bytes. The receiver is in [cc1354p10/radio-rx](../../cc1354p10/radio-rx/README.md); the write-up is the [weather station experiment](../../docs/experiments/2026-10-10-mspm0g5187-cc1407p-weather-station.md).

## What it is

- **The radio:** BP-CC1407P-9-FE BoosterPack plug-in module, a CC1407P transceiver for 902 to 928 MHz with no processor for your code. A host drives it over SPI with six signals: clock, data in, data out, chip select, reset and an interrupt. It needs a 915 MHz SMA antenna; the one used here came in the box, and TI's parts list names none.
- **The host:** LP-MSPM0G5187, Arm Cortex-M0+ at 80 MHz, 128 kB flash, 32 kB RAM. The Sensors BoosterPack stays stacked on it.
- **Why wires:** this LaunchPad has header pins only on its J1 and J3 rows, and the sensor pack uses those. So the radio sits beside the stack. Six signals go to J20, the 2x4 header printed `PB12 PB10 PB8 PB6` over `PB13 PB11 PB9 PB7` by the microSD slot. Its pins PB6 to PB9 can be one SPI port, called UC3 in TI's tools. Power and ground come from the J1 and J3 pins that pass up through the sensor pack. Stacking needs a LaunchPad with all four header rows fitted. TI's supported hosts, such as the LP-CC1354P10-1, take it stacked with TI's TRX plug-in; that was not run here. From TI's pin maps the LP-MSPM0G3507 should take it stacked too, with its J9 and J16 jumpers off; not run.

## Wiring

| Radio pin | Signal | LP-MSPM0G5187 |
|---|---|---|
| 1 | 3V3 | J1 pin 1 (3V3), through the sensor pack |
| 20 | GND | J3 pin 22 (GND), through the sensor pack |
| 7 | SCLK, the clock | J20 PB9 |
| 15 | PICO, data into the radio | J20 PB8 |
| 14 | POCI, data out of the radio | J20 PB7 |
| 18 | CS_N, chip select | J20 PB6 |
| 36 | RESET_N | J20 PB10 |
| 23 | SPI_INT, "I have data" | J20 PB11 |

On the radio, pins 1 to 10 run down the left outer row and 11 to 20 run up the right outer row (20 at the top). Pins 23 and 36 are in the inner rows: 23 is the 3rd socket down the left inner row, 36 the 5th down the right inner row.

PB10 and PB11 also reach the board's audio converter (PCM1809) through 0 ohm resistors. It leaves them alone while no audio runs; do not run the microphone examples while the radio is wired.

## Build and run

```text
RADIO_APP=radio_tx RADIO_CFLAGS=-DHB_INT_POLL mspm0g5187/radio/build.sh
/Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c mspm0g5187/MSPM0G5187_MG020001.ccxml -e -f -v -u mspm0g5187/radio/out/radio_tx.out
```

Put your own probe serial in the `.ccxml` file (`xdsdfu -e` in CCStudio's `ccs_base/common/uscif/xds110/` lists it). In UniFlash's command line `-e` prints progress; it does not erase. Then open the board's first serial port at 115200 8N1, `/dev/cu.usbmodem<probe serial>1` on a Mac. That is the XDS110 debugger's backchannel UART. The banner is `radio_tx: G5187 + Sensors BoosterPack -> CC1407P, 915 MHz 50 kbps 2-GFSK`, then one line per packet:

```text
tx FRL n=12 t=24.80C rh=36.9% lux=207.20 -> sent status=16 ev=0x0000004c sent=12 failed=0
```

`status=16` is the radio's "finished" status for the transmit command. Without `RADIO_APP`, the script builds `radio_host.c`, which only reads the radio's firmware version every 3 s. `HB_INT_POLL` adds a 10 ms check of the interrupt line besides the edge interrupt; every run here used it, including the six-hour one. It went in while the interrupt wire was missing. Once the wires were right, the trace shows the edge interrupt firing on its own, but no run without the check was made. `-DHB_TRACE` prints each SPI exchange.

| File | What it is |
|---|---|
| `radio.syscfg` | SysConfig: the sensor reader's pins plus UC3 SPI on J20, chip select, reset and interrupt |
| `hooks_mspm0g5187.c` | The eight functions TI's driver needs from a board, in DriverLib (TI's low-level MSPM0 library): start an SPI transfer, set chip select, read the data-out line, set reset, turn the interrupt on or off, wait some microseconds, start and stop a one-shot timer |
| `radio_tx.c` | The sender: TI's `ti_cc140xp_genfsk_tx` sample's sequence, its 50 kbps settings, and the HDC2080 and OPT3001 readings as text |
| `radio_host.c` | Opens the driver and reads the radio's firmware version |
| `build.sh` | Copies TI's SDK SPI example folder, adds this repo's files and TI's driver, builds, and leaves the image in `out/` |
| `diag.c`, `mosi.c` | The wire checks used at the bench: which pin really resets the radio, and whether the data line reaches it |
| `phy_matrix.c` | Every CC1407P protocol in turn, with timing; flags `-DSPI_SWEEP` (SPI clock sweep), `-DRX_CHECK` (loop four protocols for a receiver), `-DNV_TEST` (the storage test) |
| `radio_lab.c` | The CC1407P as a radio driven by text commands: profile, frequency, power, send, receive, ping, echo, RSSI scan, carrier sense (list at the top of the file) |
| `lab.py` | Sends commands to the G5187 and the CC1354P10 partner and logs both ports: `lab.py <log> "g:P 13" "p:E" "g:G 20 30 400 60"`; set `LAB_G` and `LAB_P` to your two serial ports (the defaults are this bench's) |
| `demos.py` | The end-user demos: walkie-talkie, fox hunt, key finder, the house node and the battery estimate, with results for the demos page |
| `range_ha.py` | The live range test: pings in rounds and posts packet error rate, signal at both ends and round trip to Home Assistant |
| `log_both.py` | Logs the sender's and receiver's serial ports at once with timestamps: `log_both.py <seconds> <folder> [sender port] [receiver port]`; the default ports are this bench's, so pass your own |

## Numbers

| What | Value | How |
|---|---|---|
| Packets received | 21,658 of 21,658, 0 checksum errors, about 6 h 15 min ending 2026-10-11 06:00:48 | the receiver radio's own counter against the sender's sequence number at the same second; plus or minus one at the start, because the receiver's first printed packet was 292 |
| One hour, both ends logged | 3,508 of 3,508, 0 checksum errors, 05:58:56 to 06:59:37 | Both ends logged with `log_both.py` |
| Signal at the receiver | -14 dBm, range -15 to -14 | RSSI (received signal strength) the CC1354P10 appends to each packet; boards on one desk, distance not measured |
| Firmware read | 5 of 5, version 3.0.0.25, hash 0x8352f9e3 | First serial run |
| Sender image | 20.4 kB of 128 kB flash, 5.4 kB of 32 kB RAM | `tiarmsize`; the build above (`HB_INT_POLL` on, trace off), TI's default 24 SPI buffers |
| SPI clock | 1 MHz for the weather station; 1 to 8 MHz tested | SysConfig; the sweep is in "How fast the host can feed it" |

## Every protocol the radio has

Terms used in the table (Wi-SUN, OFDM, MCS, O-QPSK, DSSS, XLR) are in the [vocabulary](../../docs/vocab.md#sub-1-ghz-radio-protocols-cc1407p). All 23 of the PHY profiles (the radio's modulation and packet settings) that TI's `rf_diagnostics` sample ships for the CC1407P run from this MSPM0 host: 345 of 345 transmit commands finished, 15 per profile. The build is `RADIO_APP=phy_matrix RADIO_CFLAGS="-DHB_INT_POLL -DMATRIX_PERSIST=1 -DRESET_ON_FAMILY=1 -DRESET_EACH=1"`; the radio is reset before every profile. Run 2026-10-11 at 1 MHz SPI.

The measured rate checks that the radio really ran each profile. It is the 180-byte difference between a 20-byte and a 200-byte packet, divided by the difference in their transmit times, so the fixed costs cancel. TI states a rate for 13 of the profiles: in the CC1407P settings headers, in the driver's name for O-QPSK rate mode 3, and in SysConfig's name for Wi-SUN mode 4b. The measured rate is within 1.2 percent of it for 11 of the 13 and within 3.4 percent for all 13. The packet time is measured from the host: submit command to the radio's "finished" event. So it includes fixed costs: the SPI messages, and the radio's preamble and header, which are long on the spread modes. For OFDM the fixed part, the 20-byte time less the 20 bytes' own time at the measured rate, is 2.4 to 3.1 ms, so read the 200-byte column as host-view time, not air time. The receiver is an LP-CC1354P10-1 with SysConfig's own setting for that protocol, 75 s per setting. OFDM, OFDM-LR and XLR need a second CC1407P to receive.

| Profile | TI's stated rate, kbps | Measured rate, kbps | 200-byte packet, ms | CC1354P10 receives it |
|---|---|---|---|---|
| Wi-SUN FSK mode 2b | 100 | 99.1 | 18.7 | yes, 260 of 260 sent |
| Wi-SUN FSK mode 4b | 200 | 198 | 10.1 | yes, 260 of 260 sent |
| Wi-SUN OFDM option 1 MCS6 | not stated | 2,175 | 3.1 | no modem for it |
| Wi-SUN OFDM option 2 MCS3 | not stated | 393 | 6.9 | no modem for it |
| Wi-SUN OFDM option 2 MCS4 | not stated | 579 | 5.5 | no modem for it |
| Wi-SUN OFDM option 2 MCS5 | not stated | 766 | 4.9 | no modem for it |
| Wi-SUN OFDM option 2 MCS6 | not stated | 1,141 | 4.2 | no modem for it |
| Wi-SUN OFDM option 3 MCS4 | not stated | 297 | 8.3 | no modem for it |
| Wi-SUN OFDM option 3 MCS5 | not stated | 393 | 7.0 | no modem for it |
| Wi-SUN OFDM option 3 MCS6 | 600 | 583 | 5.6 | no modem for it |
| Wi-SUN OFDM option 4 MCS4 | not stated | 149 | 13.8 | no modem for it |
| Wi-SUN OFDM option 4 MCS5 | not stated | 198 | 11.1 | no modem for it |
| Wi-SUN OFDM option 4 MCS6 | not stated | 295 | 8.4 | no modem for it |
| 2-GFSK 50 kbps high performance | 50 | 49.7 | 35.2 | yes, 10 of 10 (20 B), and the weather station |
| 2-GFSK 38.4 kbps | 38.4 | 38.2 | 45.4 | no matching setting found |
| 2-GFSK 9.6 kbps | 9.6 | 9.56 | 178.4 | no matching setting found |
| General purpose GFSK | 50 | 49.7 | 35.1 | yes, 10 of 10 (20 B) |
| 2-GFSK 50 kbps low power | 50 | 49.7 | 35.2 | yes, 10 of 10 (20 B) |
| SUN O-QPSK rate mode 3 | 50 | 48.3 | 56.5 | no: 260 sent, every reception had a bad checksum |
| OFDM-LR 8.33 ksps DSSS 6 | 0.69 | 0.69 | 2,432.2 | no modem for it |
| OFDM-LR 16.67 ksps DSSS 6 | 1.39 | 1.38 | 1,216.7 | no modem for it |
| OFDM-LR 66.67 ksps DSSS 2 | 16.67 | 16.6 | 107.4 | no modem for it |
| XLR 20 ksps DSSS 2 | 5.00 | 4.98 | 344.3 | no modem for it |

Rows: `radio_airtime` and `radio_cmd_success` in `data/results/mspm0g5187.csv`.

## How fast the host can feed it

The radio is fastest on Wi-SUN OFDM option 1 MCS6, about 2.2 Mbps measured. At 1 MHz SPI, handing it one 200-byte packet takes 4.05 ms; at that rate the radio needs about 0.74 ms for the payload (computed). So on this mode the host link, not the radio, sets the packet rate. The packet rate was measured on this mode and on 50 kbps GFSK only. At 1 MHz SPI, by the same arithmetic, every OFDM mode whose 200-byte host-view time is under 4.05 ms in the table above is host-limited too. The sweep sends 20 packets back to back at each SPI clock, each stored, submitted and waited for (build flag `-DSPI_SWEEP`).

| SPI clock | OFDM MCS6, 200 B: payload kbps | Packets a second | Handing over one packet, ms | SPI busy, ms | Bits on the wire as a share of SPI busy time (computed) | 2-GFSK 50 kbps, 60 B: payload kbps |
|---|---|---|---|---|---|---|
| 1 MHz | 217 | 135 | 4.05 | 3.28 | 78% | 33 |
| 2 MHz | 261 | 163 | 2.90 | 1.98 | 65% | 34 |
| 4 MHz | 290 | 181 | 2.34 | 1.34 | 48% | 35 |
| 8 MHz | 306 | 191 | 2.06 | 1.02 | 31% | 35 |

The share column counts the bits in the 320 bytes of SPI traffic for each 200-byte packet, counted in the matrix run: storing it, submitting it and reading the radio's replies. 320 of 320 packets sent: 60 and 200 bytes on both profiles at each clock, of which the table shows 8 rows. The receiver checked the 80 GFSK packets of 60 bytes, all good. Going from 1 to 8 MHz buys 41 percent more OFDM throughput, not eight times: at 8 MHz about two thirds of the SPI busy time is not wire time (suspected: the driver's byte-by-byte polling and its wait on the radio's ready line; not profiled). 50 kbps GFSK does not change, because its airtime dominates. 12 MHz, the radio's limit at 3.3 V, is not reachable from a 32 MHz bus clock (the divider gives 8 or 16). Speed options are DMA instead of polling, and storing the next packet while the radio sends the last one.

## Two radios: the CC1407P against a CC1354P10

The CC1407P also receives, answers and sends pings, and senses the channel, from this MSPM0 host. `radio_lab.c` turns the G5187 into a radio driven by text commands on its serial port, and [`partner.c`](../../cc1354p10/radio-rx/partner.c) does the same for the LP-CC1354P10-1, built once per protocol. `lab.py` on the Mac drives both. A ping is a 30-byte packet defined in this repo: `FRL PING <n>`, answered with `FRL PONG <n> r=<signal it was heard at>`. Terms: [vocabulary](../../docs/vocab.md#sub-1-ghz-radio-protocols-cc1407p).

Run 2026-10-11, boards on one desk, 915 MHz, two runs; the table is the second. Each side sent 20 packets of 40 bytes and 20 pings; every send reported success.

| Protocol | CC1407P receives, of 20 | CC1354P10 receives, of 20 | Pings from the CC1407P answered, of 20 | Round trip | Pings from the CC1354P10 answered, of 20 |
|---|---|---|---|---|---|
| 2-GFSK 50 kbps | 20 | 20 | 20 | 17.9 ms | 20 |
| Wi-SUN FSK 1a | 20 | 20 | 20 | 19.2 ms | 20 |
| Wi-SUN FSK 1b | 20 | 20 | 20 | 19.2 ms | 20 |
| Wi-SUN FSK 2a | 20 | 20 | 9 | 11.7 ms | 20 |
| Wi-SUN FSK 2b | 20 | 20 | 17 | 11.6 ms | 20 |
| Wi-SUN FSK 3 | 20 | 20 | 20 | 9.2 ms | 20 |
| Wi-SUN FSK 4a | 16 | 20 | 4 | 8.2 ms | 14 |
| Wi-SUN FSK 4b | 20 | 20 | 20 | 8.1 ms | 20 |
| Wi-SUN FSK 5 | 6 | 20 | 11 | 7.1 ms | 3 |
| XLR against SimpleLink Long Range, 5 and 2.5 kbps (two pairs) | 0 | 0 | 0 | | 0 |

- **Packets went 20 of 20 both ways in seven protocols**: 2-GFSK 50 kbps and Wi-SUN FSK 1a, 1b, 2a, 2b, 3 and 4b. The CC1354P10 received 20 of 20 in all nine.
- **Modes 4a and 5 lose packets from the CC1354P10 to the CC1407P**, in both runs (14 and 16, then 6 and 6). The CC1407P's sync counter (packets whose start it found) equals its checksum counter, so the missed packets are never detected.
- **Pings are partial at 2a, 2b, 4a and 5.** In run 2 the signal was about 5 dB lower from mode 2a on (about -21 to -24 dBm against -18), and pings from the CC1354P10 at mode 5 fell from 9 of 20 in run 1 to 3.
- **XLR and SimpleLink Long Range exchanged no packet.** The CC1407P decoded the header 7 and 16 times and passed no checksum; the CC1354P10 decoded nothing. The two run at different rates (XLR 20 ksps with spreading 2, SimpleLink Long Range 5 and 2.5 kbps), and no TI document that pairs them was found.
- **Pings from the CC1407P use TI's command chaining**: its transmit in command slot 0 chains to its receive in slot 1 on the radio. In run 1 the receive was started by a host command after each ping, and only the 50 kbps modes got answers; the suspected cause is that the answer came before the receive started.
- **Round trip** is host time on the G5187: from handing the ping to the radio, including its SPI transfer, to the answer arriving, so it includes the CC1354P10's turnaround. It is 7 to 19 ms and shorter at higher rates.

### Live range test in Home Assistant

`range_ha.py` pings in rounds of 10 and posts five sensors: `sensor.frl_range_packet_error_rate` (over the last 100 pings), `sensor.frl_range_signal_at_cc1407p`, `sensor.frl_range_signal_at_cc1354p10`, `sensor.frl_range_round_trip` and `sensor.frl_range_pings`. On the desk: 1,620 of 1,620 pings in 3 minutes, -22 to -23 dBm at the CC1407P and -21 to -23 dBm at the CC1354P10, 17.9 ms round trip. The yard walk is next. Flash `radio_lab.c` and the 50 kbps `partner.c` first, then:

```text
LAB_G=/dev/cu.usbmodem<G5187 probe serial>1 LAB_P=/dev/cu.usbmodem<CC1354P10 probe serial>1 \
HA_URL=http://homeassistant.local:8123 HA_TOKEN=<long-lived token> \
python3 mspm0g5187/radio/range_ha.py --minutes 10 --log range_ha.log
```

### Listening before talking, power steps and a band scan

- **Carrier sense answered correctly at the low thresholds.** The driver's `TRX.h` calls the command "not functional". With nothing on the air it reported idle at -60 and -80 dBm and busy at -100 and -110 dBm, below the noise floor. With the CC1354P10 transmitting back to back, -60 dBm read busy all three times and idle again after. At -10 dBm it read idle, as expected for a signal near -19 dBm (the CC1407P's +14 dBm measured at the CC1354P10; the reverse was not measured). At -30 dBm it also read idle, which is not explained; the check samples 4 readings, with gaps between the interferer's packets. About 5 ms per check from the host.
- **Transmit power steps mostly as asked.** The low-power amplifier moved the signal at the CC1354P10 by 42 dB for 44 dB requested, -30 to +14 dBm. Three pairs read the same: -10 and -6 dBm on the low amplifier, 0 and 4 dBm and 20 and 21 dBm on the high one. Relative only; no power meter.
- **Band scan:** the CC1407P's own RSSI (received signal strength) across 902 to 928 MHz, 53 steps, averaged -88 to -97 dBm indoors, no reading above -79.

Keep the boards apart or the power low with both radios on one desk: the CC1354P10's sub-1 GHz input is rated +10 dBm maximum (data sheet SWRS267B), and the CC1407P can transmit +21 dBm.

## End-user demos

The same two radios run five demos a family would recognize; each result is on the [radio demos page](../../site/radio-demos.html), with the walkie-talkie's two recordings to play. `demos.py` runs and measures each one and writes `site/data/radio-demos.json`; `tools/render_radio_demos.py` draws the page. Run 2026-10-11, boards on one desk, +14 dBm on one channel. That is over the US limit for a fixed-channel link (47 CFR 15.249, about -1 dBm); this is a home build of fewer than five units, run on the bench, and any recipe for readers will use legal power.

| Demo | What it does | Result |
|---|---|---|
| Walkie-talkie, one way | Speech from the Mac, IMA ADPCM at 32 kbps in 50 ms frames, sent by the CC1354P10 on Wi-SUN FSK 2b, received by the CC1407P | 122 of 122 frames, 99 ms typical delay computer to computer; the recordings match except 40 ms where one frame was cut on the sender's serial input |
| Fox hunt | The CC1407P beacon steps its power down; the CC1354P10 turns the signal into hot, warm, cool or cold | 64 of 64 beacons heard, -34 to -78 dBm, hot to cool |
| Key finder | The base sends "ring"; the handset answers, then blinks its LEDs | 10 of 10 on the first send, 21 ms |
| Around the house | Mailbox (light sensor), doorbell and panic (S1), kid messages (S2), parent replies, keep-close tag, all into Home Assistant | mailbox, doorbell, panic and 6 kid messages all seen, each in Home Assistant within 30 ms of reaching the base; 7 of 8 parent messages shown (one missed, cause not known); 235 of 237 heartbeats; tag alarm 3.0 s after the tag stopped |
| Far shed sensor | Battery life on two AA cells by report interval | computed: 0.8 years at one report a second, 7.5 years at one every 10 s |

Two things changed the plan. The G5187's onboard microphone sends its audio on PB10 and PB11, the radio's reset and interrupt wires, so the walkie-talkie's voice comes from the Mac; and the Sensors BoosterPack's motion sensor does not answer on this board, so the tracker is a keep-close tag, not a motion or GPS tracker.

```bash
python3 mspm0g5187/radio/demos.py voice      # needs partner.c for Wi-SUN 2b on the CC1354P10
python3 mspm0g5187/radio/demos.py ring       # the others need partner.c at 50 kbps
python3 mspm0g5187/radio/demos.py node --minutes 6 --reply
python3 tools/render_radio_demos.py
```

## If you see this

- **The radio answers but every reply is a checksum error** (`04 02 f0 00` in its message). The data line into the radio is not connected: the wire used here was in pin 16, one socket above pin 15. `mosi.c` forces PB8 high, then low; if the radio's reply does not change, the line is open. FAQ: [the radio answers but every reply is a checksum error](../FAQ.md).
- **Holding the reset pin low does not stop the radio answering, and its interrupt never fires.** The reset and interrupt wires are not in pins 36 and 23, the two inner-row pins. `diag.c` holds each candidate pin low for 200 ms and talks to the radio while it is held. FAQ: [holding the reset pin low does not stop it answering](../FAQ.md).
- **The receiver's printout skips packets but its count line does not.** Trust the `count ok=` line; see the [receiver FAQ](../../cc1354p10/FAQ.md).
- **A transmit on a new protocol ends with status 0x8A (138, unknown operation).** The radio appears to keep the image of its first transmit since its last reset; the logs show only that a reset cures it. Changing modem (FSK, OFDM, O-QPSK) or RF mode needs a radio reset: close and reopen the driver, about 50 ms. Storing the device config or calling `utilSetRfMode` did not switch it.
- **`storeConfig` never completes after a few profile changes.** Probably the radio's configuration storage fills: it took four persisted copies of a 1,776-byte config. The fifth persist returned in 1 ms instead of 13; from the sixth on, the store's completion event had not arrived after 5 s. Capacity in bytes is not measured. Call `TRX_Host_eraseNv` before storing a new set: that worked in the 23-profile run, which erases at every reset; erase alone was not tested.
- **200-byte packets come back `TRX_Host_No_Resources` (status 2) but 20-byte ones go.** The suspected cause is that a config stored but not persisted keeps holding the radio's buffer space; the run shows refusals without the persist and none with it. Persist each config right after storing it.
