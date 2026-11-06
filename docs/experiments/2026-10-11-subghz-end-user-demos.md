<!-- experiment
id: E183
board: LP-MSPM0G5187
date: 2026-10-11
status: works
reached: verified
line: All five demos work as built. One-way voice passed 122 of 122 frames. Fox hunt heard 64 of 64 beacons, hot to cool. The key finder answered 10 of 10 in 21 ms. The house node sent mailbox, doorbell, panic, kid messages and a keep-close tag into Home Assistant. The fifth is a computed battery estimate. Two-way voice needs a speaker and a second CC1407P. The board's microphone shares pins with the radio wires.
stop: none
-->
# Can one TI radio and an MSPM0 run family demos: walkie-talkie, fox hunt, key finder, smart house?

**Short answer.** Yes, on the bench. The CC1407P on an LP-MSPM0G5187, with an LP-CC1354P10-1 as the other end, ran five demos and each has a result on the [radio demos page](../../site/radio-demos.html). A spoken message went over the air in 122 of 122 frames and plays back the same as it was sent, except for 40 ms where one frame was clipped before sending. A fox hunt read hot to cool as the beacon stepped down. A key finder answered 10 of 10 rings in 21 ms. One board acted as mailbox, doorbell, panic button, kid messenger and keep-close tag, and every event reached Home Assistant within 30 ms of reaching the base. All of it ran at +14 dBm on one channel, over the US limit for that kind of link (47 CFR 15.249, about -1 dBm); it is a home build of fewer than five units, kept on the bench. A version for readers needs legal power.

Terms: [vocab](../vocab.md#sub-1-ghz-radio-protocols-cc1407p). Code: `mspm0g5187/radio/radio_lab.c`, `cc1354p10/radio-rx/partner.c`, `mspm0g5187/radio/demos.py`; results `site/data/radio-demos.json`. Checked on an Intel Mac, macOS 15.7.9, TI Arm Clang 5.1.1.LTS, MSPM0 SDK 2.11.00.07, F2 SDK 8.33.00.16, radio firmware 3.0.0.25.

**Question.** Beyond lab tests, what would a parent or hobbyist build with this radio, and does it work from an MSPM0?

**Vendor position.** TI ships no end-user demos for the CC1407P; its examples are transmit, receive, mode switching and diagnostics. The ideas came from a survey of maker projects and competitor kits.

**What was tried.**

- Walkie-talkie, one way. The Mac speaks a line and compresses it to 4-bit ADPCM at 32 kbps in 50 ms frames. The CC1354P10 sends each frame on Wi-SUN FSK mode 2b. The CC1407P receives, and the Mac plays it back. The G5187's own microphone could not be used, because it sends audio on PB10 and PB11, the radio's reset and interrupt wires.
- Fox hunt. The CC1407P beacon steps its power from +14 to -30 dBm in 8 steps, standing in for walking away. The CC1354P10 turns the signal into hot, warm, cool or cold.
- Key finder. The base sends "ring"; the CC1407P answers with the signal it heard, then blinks its LEDs.
- Around the house. The Sensors BoosterPack's light sensor is the mailbox. S1 is the doorbell, and holding it 2 s sends panic. S2 sends canned kid messages. A heartbeat every second is the keep-close tag. The Mac sends parent replies, and the base posts each event to Home Assistant.
- Far shed sensor. Battery life computed from the measured send time and TI's data sheet currents.

**How far it got.** Verified on the boards, 2026-10-11.

| Demo | Result | Evidence |
|---|---|---|
| Walkie-talkie | 122 of 122 frames with good checksums; 121 exactly as sent; 99 ms typical delay computer to computer; recordings identical except 40 ms | `site/data/radio-demos.json`, `site/media/radio-demos/`. |
| Fox hunt | 64 of 64 beacons heard, -34 to -78 dBm | Same file. |
| Key finder | 10 of 10 on the first send, 21 ms | Same file. |
| Around the house | mailbox twice, doorbell, panic, 6 kid messages, each in Home Assistant within 30 ms; 7 of 8 parent messages reached the board; 235 of 237 tag heartbeats; tag alarm 3.0 s after the tag went quiet | Same file. |
| Far shed sensor | computed: 0.8 years on two AA cells at one report a second, 7.5 years at one every 10 s | Same file. |

**The line.** None for the demos as built. Two limits:

- **Two-way voice.** The microphone path and the radio wires share PB10 and PB11, and neither board has a speaker; a speaker amplifier and a second CC1407P would make it two-way.
- **One parent message of 8 did not reach the board.** The suspected cause is that the board listens in 80 ms windows between its own sends, so a message that arrives while it sends can be missed; not checked.

**Ask.**

- **Open question:** would end-user examples like these, on an MSPM0 host, be useful to makers?
- **For readers:** which of these would you build first?
