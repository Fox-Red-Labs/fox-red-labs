<!-- experiment
id: E181
board: LP-MSPM0G5187
date: 2026-10-11
status: works
reached: verified
line: On transmit, all 23 of TI's CC1407P PHY profiles ran from the MSPM0G5187. That is 345 of 345 packets, each within 3.4 percent of TI's stated rate. Receive was checked where a CC1354P10 setting exists: Wi-SUN FSK 2b and 4b 260 of 260 each. SUN O-QPSK: 0 passed the checksum. Cause suspected: packet format. At 1 MHz SPI the host, not the radio, limits the fastest OFDM mode; 8 MHz gives 41 percent more.
stop: none
-->
# Can an MSPM0 host run every protocol TI's CC1407P has, and keep up on the fastest?

**Short answer.** Yes for all 23 protocol settings in TI's CC1407P `rf_diagnostics` sample: from 0.69 kbps OFDM long range to Wi-SUN OFDM at about 2.2 Mbps, 345 of 345 packets from an LP-MSPM0G5187 host, each within 3.4 percent of the rate TI states, for the 13 where it states one. The radio needs a reset between protocol families, and its configuration storage took only four persisted copies of a 1,776-byte config. An LP-CC1354P10-1 with SysConfig's own settings received the two Wi-SUN FSK modes, 260 of 260 each, but none of the 260 SUN O-QPSK packets. The host's packet rate was measured on two profiles. On the fastest, Wi-SUN OFDM option 1 MCS6, handing over a 200-byte packet takes 4.05 ms at 1 MHz SPI and 2.06 ms at 8 MHz, both longer than the about 0.74 ms the radio needs for that payload (computed from the measured rate). There the host, not the radio, sets the rate.

Terms: [vocab](../vocab.md#sub-1-ghz-radio-protocols-cc1407p) (PHY, 2-GFSK, Wi-SUN, OFDM, MCS, O-QPSK, DSSS, OFDM-LR and XLR; SPI and CRC earlier on that page). Code and tables: [mspm0g5187/radio](../../mspm0g5187/radio/README.md#every-protocol-the-radio-has); receiver: [cc1354p10/radio-rx](../../cc1354p10/radio-rx/README.md#other-protocols-from-the-same-radio). The link and host are the ones in [the CC1407P weather station page](2026-10-10-mspm0g5187-cc1407p-weather-station.md). Checked on an Intel Mac, macOS 15.7.9, TI Arm Clang 5.1.1.LTS, MSPM0 SDK 2.11.00.07, F2 SDK 8.33.00.16 with SysConfig 1.28.0 (receiver), radio firmware 3.0.0.25, TI's driver from `simplelink-zephyr` tag `v3.7.0-ti-1.00.01_ga`.

**Question.** A customer picks a sub-1 GHz radio for its protocols. Does a plain MSPM0 drive all of them, and where does the host become the limit?

**Vendor position.** TI's `rf_diagnostics` sample for the CC1407P ships 23 PHY settings. They are Wi-SUN FSK modes 2b and 4b, Wi-SUN OFDM options 1 to 4 at several MCS levels, five 2-GFSK settings, SUN O-QPSK, three OFDM long-range settings and XLR. MCS is the modulation and coding scheme. The radio takes SPI up to 12 MHz at 3.3 V (data sheet SWRS301). No MSPM0 host is listed.

**What was tried.**

- **Every profile.** `phy_matrix.c` stores each profile's settings, sends 10 packets of 20 bytes and 5 of 200, and times each from the host: store, submit, and the radio's "finished" event. The bit rate is the 180-byte difference over the difference in transmit times, so fixed costs cancel.
- **Switching.** Ten runs found what switching needs: a radio reset before a change of modem or RF mode, an erase of the radio's storage before it fills, and a persist after every store.
- **Receive.** A loop of four protocols, 20 numbered packets each, against the CC1354P10 with SysConfig's matching setting, 75 s per setting. The Wi-SUN settings needed TI's advanced receive command for the 802.15.4g header.
- **SPI clock.** 20 packets back to back at 1, 2, 4 and 8 MHz on the fastest OFDM mode (200 bytes) and on 50 kbps GFSK (60 bytes, checked by the receiver).

**How far it got.** Verified on the boards:

| Step | Result |
|---|---|
| All 23 profiles | 345 of 345 finished; rates within 1.2 percent of TI's for 11 of the 13 it states, 3.4 percent for all 13 |
| Reset between profiles | 48 to 53 ms each; without it, 20 transmits ended 0x8A, on the five profiles that changed modem or RF mode |
| Wi-SUN FSK 2b and 4b to the CC1354P10 | 260 of 260 sent each (13 batches of 20 in the sender's log), 13 complete batches decoded, 0 checksum errors, -20 dBm |
| SUN O-QPSK to the CC1354P10 | 220 receptions, all with a bad checksum |
| SPI sweep | 320 of 320 sent (16 rows of 20); OFDM 200 B 217 to 306 kbps from 1 to 8 MHz; GFSK 80 of 80 received |
| Radio storage | 4 persisted 1,776-byte configs; the 5th persist returns in 1 ms instead of 13, and from the 6th the store has not completed after 5 s |

**The line.** None for transmit. Two limits on the way:

- **The host link, for fast OFDM.** At 1 MHz SPI the host spends 3.28 ms with the SPI busy per 200-byte packet, and the bits themselves (320 bytes on the wire) take 78 percent of that (computed). At 8 MHz the busy time falls to 1.02 ms, but the bits take only 31 percent of it (computed). The rest is not wire time (suspected: the driver's byte-by-byte polling and its wait on the radio's ready line; not profiled). So 8 MHz gives 41 percent more throughput, not eight times. 12 MHz is not reachable from the G5187's 32 MHz bus clock with this SPI port's divider.
- **Receive coverage, for O-QPSK.** SysConfig's SUN O-QPSK setting for the CC1354P10 generates a receive command for TI's proprietary frame (sync word 0x930B51DE, 1-byte length), while the CC1407P's driver names its header for 802.15.4 SUN O-QPSK. A frame mismatch is the suspected cause; no sniffer checked it.

**Ask.**

- **Open question:** has anyone found written down that a modem or RF-mode change needs a radio reset, how much configuration storage the radio has, and that a stored config holds buffer space until persisted?
- **For readers:** if you run the OFDM modes from an MSPM0 with DMA SPI, report the packet rate you reach.
