<!-- experiment
id: E190
board: IWRL6432BOOST
date: 2026-10-13
status: waiting
reached: compile
line: Room zones need only a configuration change to TI's shipped Motion and Presence demo: up to 12 boxes, each reported in every frame. The four-zone configuration, zone logger, live page and Home Assistant poster are written and tested without the radar. Nothing ran on it. The board has been silent on USB since 2026-10-12, and three probe resets and a USB power cycle did not wake it. A replay of the frames recorded on 2026-10-10 with the demo's four shipped boxes shows one person lighting two boxes. Seated at 4 m on 2 by 3, near and far were both lit for 70 percent of the seated time.
stop: unknown
-->
# Can TI's Motion and Presence demo on the IWRL6432BOOST board's IWRL6432 radar report room zones with no reflash, and can each zone become a Home Assistant sensor?

**Short answer.** Zones come from a configuration change alone. The shipped demo's command line takes up to 12 `mpdBoundaryBox` lines and puts one 2-bit state per box in every frame (type 315). Built here: a four-zone configuration, and `zones.py`, which reads frames, logs each zone's on and off, counts zone changes and writes a live page once a second. The Home Assistant posting code is switched off. The radar did not answer, so nothing was measured on it. What was measured is a replay of 2026-10-10 recordings made with the demo's own four boxes: a still person lit more than one box most of the time. The live measurements (20 minutes empty, a timed walk-through) are not taken.

How to run it and what each part does: [iwrl6432boost/zones/README.md](../../iwrl6432boost/zones/README.md). Evaluation background: [the antenna evaluation](2026-10-10-radar-antenna-eval.md), [how to evaluate a presence radar](../guides/evaluate-a-presence-radar.md).

Terms: [vocab](../vocab.md). A zone is a floor-plan rectangle (x sideways, y out from the radar); the 2D demo reads only x and y of a cluster's center. Board: [IWRL6432BOOST](../../iwrl6432boost/README.md). Files: `iwrl6432boost/zones/`. Read and run 2026-10-13 on macOS 15.7, MMWAVE-L-SDK 05.05.04.02 source, Radar Toolbox 4.00.00.05.

**Question.** A room radar for a home needs to say which area is occupied (the desk, the door), beyond whether anyone is there. The shipped demo already has boxes. How far do they get with no firmware change, and is the result good enough to post each area to Home Assistant?

**Vendor position.** The demo's CLI help (captured 2026-10-10) reads `mpdBoundaryBox: <zone_number> <xMin> <xMax> <yMin> <yMax> <zMin> <zMax>`. In the SDK source (`mmw_cli.c`): each line adds a box (`numBoundaryBoxes++`), the zone number is not used, and nothing clears the boxes until the next boot. `mpdproc.c` assigns a cluster to a box when its center's x and y are inside (z is not tested in the 2D build), keeps a state per box (`STATE_EMPTY` 0, `STATE_MINOR` 1, `STATE_MAJOR` 2) and allows 12 boxes. `motion_detect.c` sizes the type 315 payload as `ceil(boxes / 4) + 1` bytes. TI's user guide does not list the state values or the byte layout. Read 2026-10-13.

**Empty room, replay.** The 2x2 empty pass had 0 frames lit; the 2x3 empty pass had all four boxes lit for its first 14 s (the antenna evaluation counts that as the person leaving plus the 12-frame hold, not a false alarm) and none after.

**Expectation, written before any run.** Four boxes accepted, all lines Done; 4.0 frames a second; empty room for 20 minutes: 0 zone events (the shipped four boxes gave 0 presence frames in 87,602 empty-room frames); a person walking in lights the door end first and the desk last, with a neighbouring box sometimes lit; the desk stays lit while the person sits; every box clears about 3 s plus the walk out after the person leaves (the hold is 12 frames).

**What was tried.**

1. Chose depth bands, not left and right. The recorded seated passes on the center line lit the left box 87 to 92 percent and the right box 0 to 38 percent, so an x split near x = 0 is unreliable. Edges: desk 0.25 to 3.5 m, middle 3.5 to 5.5, far room 5.5 to 6.5, door end 6.5 to 7.5, each from x = -3.5 to 3.5. The 3.5 m edge sits between the range profile's reading of the 2 m chair (2.78 m) and the 4 m chair (4.6 m). Layout from the antenna evaluation's setup, not re-surveyed.
2. Wrote `zones.json`, and `zones.py cfg` generates `cfg/PresenceDetect-zones.cfg`: the 12-frame-hold configuration with the four box lines replaced, `lowPowerCfg 0`, 2 by 3 antennas.
3. Wrote `zones.py` (`apply`, `run`, `walk`, `check`, `replay`, `selftest`) and `draw_map.py`. The self test checks the generated lines, the decoder on a hand-made 315 payload (zones 2, 0, 0, 1), the tracker's episodes and hold arithmetic, the live page's colors and the Home Assistant request shape against a local mock server. It passes.
4. Rendered the live page from a made-up two-zone state in headless Chrome to check the layout (`iwrl6432boost/zones/img/live-page-layout-check.png`). A picture of the layout, not a measurement.
5. Replayed the 2x3 and 2x2 passes from the antenna evaluation's recordings through the same tracker (`zones.py replay`): per box, when it lit and cleared against the cue, and, for the seated passes, how often a box on the wrong side of the 3.5 m line lit.
6. Tried to start the radar. `lsof` showed nothing holding its port and no feeder or logger was running. The port `/dev/cu.usbmodemRI32RI321` ended every read at once (`device reports readiness to read but returned no data`). `xds110reset -s RI32RI32` and `xds110reset -a toggle -d 100 -s RI32RI32` changed nothing. A 5 s power cycle of the probe's hub port (`uhubctl -l 20-1 -p 3 -a cycle -d 5`, only that port) brought the probe back, and macOS then named the application port `/dev/cu.usbmodem3` (the old name stayed as a dead node). On it `version`, `help` and plain newlines got no answer at 115,200 baud, and nothing came at 1,250,000, 921,600 or 460,800 baud, with the line read for 3 to 16 s each and a reset toggle in the middle of one read.

**How far it got.** Compile: the configuration and the program build and pass their own checks. Run on the radar: not reached. The board has answered in neither run nor flash mode since a rewiring on 2026-10-12, through a reset, a replug and a power cycle with the barrel jack.

**Replay results (2026-10-10 recordings, the demo's four shipped boxes: left and right of x = 0, near 0 to 3.5 m, far 3.51 to 7 m; one pass per cell).** Seated window = the 45 s between the enter and leave cues. The four rows below are the full table.

| Pass | Either near box lit | Either far box lit | Near and far both lit | Any box lit | Lit on the wrong side of the 3.5 m line (against the range profile peak) |
|---|---|---|---|---|---|
| 2x3 seated 2 m | 87% | 32% | 32% | 87% | 37% of lit frames |
| 2x3 seated 4 m | 73% | 88% | 70% | 91% | 76% |
| 2x2 seated 2 m | 88% | 20% | 20% | 88% | 23% |
| 2x2 seated 4 m | 92% | 92% | 92% | 92% | 100% |

Reading: at 4 m the near box was lit most of the time with the person's profile peak at 4.6 m. In the 2x3 walk-in the near and far left boxes lit together at 6.7 s, the moment the person appeared at about 6.5 m by the evaluation's cue timing; a box covering 0 to 3.5 m lit for a person 6.5 m away, which is the misassignment this page is about. One box per person did not hold. **Unknown (suspected: Chip limit).** The body's clusters may straddle the line (the profile read the chairs 0.78 m and 0.6 m long), multipath may add points, and the 2 by 3 angle estimate is coarse. Not tested: the cluster radius, the minimum points per cluster, a higher mount.

**Serial load and frame rate (recorded 2026-10-10, not re-measured).** 4.003 frames a second, 704 bytes a frame on the 2 by 3 setting: 2,818 B/s, 24.5 percent of the 11,520 B/s a 115,200 baud link carries (computed).

**What the shipped demo cannot do.**

- Count people or track them. A zone is occupied or not. TI's People Tracking material for the same Motion and Presence Detection demo (the file `IWRL6432BOOST_Tracking_3Persons_7m_wall_mount.cfg` is named in the antenna evaluation and is not in the toolbox copy on this Mac, so unverified) gives a track per person. The prebuilt 2D and 3D images are in the toolbox. Loading one needs flash mode (S1 position 1 OFF, S2, flash, back, S2), done by hand at the board. The shipped demo's help text lists `trackingCfg`, `boundaryBox`, `gatingParam`, `stateParam`, `allocationParam` and `profileSwitchCfg`, so the shipped image may run the tracker if sent those lines.
- Height. The 2D build ignores z. The 3D image uses it for sit against stand.
- Clean attribution: see the replay.

For scale, public sources only: Aqara's FP2 page says up to 30 zones, up to 5 people at once, wall mounting needed for zones. Hi-Link's HLK-LD2450 manual: 24 GHz, up to 3 targets with x and y in millimeters at 10 frames a second, 6 m, one region filter. Neither was tested here.

**The line.** The radar does not answer on USB in any state reachable without changing switches, cause unknown. Everything that depends on it is unmeasured: empty-room false zone events in 20 minutes, per-zone detect and clear times, the page against a live person, the frame rate with the four boxes (the recording also had four boxes, so no change is expected).

- Derive zones in software from the minor-motion range profile (type 303) instead of the demo's boxes. The profile peak is unreliable with nobody in the room (it sits on a fixed reflector near 6.5 m), so it would need a background subtraction first.
- Send TI's tracking lines to the shipped image and count tracks.
- A cluster radius and minimum points sweep to see whether the two-boxes-lit result moves.
- Debounce in the host: ignore a zone that lights for fewer than N frames.
- Home Assistant over MQTT discovery once a token or broker exists.

**Ask.** If you have used `mpdBoundaryBox` zones on an IWRL6432 and a person on a zone line lit one box only, which cluster and state settings did you use?

## Findings log

- No feeder or radar process was running and no port lock was held when the port was tried.
- The port gave a read error at once. Three probe resets and a 5 s power cycle of the probe's USB port did not wake the radar (the 2026-10-12 record says a power cycle of USB and the barrel jack together also failed). After the cycle macOS listed the application port as `/dev/cu.usbmodem3`; the old node stays as a dead entry. If you see a dead port name after a probe power cycle, look for the new name. No answer came at any baud rate in either mode.
- The replay and the page checks were built without the radar.
