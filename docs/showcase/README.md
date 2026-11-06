# TI SDK examples worth running, where each runs, and what can run together

**Short answer.** About one in six TI examples is worth running for its own sake. About 1.5k distinct examples (1,454) were read across the six SDK families for the boards here. 331 rows are showcase demos, about 260 once one demo's variants are merged; 994 show one peripheral (building blocks, not demos); 129 are plumbing (bootloaders, tests, tools). About 70 score high on both "would you show a friend" and "is it the start of a real project". The best portability story is TI's own AI demos: arc fault and motor fault ship, by name, in C2000Ware, the AM13E2x SDK, the MSPM33 SDK and (as twins) the MSPM0 SDK, so one demo can be compared across four families. TI ships almost nothing chained: the one combined Wi-Fi image is the Matter app, Linux-only and without MQTT, and Bluetooth plus Zigbee at once (DMM) exists on the CC1354P10 and, as BLE plus 15.4, on the CC2755P10.

Checked 2026-10-08. Scores (cool and useful, 1 to 5 each) are judgment calls, one reader per family, and port routes are estimates until built. Terms: [vocab](../vocab.md).

## The counts

| Family (SDK) | Boards | Examples | Showcase | Block | Plumbing |
|---|---|---|---|---|---|
| MSPM0 (SDK 2.11) | G3507, L1306, C1104, G3519, G5187 | 443 | 115 | 281 | 47 |
| C2000 (C2000Ware 26.02) | F2800137, F28P65X | 388 | 41 | 311 | 36 |
| MSPM33 and AM13 | MSPM33C321A, AM13E230 | 348 | 44 | 286 | 18 |
| CC13xx (F2 SDK 8.33) | CC1354P10 | 152 | 100 (about 30 demos; one row per role or band variant) | 43 | 9 |
| CC23xx and CC27xx (F3 SDK 9.21) | CC2340R5, CC2755P10 | 91 | 25 | 51 | 15 |
| CC35xx (Wi-Fi SDK 10.20) | CC35X1 | 32 | 6 | 22 | 4 |
| **Total** | | **1,454** | **331** | **994** | **129** |

## The picks: twelve demos people would want, and where they run

| Demo | What you see | Native on | Port route (estimate) | Extra hardware |
|---|---|---|---|---|
| Wake word "OK Kilby" (NPU) | Say it at the microphone, an LED lights | G5187 | NPU-bound; CPU twin is a same-SDK swap | analog microphone (on the board) |
| Hand gesture, fall, glass break (NPU) | The board names a gesture, a fall, breaking glass | G5187 | NPU-bound; CPU twins exist | sensor BoosterPack for gesture |
| **Arc fault AI** | A current waveform called arc or normal | F2800137, F28P65X, AM13E230, MSPM33C321A; G5187 twin (NPU) | **same demo in four families: the cross-family test** | none (stored test data) |
| **Motor fault AI** | Vibration data sorted into six bearing faults | C2000, AM13E230, MSPM33C321A; MSPM0 twins on three boards | same, four families | none (stored test data) |
| PIR presence AI | Human, dog or background motion from a real PIR | G3507, G3519, G5187, MSPM33C321A | same-SDK across MSPM0; twin on MSPM33 | PIR sensor |
| Sensorless FOC motor | A brushless motor spins with no hall sensors | G3507, G3519 | same-SDK between them; does not fit L1306 or C1104 | DRV83xx EVM and motor |
| Post-quantum key exchange (ML-KEM) | Both sides agree on a key, checked against NIST answers | G3519, MSPM33C321A | same crypto folder in both SDKs | none |
| LVGL touch GUI | TI's widget and benchmark demo on a touch screen | MSPM33C321A | rewrite for G3519 and G5187; L1306 too small | a Waveshare 3.5 in display |
| USB headset and USB drive | The board appears to the Mac as a sound card or a disk | G5187 | USB-device bound | none |
| Zigbee light plus phone at once (DMM) | Home Assistant and a phone both control one light | CC1354P10 (470 kB flash, 44.6 kB RAM, measured) | the F3 SDK ships BLE plus 15.4 only; does not fit the CC2340R5 (2.1 kB short) | Zigbee hub (ZBT-2) |
| Firmware over the air from a Mac | New firmware over Bluetooth, 43 s measured | CC2340R5, CC1354P10, CC2755P10 | same API across the three | none |
| Wi-Fi MQTT sensor and BLE provisioning | Phone gives Wi-Fi details over Bluetooth; the board publishes to a broker | CC35X1 | Wi-Fi-bound (only Wi-Fi board) | phone, MQTT broker |

Also high on the lists: Bluetooth Channel Sounding distance on the CC2755P10 (needs a second CC27xx board), the sleepy Zigbee sensor (joined Home Assistant in 6 s), sub-1 GHz long-range networks on the CC1354P10 (TI 15.4, Wi-SUN, a wireless serial cable), CCStudio's live-plot GUI on the C2000, the F28P65X's second control CPU (CLA) and EtherCAT servo drive.

## What runs together (the chained demos)

| # | Chain | What TI ships | Fits? | Status |
|---|---|---|---|---|
| 1 | **Phone sets up Wi-Fi over Bluetooth, the board joins, publishes MQTT, takes an OTA update from an MQTT command** | Four separate demos; the Matter app combines Bluetooth commissioning, Wi-Fi and OTA but no MQTT and builds on Linux only | Estimate yes: about 1.32 MB of a 2.63 MB image slot, about 14 kB RAM spare | In `cc35x1/chained-demo/`; compile-only until run on a board |
| 2 | Zigbee light and phone control at once | DMM on the CC1354P10 (Zigbee plus BLE) and CC2755P10 (15.4 plus BLE) | Runs on the CC1354P10: Zigbee join plus BLE light control at once, 2026-10-08 (`cc1354p10/dmm-zigbee-ble/README.md`) | Toggle it from Home Assistant; the CC2755P10 version is not built |
| 3 | Motor control loop plus fault AI on one chip | Nothing combined | motor_fault fits beside the 301.5 kHz FOC loop on the F2800137 (42 kB flash, 22.9 kB RAM, about 7 ms per window); arc_fault does not (under 2 kB RAM left). On the F28P65X: loop on CPU1 or the CLA, model on CPU2 | Not built beyond the F2800137 numbers |
| 4 | AI sensor node plus a radio | Nothing combined; MSPM0, MSPM33 and AM13 have no radio | Two chips: the model on a G5187 or MSPM33, the result over UART to a CC2340R5 (BTHome) or CC35X1 (MQTT) | Not built |
| 5 | Zigbee sensor plus Bluetooth firmware update in one image | Nothing combined | Room in the CC1354P10's 1 MB flash (estimate) | Idea |

## Portability checks, in order

1. **The four-family AI test:** build and run arc fault and motor fault from each SDK's own folder on the F2800137 (done, 11 of 11 AI examples pass), AM13E230, MSPM33C321A and G5187 (NPU twin), same test vectors, cycles and time on each. One table, four families, TI's own code.
2. **BLE OTA and the Zigbee light across the three wireless parts:** CC1354P10, CC2340R5 (both done), CC2755P10 not run.
3. **PIR AI and ML-KEM across MSPM0 and MSPM33:** G3519 and MSPM33C321A builds, then runs.
4. **Port one demo across a family line the hard way:** the LVGL touch demo from the MSPM33 to the G3519 (the same display the G3507 drives).

Each confirmed port becomes a row in `portability/matrix.csv`, a result row, and a line on the project page.
