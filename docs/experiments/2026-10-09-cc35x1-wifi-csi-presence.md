<!-- experiment
id: E144
board: LP-EM-CC35X1
date: 2026-10-09
status: line found
reached: run
line: In a lived-in house the shipped model said "presence" in 98% of scored windows across 21 minutes. In the one phase with a known label, the room empty with the door closed, it said "presence" in 135 of 136 windows. It runs, streams and times as TI says; the shipped weights do not transfer to this room without training on this repo's own data.
stop: none
-->
# Does TI's Wi-Fi CSI presence model on the LP-EM-CC35X1 detect a person in a room through the house Wi-Fi, and how well?

**Short answer.** It runs, and in this room it does not detect anyone. Over a 21-minute run on 2026-10-09 the board called "presence" in 533 of the 543 scored windows (98%; 571 of all 589 saved), whether the room was empty or not. In the one phase with a known label, the room empty with the door closed, 1 of 136 scored windows was right (mean probability of presence 0.92). The other three phases are not scored: the second cue (a spoken instruction) did not carry through the closed door and the cues after it were not followed, so who was in the room is unknown. The board's output looked the same in all four (mean probability 0.80 to 0.87). Fifteen seconds before the run, with a person sitting at the desk (no scored label), the same board read "empty" seven times in a row (probability 0.13 to 0.44). So the shipped model separates nothing here.

What works as TI describes: the image builds and signs from a Mac, the Wi-Fi login is typed to the board at run time, the first result arrives 7.4 s after the host asks for the stream, and one result comes every 2.0 s. The most likely reason for the failure (suspected, not tested) is the one TI's white paper gives: placement and the room change what the radio sees, and this model was trained in TI's rooms. A room-specific model needs TI's data-capture image and training.

Contents: Question, Vendor position, Expectation, What was tried, Test protocol, Test steps, How far it got (the results table), The line, Ideas, Ask. Related FAQ: [CC35X1 FAQ](../../cc35x1/FAQ.md).

Terms: CSI is channel state information (defined in the Question). The access point is the Wi-Fi router. DAP is the plugin's data-exchange protocol between board and host (a short binary frame). A window is one 2 s slice of CSI that gets one yes/no answer. NPU-CDE is the chip's neural-network accelerator. More terms: [vocab](../vocab.md). 1 kB = 1,024 bytes. Related: [Wi-Fi provisioning over Bluetooth](2026-10-09-cc35x1-ble-provisioning.md) (how the board got its Wi-Fi login without compiling it in).

**Question.** Wi-Fi channel state information (CSI) is how the radio sees the room: each packet from the access point arrives with a measured gain and phase for every subcarrier, and a person moving through the path changes them. TI shipped a pre-trained model for this on 2026-10-07. In a room that is not TI's, with a home access point, how often does it say "presence" when someone is there, and "empty" when nobody is?

**Vendor position.** Quoted from TI, all read 2026-10-09.

- TI.com use-case page "Wi-Fi sensing for presence detection": "occupancy sensing capabilities at no extra BOM cost, eliminating the need for extra components such as a PIR" and "works with existing Wi-Fi infrastructure". Its dataset table lists estimated accuracy for training, validation and testing: `wifi_presence_detection_dsi` (800 samples, the default) 99.16%, 99.16%, 98.75%; `dsd_2000` 97.67%, 97.67%, 99.00%; `dsd_4400` 97.27%, 97.35%, 99.55%. Model test accuracy 99.03% "based on TI's sample dataset"; latency 24.57 ms on the M33 core, 13.88 ms with the NPU-CDE; flash about 10.57 kB and 23.26 kB; SRAM about 4.11 kB and 5.38 kB.
- Example README (`edgeai_wifi_csi_occupancy_detection_live_preview/README.html`, plugin 1.20.00.00): needs an LP-EM-CC35X1 on an LP-XDS110, an access point's name, password, security type and MAC address, and Edge AI Studio. Each 2 s window of CSI is turned into a 64 by 26 input (FFT over time and over subcarriers), the model runs on the board, a 3-window soft average with a 0.5 threshold gives the decision. "WiFiPresDet_4.7k_CDE ... the only one validated end to end on the LaunchPad." Samples per window: 256, "no other Samples value is supported". CSI runs at 128 Hz, which needs `csi_config -s 1 -p 7`.
- White paper STDA051 "Sensing Without Sensors: Why Wi-Fi is the Smart Choice" (October 2026, 8 pages): Wi-Fi sensing "does not require a direct line-of-sight", trades "localization accuracy for the ability to work through obstacles", "consum[es] significantly more power than ultra-low-power sensing alternatives such as passive infrared", is "generally not optimized for battery-powered designs", and placement of sender and receiver "directly impacts sensing accuracy".
- TI gives no figure for a still person, for an unseen room, or for false alarms per hour.

**Expectation (this repo's, a guess).** The 99% figures are on TI's own recordings, in TI's rooms. An unseen room is expected to cost accuracy. Guess: empty room 90% or better of windows right; a person walking 90% or better; a person sitting still 60% to 90%, the weakest case, because the model was trained on motion. A result under 70% overall would mean the shipped model needs this repo's own data (the data-capture example and Edge AI Studio exist for that). Basis for the guess: the white paper's warning that placement changes accuracy, and the use-case page's description of its larger datasets as covering "a variety of human motions".

**What was tried.** Prepared without the board.

- Pages read, with versions, sizes and dates, in the table below.
- Downloaded the plugin (63 MB zip, SHA-256 `a4ba1aea...0856`, no sign-in needed) and the default dataset (45 MB zip, 803 CSV files of 256 windows by 52 subcarriers). The larger datasets (112 MB and 246 MB zips) were fetched, then deleted for space.
- Built both examples with TI Arm Clang 5.1.1 (TI tested 4.0.4.LTS) and SysConfig 1.28.0: `cc35x1/csi/build.sh`. TI's zip ships old generated files; they must be deleted and the `.syscfg` touched or the Toolbox signing step stops on a missing `action_request_extra.txt`. Live preview: 1,490 kB code and constants, 573 kB zero-initialised data; data capture: 1,375 kB and 487 kB. Both signed with the SDK example key (the board is activated with it).
- TI's example does not compile Wi-Fi credentials in. It takes them as console commands over the serial port at 921600 baud: `wlan_start`, `wlan_sta_role_up`, `wlan_connect -s "<name>" -t WPA2 -p "<password>"`, then `csi_enable`, `csi_config -s 1 -p 7`, `csi_set_mac -a 1 -m <access point MAC>`, `csi_get_results`. TI's helper `serial_automation.py` reads those from a JSON file with the password in plain text. Use `cc35x1/csi/csi_run.py` instead: it reads the name, password and MAC from environment variables or a hidden prompt, scrubs them from its saved log, and writes no file that contains them. Bluetooth provisioning (linked in the terms line) is the fallback and was not needed.
- Reading the result without Edge AI Studio: Studio 1.8.0 has desktop installers for Windows and Linux only. On a Mac only the cloud version (browser) exists, and the release notes do not say whether it can open a local serial port. The board reports each result as a DAP frame (class, probability of presence) once a host sends "start streaming". `csi_run.py` sends that frame and decodes the replies. The frame layout comes from the plugin's DAP source (`source/ti/ai/edge_ai/dap`); the parser passes a synthetic self-test (`python3 -I cc35x1/csi/csi_run.py --selftest`). The live exchange was untested at the time of writing, and the run below is its first test. It worked.

- Other presence routes in this repo, compared on facts only (none of them has been measured against another). This board is a Wi-Fi 6 chip on a LaunchPad plus a debugger. It uses the access point already in the house and needs no line of sight (TI). It gives one yes or no per 2 s window and no position. It needs continuous packets, so TI says it draws far more than a PIR and is not made for batteries. The IWRL6432 radar: its own 60 GHz transmitter, "typically up to 25 m" (data sheet), range and motion data rather than a single flag, about 1.2 mW at one update a second in the data sheet's power-optimized mode. It needs a clear view in front of the board and 20 cm between the board and a person. Board stock was "Out of stock" on ti.com when read. PIR: senses motion of a warm body, lowest power of the four (white paper). TI's PIR model runs on this same chip in 29.45 ms with 176 kB flash and 57 kB SRAM (TI.com PIR use case). Time-of-flight (VL53L1X): one narrow beam, about 3 cm to 4 m, sees only what crosses that beam. The same-room, same-hour comparison is in the ideas list below.

- First live run (2026-10-09): the login was typed into a shell at run time. Four script fixes were needed before the join went through, all in this repo's script or TI's console, none in the model. The board joined on the first full attempt and printed its access point MAC in the join message, which the script now reads instead of asking. The console then flooded with `ERROR rxData_DataPacketDisptcher(): Received VLAN packet - drop it` (33 lines in 60 s; the house network carries VLAN-tagged frames, a harmless message, see the FAQ) and printed `[CSI] DAP inferencing ready`, not the `DAP streaming ready` the README promises. The stream started anyway once the host sent the start frame. The script sent the start-streaming frame; the board acknowledged it and sent one inference every 2.0 s. The protocol then ran its four phases. The second cue was not audible from outside the closed door and the cues after the first were not followed, so only the first phase (empty, door closed) has a trusted label. The still and walking cases are not measured.

Versions and sources (read 2026-10-09):

| Item | Version, date | Size | URL |
|---|---|---|---|
| SimpleLink SDK EdgeAI Plugin | 1.20.00.00, released 2026-10-07; manifest dated 2026-09-24 | zip 64,612 kB (downloaded: 63.1 MB); Windows installer 29,427 kB; Linux 29,773 kB; no macOS installer, the zip is OS-independent | ti.com/tool/download/SIMPLELINK-SDK-EDGEAI-PLUGIN/1.20.00.00; release notes under software-dl.ti.com/simplelink/esd/simplelink_sdk_edgeai_plugin/1.20.00.00/exports/ |
| CCStudio Edge AI Studio (MCU) | 1.8.0 (build 1.8.0.11), 2026-10-09 | Windows 613,341 kB; Linux 753,932 kB (over 500 MB, not downloaded) | ti.com/tool/download/EDGE-AI-STUDIO-MCU/1.8.0; cloud at dev.ti.com/edgeaistudio |
| macOS build of Edge AI Studio | none; "available for 64-bit Windows and 64-bit Linux" (desktop), cloud in any browser | | release notes v1.8.0 |
| Wi-Fi SDK | 10.20.00.39 (2026-09-03), installed | | |
| TI example datasets | `wifi_presence_detection_dsi` 44.8 MB, `dsd_2000` 111.7 MB, `dsd_4400` 246.3 MB | | software-dl.ti.com/C2000/esd/mcu_ai/datasets/ |
| White paper | STDA051, October 2026, 8 pages | 536 kB | ti.com/lit/pdf/STDA051 |
| Use-case page | "Wi-Fi sensing for presence detection" | | ti.com/technologies/edge-ai/use-cases/wi-fi-sensing-for-presence-detection.html |

**Test protocol.**

- Setup: LP-EM-CC35X1 on the LP-XDS110ET (TGT VDD on EXT, probe <probe serial>), board USB-C and probe USB-C both into the Mac. Board on the desk in the test room, antenna end clear of the Mac and metal. The Mac is on the same Wi-Fi network as the board (TI: the keepalive needs the access point, board and host on one network). Record before starting: access point model and room, band and channel the board joins, straight-line distance from board to access point, walls between them, board height, where the chair is.
- Program the live preview image with the Toolbox (`programmer -i XDS110 -param1 E10000FQ programming --tool_settings <path to toolbox/tool_settings.json>`, absolute paths), reset, then run `csi_run.py`. It joins, enables CSI, starts the stream and walks through four phases of 5 minutes: empty, one person still, one person moving, empty again. About 21 minutes with the join.
- The first 20 s after each cue is not scored (the person is moving into place and the 3-window average needs 6 s to settle). That leaves about 140 scored windows per phase.
- Score each scored window: right if the board's class equals the phase label (empty 0, person 1). Report per phase: windows scored, correct, accuracy, mean probability. Report overall accuracy, the false-alarm rate (empty windows called presence) and the miss rate (person windows called empty). Also the join time and the time to the first result.
- Repeat once on another day or with the board moved, because one room and one hour is one sample. If the shipped model scores under 70%, run the data-capture image, record labelled data with Edge AI Studio cloud (or this repo's own CSV in TI's file format: `time` plus 52 subcarrier columns, 256 rows), and train.
- Visualisation: TI's CSI view (time and frequency domain) is in Edge AI Studio, which has no Mac desktop build. If the cloud version cannot reach the serial port, plot the CSI from the data-capture stream and from TI's `dsi` dataset (same 52-column format). Not built.

**Test steps** (the plan as run; about 25 minutes).

1. Plug the LP-XDS110ET into the Mac (USB-C), and the board's own USB-C.
2. In a terminal, type the line below with the real values (the script scrubs them from its log and saves them nowhere):
   `export FRL_WIFI_SSID='...'  FRL_WIFI_PSK='...'  FRL_AP_BSSID='aa:bb:cc:xx:xx:xx'`
   The access point's MAC is in its admin page, or Mac option-click on the Wi-Fi icon (BSSID).
3. Start the run. A voice cues each phase: "Leave the room now and close the door", "Come in, sit in the chair and stay still", "Stand up and walk around", "Leave the room again". Door closed means no one else in the room; anyone else in the house stays out of it.
4. Phone face down while sitting. Do not touch the board or the Mac during a phase.
5. Note the access point model and where it is.

Needs: (a) the USB connections, (b) programming this image over the board's current one (the board then runs the CSI example until it is reprogrammed; TI's blink and the Network Terminal images are rebuilt in a minute), (c) `pyserial` in a venv for the run, and (d) the Wi-Fi login typed into a shell as above.

**How far it got.** Link, sign, flash (52 s), boot, join, CSI stream, model output, the full 21-minute protocol. Right means the board's class equals the phase label; the first 20 s of each phase is not scored. Only the first phase has a trustworthy label: that cue was heard and the room was left. The second cue did not carry through the closed door and the cues after it were not followed, so phases two to four are kept for the board's output and not scored.

| Phase (what actually happened) | Windows scored | Called "presence" | Right | Mean P(presence) |
|---|--:|--:|--:|--:|
| Empty, door closed | 136 | 135 | 1 (0.7%) | 0.919 |
| Occupancy unknown (cued "sit still"; not heard outside the door) | 135 | 131 | not scored | 0.795 |
| Occupancy unknown (cued "walk around") | 136 | 131 | not scored | 0.842 |
| Occupancy unknown (cued "leave again") | 136 | 136 | not scored | 0.869 |
| **All four phases** | **543** | **533 (98.2%)** | only the first phase is scored | 0.856 |

Time from the host's start-streaming frame to the first result: 7.4 s. Result period: 2.0 s. Join time: not measured in this run (the join happened in an earlier attempt that stopped before the stream; TI's Network Terminal joined the same network in 5.74 s on 2026-10-08). Conditions: the test room, the house access point (model not recorded), band and channel not recorded, other people in the house not recorded. The Mac that ran the script stayed in the room.

**The line.** Most likely the model's weights in this room, not the pipeline (suspected: another spot and the lab's own data were not run). The pipeline produced steady, timed output; the output just did not follow the room. With the room empty and the door closed (the one phase with a known label) the board said "presence" 135 of 136 times, and 533 of 543 across the whole run. A detector that is always on is not a detector. The guess of 90% for an empty room was wrong by the whole range. Two things could still rescue the shipped weights and were not run: a different spot for the board (TI's white paper says placement "directly impacts sensing accuracy"), and a quieter network (the VLAN flood is dropped packets, but it shows traffic the model never saw). A room-specific model needs TI's data-capture image: record this room empty and occupied, train in Edge AI Studio, and run the same protocol again.

**Ask.** The DAP start-streaming frame `ED 08 00 9E` works (the board acknowledges with the same command byte and streams). Not established: whether Edge AI Studio's cloud version opens a local serial port on a Mac, and the board placement, distance to the access point and traffic when the shipped `wifi_presence_detection_dsi` set was recorded (TI's pages read do not give them).

## Side by side with the radar, 2026-10-10

**Overnight baseline (2026-10-10 to 2026-10-11).** The test room was empty and the rest of the house occupied but quiet (the house itself was not empty). The comparison above is one person in one evening; the overnight run is a free run in the empty room. The board's results and the radar's frames were logged on the same Mac clock. From 23:49 to 01:00:21 (the window ends where the logger's restart loop stopped; cause not found in its log), the Wi-Fi model classed **0 of 2,065 results as present** (median probability 0.013) over 1.19 h; the radar at a 12-frame hold gave 0 flags over the same window and on to 05:54 (6.08 h). So the model's evening behavior (present in 135 of 136 empty-room windows) was not noise: it followed activity elsewhere in the house, and in a quiet house it reads empty. It still cannot tell this room from the next one. Rows: `cc35x1 ai_presence_accuracy wifi-csi-empty-overnight`, `iwrl6432boost radar_false_alarm presence-2x3-hold12-overnight`.

**The line.** In the same room, on the same clock, the Wi-Fi model said "present" in every window of every pass, including the empty-room passes. It cannot be wrong about a person who is there and it is always wrong about an empty room. The IWRL6432BOOST radar followed the walks. Putting the two together with a rule can only help by lowering the Wi-Fi false alarms, and the radar has to do all of that work.

**What ran.** The board ran the live-preview image (programmed 2026-10-10 in 52 s with the Toolbox), logged by `csi_run.py --freerun` (login and start-streaming as before, then one host-timestamped row per result, about 2.1k rows from 20:35 to 21:51, all class "presence"). The radar test ran its in-and-out passes (empty, walk, seated at 2 m, seated at 4 m) in two rounds, 2 transmit by 2 receive and 2 by 3. `cc35x1/csi/compare_presence.py` joins the two on the Mac's clock. The label of each moment comes from the radar tool's own cue times: empty for the empty pass and before the enter cue and after the leave cue, person present between them. The 6 s after each cue are not scored (the board averages 3 windows). Only moments where both logs have a fresh value are scored. One sitting, one room, one access point, one person; not a general result.

| Round | Rule | Misses (person there, said empty) | False alarms (room empty, said present) |
|---|---|--:|--:|
| 2x2 | Radar alone | 0% (0 of 177) | 24% (81 of 337) |
| 2x2 | Wi-Fi alone | 0% (0 of 177) | 100% (337 of 337) |
| 2x2 | OR (either says present) | 0% (0 of 177) | 100% (337 of 337) |
| 2x2 | AND (both say present) | 0% (0 of 177) | 24% (81 of 337) |
| 2x2 | Radar confirms Wi-Fi (Wi-Fi flag and radar present within 5 s) | 0% (0 of 177) | 34% (113 of 337) |
| 2x3 | Radar alone | 2% (3 of 179) | 22% (74 of 338) |
| 2x3 | Wi-Fi alone | 0% (0 of 179) | 100% (338 of 338) |
| 2x3 | OR | 0% (0 of 179) | 100% (338 of 338) |
| 2x3 | AND | 2% (3 of 179) | 22% (74 of 338) |
| 2x3 | Radar confirms Wi-Fi | 0% (0 of 179) | 33% (112 of 338) |

Per pass, the empty-room pass alone (door closed, nobody in the room): Wi-Fi present in 100% of its windows in both rounds (28 and 29 windows); radar present in 0% of its frames in the 2x2 round and 24% in the 2x3 round (240 frames each; the 24% is not explained, nobody was seen entering). The radar's other "false alarms" are the minutes after the person left: the radar took 10.6 to 14.6 s to clear after the leave cue (Wi-Fi never cleared), and the label calls those moments empty after the 6 s settle band. Detect after the enter cue: radar 0 to 6.7 s, Wi-Fi 0.2 to 1.2 s, which means nothing because Wi-Fi was already "present". The full per-pass tables are in `data/results/cc35x1.csv` (variants `wifi-presence-*`) and come from `python3 cc35x1/csi/compare_presence.py --config 2x2-both` and `--config 2x3`.

**What each rule does with a sensor that is always on.**

- OR: identical to Wi-Fi alone, so it never helps. It would only add something if Wi-Fi ever missed a person, and it never did.
- AND: identical to the radar alone, because Wi-Fi is always true. Fusion here equals the radar, with the radar's own clear delay and its misses (3 moments in the 2x3 round).
- Radar confirms Wi-Fi: no misses in either round, but about a third false alarms, because the radar's 5 s agreement window keeps a flag alive after the radar's own clear. It trades the radar's 2% misses for about 10 more points of false alarms than the radar alone.

**Where fusion helps and where it cannot.** It helps in the case you would hope for: an empty room where Wi-Fi false-alarms and the radar is quiet. Here AND and radar-confirms turn Wi-Fi's 100% false alarms into the radar's 0% (2x2) or 24% (2x3) on the empty pass. It could also help a seated still person if the radar dropped while Wi-Fi held; that was not seen, because the radar held its seated person (misses 0 to 1% on the seated passes) and Wi-Fi never dropped. It cannot help when both sensors see the same person the same way, and it cannot rescue a model that says "present" regardless of the room: a rule that needs Wi-Fi to agree adds no information when Wi-Fi's answer never changes. The finding is about this model in this room (consistent with the 2026-10-09 run, 98% present); TI's training rooms and a retrained model are untested.

### Who was first, and where they agreed

**The line.** The radar was first by default and the only one that ever said "empty". The Wi-Fi class stayed at "present" in every row after the first minutes (none of about 1.1k rows after 21:15 were class 0), so it agreed with the radar whenever the person was in the room and disagreed in every empty moment and during every clear. Its probability p1 did move (62 rows below 0.9, down to 0.44), but no cut-off on it separates empty from occupied.

**Agreement per pass.** Moments are on the 0.5 s grid where both logs have a fresh value; "both off" is 0% everywhere because the Wi-Fi never said empty. Detect and clear times are in the table above: after the enter cue the radar flagged in 0 to 6.7 s and cleared 10.6 to 14.6 s after the leave cue; the Wi-Fi was already "present" before the cue and never cleared.

**2 transmit, 2 receive round, agreement.**

| pass | moments | both on | Wi-Fi on, radar off | radar on, Wi-Fi off | both off |
|---|---|---|---|---|---|
| empty 1 | 120 | 0% | 100% | 0% | 0% |
| walk 1 | 157 | 82% | 18% | 0% | 0% |
| sit2 1 | 191 | 53% | 47% | 0% | 0% |
| sit4 1 | 190 | 68% | 32% | 0% | 0% |

**2 transmit, 2 receive round, threshold sweep** (Wi-Fi counts as present when p1 is at least the threshold; empty = the empty-room pass; seated = the two seated passes between the cues, 6 s settle excluded; the last column scores the Wi-Fi against the radar's per-frame presence):

| threshold | Wi-Fi present in empty pass (false alarms) | Wi-Fi absent in seated windows (misses) | agrees with radar, all moments |
|---|---|---|---|
| 0.5 | 100% (120 of 120) | 0% (0 of 134) | 55% (359 of 658) |
| 0.6 | 100% (120 of 120) | 0% (0 of 134) | 55% (359 of 658) |
| 0.7 | 100% (120 of 120) | 0% (0 of 134) | 55% (359 of 658) |
| 0.8 | 100% (120 of 120) | 0% (0 of 134) | 55% (359 of 658) |
| 0.9 | 100% (120 of 120) | 0% (0 of 134) | 54% (358 of 658) |
| 0.95 | 97% (116 of 120) | 0% (0 of 134) | 55% (362 of 658) |
| 0.97 | 93% (112 of 120) | 6% (8 of 134) | 51% (335 of 658) |
| 0.99 | 48% (58 of 120) | 47% (63 of 134) | 43% (286 of 658) |
| shipped class | 100% (120 of 120) | 0% (0 of 134) | 55% (359 of 658) |

**2 transmit, 3 receive round, agreement.**

| pass | moments | both on | Wi-Fi on, radar off | radar on, Wi-Fi off | both off |
|---|---|---|---|---|---|
| empty 1 | 120 | 24% | 76% | 0% | 0% |
| walk 1 | 159 | 53% | 47% | 0% | 0% |
| sit2 1 | 191 | 53% | 47% | 0% | 0% |
| sit4 1 | 191 | 58% | 42% | 0% | 0% |

**2 transmit, 3 receive round, threshold sweep** (Wi-Fi counts as present when p1 is at least the threshold; empty = the empty-room pass; seated = the two seated passes between the cues, 6 s settle excluded; the last column scores the Wi-Fi against the radar's per-frame presence):

| threshold | Wi-Fi present in empty pass (false alarms) | Wi-Fi absent in seated windows (misses) | agrees with radar, all moments |
|---|---|---|---|
| 0.5 | 100% (120 of 120) | 0% (0 of 134) | 49% (324 of 661) |
| 0.6 | 100% (120 of 120) | 0% (0 of 134) | 49% (324 of 661) |
| 0.7 | 100% (120 of 120) | 3% (4 of 134) | 50% (328 of 661) |
| 0.8 | 100% (120 of 120) | 18% (24 of 134) | 50% (328 of 661) |
| 0.9 | 97% (116 of 120) | 33% (44 of 134) | 48% (314 of 661) |
| 0.95 | 97% (116 of 120) | 45% (60 of 134) | 50% (328 of 661) |
| 0.97 | 90% (108 of 120) | 51% (68 of 134) | 53% (350 of 661) |
| 0.99 | 68% (82 of 120) | 66% (88 of 134) | 57% (379 of 661) |
| shipped class | 100% (120 of 120) | 0% (0 of 134) | 49% (325 of 661) |

**Threshold result.** No threshold beats the shipped class. Up to 0.95 the Wi-Fi still calls the empty room present (97 to 100% of the empty pass) with few or no seated misses in the 2x2 round and growing misses in the 2x3 round; at 0.99 the false alarms fall (48% in the 2x2 round, 68% in the 2x3 round) only by missing the seated person about as often (47% and 66%). Agreement with the radar tops out at 57% (2x3 round, 0.99), which is no better than a coin that always says present. The model's probability carries a small amount of information (it dips a little more in the empty pass at 0.99), not enough to use.

**Timeline per pass.** One character per 2 s bin, 2 transmit by 2 receive round first. R is the radar (# present in at least half the bin's frames), W the Wi-Fi class, P the Wi-Fi probability (9 means 0.9 or more, 8 means 0.8 to 0.9 and so on), C the cues (E enter or go, L leave). A blank in W and P is a bin with no Wi-Fi result (the logger's result stream skips a window now and then). Read down a pass: the radar's # starts after E and ends 10 to 15 s after L; W is # throughout.

2 by 2 round:

```text
empty 1 (60 s)
  C                               
  R ..............................
  W #### #########################
  P 9999 9999999999999999999999999
walk 1 (80 s)
  C          E                L             
  R ################################........
  W ########################### ############
  P 999999899999999999999999999 999999999989
sit2 1 (96 s)
  C          E                      L               
  R .............#########################..........
  W ######################## #######################
  P 999999999999999999999999 99999999999999999999999
sit4 1 (96 s)
  C          E                      L               
  R ............##########################...######.
  W ###### ########################### #############
  P 999999 999999999999999999999999999 9999999999999
```

2 by 3 round:

```text
empty 1 (60 s)
  C                               
  R #######.......................
  W ############## ###############
  P 99999999999999 999999999998999
walk 1 (80 s)
  C          E                 L            
  R .............#####################......
  W ######### ########################## ###
  P 699999997 98799999899999999999999999 887
sit2 1 (96 s)
  C          E                      L               
  R .............##########################.........
  W # ########################## ###################
  P 7 86999999499999979979998989 9999999999999999996
sit4 1 (96 s)
  C          E                      L               
  R ............############################........
  W ### ########################### ################
  P 999 999999999999999997869877874 8889899999997796
```

**Limits.** One room, one person, one placement of the CC35X1 (on the bench next to the Mac), one access point, a model TI trained elsewhere, two passes of each kind, and a label taken from cue times that the person may have followed a second or two late. The radar is the reference for "agrees with radar" but it is not ground truth: in the 2 by 3 round it flagged 24% of the empty pass, all in its first 14 s (the person may still have been leaving; unexplained). A retrained model, another placement or a different room could change the Wi-Fi result; this run says nothing about those.

## Empty room on the guest network, 2026-10-26

False presence in an empty room is 0.5 %: 5 of 947 windows over 33 minutes (23:01 to 23:34), median P(presence) 0.050, 95th percentile 0.438. TI's shipped presence model on the live-preview image, `csi_run.py --freerun`, the board joined to the guest network, the lab door closed, the rest of the house occupied. The five hits sit in two 5-minute blocks (3 % and 1 %); nobody was seen entering, and the lab camera, which sees only the bench, showed it empty at the start. Row `ai_presence_accuracy / wifi-csi-empty-guest-33min` in `data/results/cc35x1.csv`.

This agrees with the overnight run of 2026-10-11 (0 of 2,065 windows) and not with the first phase of 2026-10-09 (135 of 136 called present). The 2026-10-09 phase is now the odd one out, and its label is the weak part: the person had just left and the house was active. Read together, the shipped model's empty-room false-alarm rate is low in this room; what is still unmeasured is detection of a still person and the rate with people moving elsewhere in the house. The per-room training question from the 2026-10-09 page stays open for those cases, not for the empty room.
