<!-- experiment
id: E162
board: LP-MSPM0G3519, IWRL6432BOOST, LP-EM-CC35X1
date: 2026-10-10
status: works
reached: verified
line: The screen half runs on live radar data: radar frame to panel in 111 ms median over 10 minutes with 2,310 frames and none dropped, on a two-page screen that also shows a Wi-Fi presence badge. The Mac sits in the radar path: it sends the radar's configuration and forwards frames. Not run: the one-wire fix that removes it, the radio leg, and the Home Assistant leg from the real radio.
stop: none
-->
# Can an MCU-only thermostat work: a LaunchPad with an LVGL touch screen, a radar sending presence, and a Wi-Fi radio feeding Home Assistant?

**Short answer.** The screen half works on the LP-MSPM0G3519 with live radar data: a radar frame reaches the panel 111 ms later (median, 2,310 frames in 10 minutes, none dropped), in 248 kB of flash and 28 kB of RAM. The path is radar, then a Mac script that sends the radar its configuration and forwards frames, then the G3519 over USB. The wired radar, the radio leg and the Home Assistant leg from the real radio have not run: the radio firmware needs the home network name put in at build time and replaces the Matter image on the CC35X1.

**Two-page screen and an overnight run.** The screen now has two pages. The thermostat page shows the held presence badge, a "Motion now" chip from the radar's point-cloud TLV 301, the temperature and the setpoint. A Sensors page shows the radar's held presence, motion, nearest distance, point count, SNR, zone, frames per second and the Wi-Fi model's class, probability and age, with 30 s history strips. The LVGL heap is 28 kB; 12 kB crashed with two screens. At 81 percent CPU on the Sensors page the screen's serial ring buffer began dropping bytes (ring drops rising), a limit to fix in the firmware. A feeder (`thermostat-system/tools/feeder.py`) forwards radar frames and the Wi-Fi logger's `W,` lines to the board. It has to run detached from the shell (`nohup ... & disown`): a feeder started as a background process of a command exits when that command's pipes close. A 12-frame hold (`PresenceDetect-hold12.cfg`, about 3 s) ran overnight with the test room empty, logging every frame to a per-day file; its false-alarm result is on the [radar antenna page](2026-10-10-radar-antenna-eval.md).

Terms: [vocab](../vocab.md). 1 kB = 1,024 bytes. Wire format: [PROTOCOL.md](../../thermostat-system/PROTOCOL.md).

**Question.** Can three TI boards, each doing one job, make a thermostat: radar presence in, a touch screen with a setpoint, Wi-Fi out to Home Assistant, with no Linux board?

**Vendor position.** TI's radar data sheet calls this "Topology 1": the radar does the signal processing and sends only the result to a host. This is from the IWRL6432 data sheet, read 2026-10-09. For the host link, TI's E2E answer on the IWRL6432BOOST says "You can directly access the UART rx and tx from J8.5 and J8.7 ... S1.4 must be in ON position for UART to be routed to the LP/BP connector". That is E2E thread 1497790, read 2026-10-10. TI publishes no thermostat reference with a radar, Matter or video; the newest is from 2020.

**What was tried.**

- Radar to G3519: the radar's shipped demo sends 384-byte frames at 115,200 baud, about 4 a second. The G3519 parses them with the same C parser as the AM13E230 page ([radar parser page](2026-10-09-am13e230-radar-host-parser.md); code in `am13e230/radar-parser/`). Presence is any set zone bit in TLV 315.
- Screen: LVGL 9.6 on the G3519, Waveshare 3.5 inch panel as wired for the earlier display tests. A thermostat screen: presence badge, temperature (a fixed 20.8 C, no sensor read), setpoint 15.0 to 30.0 C in 0.5 steps with plus and minus buttons, radar frame counter, live stats. Source `thermostat-system/g3519/`, build `thermostat-system/g3519/build.sh` (TI Arm Clang 5.1.1, -Oz, 80 MHz, 40 MHz SPI), flashed with DSLite on probe <probe serial>; TI's demo is restored at the end.
- Test source, because the radar was busy: `thermostat-system/tools/replay.py` sends frames of the 60 second shipped-demo capture (a UART capture of TI's shipped demo) into the G3519's USB serial port at 4 frames a second. Frame numbers are renumbered from 1 and the zone-bit byte of TLV 315 is rewritten to flip presence every 8 frames, because nobody stood in front of the radar when the capture was made. Everything else in each frame is the radar's.
- Radio: TI's `mqtt_client` with a UART0 line reader and the thermostat topics (`thermostat-system/cc35x1/`), built for the CC35X1. Broker and Home Assistant bridge: `thermostat-system/tools/hub.py` (a small MQTT broker that posts each update to Home Assistant by REST; Home Assistant has no MQTT integration).
- Wiring (4 wires, pictures `docs/arrivals/img/thermostat-*-wires.png`): radar J8 pin 5 to G3519 J1 pin 3 (PA9); radar J8 pin 2 (GND) to G3519 J3 pin 22 (GND); G3519 J1 pin 4 (PA8) to CC35X1 P1 pin 3 (P28); CC35X1 P1 pin 22 (GND) to radar J8 pin 4 (GND). Pins read from the radar schematic SWRR180 sheet 15 (DCA_LP_RS232_TX on J8 pin 5, RX on pin 7) and checked against the E2E answer above. Radar S1.4 ON, then a reset.

**How far it got.**

| Step | Result |
|---|---|
| Configure, compile, link | Yes. 242.4 kB of flash and 27.3 kB of RAM (bss; the 6 kB stack is extra) |
| Flash, boot, run | Yes, DSLite "Program verification successful", the board prints one STAT line a second |
| Replay, 240 frames | 239 of 240 SHOWN lines matched (frame 1 is the sync frame); no ring-buffer drops |
| Frame write to frame on the panel | median 89.7 ms, max 98.9 ms (33 ms of that is the 384 bytes on a 115,200 baud wire); on the board, last byte to end of redraw: median 51.3 ms, max 60.4 ms |
| Screen refreshes | 8 a second (7 to 10), 25 percent CPU, two refreshes per radar frame |
| Radar bytes in | 1.5 kB a second (4 frames of 384 B), 12.3 kbit/s of the 115.2 kbit/s wire |
| Checked by eye | Yes: the + and - taps change the setpoint |
| Broker to Home Assistant (a fake publisher standing in for the CC35X1) | Home Assistant took the three states in 33 to 106 ms per update, median 37 ms (10 updates). Entities `binary_sensor.fxl_therm_presence`, `sensor.fxl_therm_setpoint`, `sensor.fxl_therm_temp` exist now |
| CC35X1 firmware | builds (1.2 MB flash, 471.2 kB RAM, from `tiarmsize`); never flashed |
| Live radar through the Mac bridge, 600 s (Motion and Presence 2D, PresenceDetect.cfg) | 2310 frames at 3.85 a second, no gaps in the radar's frame numbers, 2310 shown, 0 ring drops. Frame arriving at the Mac to frame on the panel: median 110.8 ms, p95 153.2, max 255.1; on the board, last byte to end of redraw: median 51.6, max 122.1. Presence changed 7 times; each change reached the panel in 111 to 120 ms. Seen by a person: frame counter climbing, "Someone is here" held while a person sat in the zone |
| Two-page screen, firmware 97c3a0a2 | Thermostat page plus Sensors page (radar and Wi-Fi columns, 30 s strips) running on the G3519; LVGL heap 28 kB; ring drops rise at 81 percent CPU on the Sensors page |
| Fast-clear configuration (`minor2emptyThre` 6), seated person | Failed the expectation: the demo reported empty for 11 minutes 19 seconds while a person sat still at the bench, and re-armed only when they walked in. See the finding below |
| 12-frame hold (`minor2emptyThre` 12), room empty overnight | Ran overnight; result on the [radar antenna page](2026-10-10-radar-antenna-eval.md); the walk-out clear time is not computed |
| Wired radar, radar to Home Assistant latency | not run |

**Fast-clear finding.** To make the badge clear faster than the shipped hold of about 5 s, the shipped configuration was changed to `minorStateCfg ... minor2emptyThre 6` (`PresenceDetect-fastclear.cfg`). With a person seated at the bench the demo then reported empty for 11 minutes and 19 seconds and showed presence again only when the person walked in. TI's page for the demo says a motion status is kept if at least one motion detection was recorded in the last `minor2emptyThre` frames. With a small count, a still person's small motions fall outside the window and the state machine does not re-detect until a larger motion, so the setting stops holding a seated person. **Tool or docs.** If you shorten the hold, test it with a seated person before putting it on a live screen. A value of 12 was used for the overnight run. A hold of 6 should not be used for a seated person.

**The line.** Not reached: the wire from the radar and the radio leg. Two things gate them. The first is four jumper wires and a switch change, which need the radar running TI's demo. The second is the CC35X1 image: the build needs the home network name and password put into the firmware, and the image replaces the Matter light that is paired in Apple Home; the pairing probably has to be redone.

**Why the wired path waits.** TI's prebuilt Motion and Presence 2D demo sends nothing after boot until its configuration (`PresenceDetect.cfg`, 27 lines, about 7 s) is sent over the CLI, and the CLI is the same UART. The flash pass measured this on 2026-10-10; the radar has no command to save the configuration. With S1.4 ON the Mac cannot send it, so the radar's own wire needs a host that sends the cfg after each boot. The earlier shipped image streamed without it, but it was overwritten by the first flash and cannot be read back. The live numbers above therefore come from the Mac bridge (`thermostat-system/tools/bridge.py`: the Mac sends the cfg, reads the radar's USB port and forwards each frame to the G3519).

**One-wire fix, not built.** Wire a spare G3519 UART TX to J8 pin 7 (the radar's RX), bake the 27 cfg lines into the screen firmware, and send them after the radar's banner at every start. Then the radar's J8 pin 5 to the G3519 carries the frames as in the wiring picture, and the Mac is not needed.

**Open points about this page's own numbers.**

- The 89.7 ms replay figure includes the Mac's write time, so it is a bench figure for "radar frame to panel", not the radar's own latency. The real radar adds its frame time (about 250 ms between frames, unmeasured here).
- One STAT line in 71 had a SHOWN line spliced into it: a line printed from inside a refresh event. Unexplained; no frame was lost.
- A single replay run and a single live run, not three.
- The live run cannot give the radar's detect or clear delay: the entry and exit were not timed, so those rows are not measured.
- The radar capture has no person in it; presence in the replay is rewritten, so this page says nothing about detection.

**Ask.** Whether the demo as shipped streams on J8 pin 5 with S1.4 ON and a reset alone, or needs rebuilt radar firmware. The measured behavior above (silent until the configuration is sent) says the configuration step is needed.

**Restyle (2026-10-11).** The screen is redrawn in the Fox Red Labs palette (Paper background, Ink text, Fox Red only for the presence badge and logo, Ink-outline buttons), with the horizontal logo in the header (A8 image, 120x59, `g3519/src/frl_logo.c`) and Barlow 14/20/48/64 fonts generated by `tools/make_assets.py`. Same pages, buttons, SHOWN and STAT lines. Size: flash 249.4 kB before, 191.8 kB after (Montserrat 48 was 634 kB of source; text+data from `tiarmsize`), RAM (bss) 27.7 kB before and after, 33.7 kB with the stack raised to 12 kB. It needs `LV_DRAW_SW_SLIM 0` and `LV_USE_IMAGE 1` (the slim patch removes the image decoder; drawing an image with it crashes). A host render of both pages checked the layout, and the board then ran with the radar live at 6 to 8 screen refreshes a second with no drops. If the board prints its start-up banner and then no STAT lines, the LVGL heap is too small: the two-page screen peaks at 14.9 kB, and a 12 kB heap hard-faults in `lv_draw_fill_dsc_init`. Build with `MEM_KB=28` (now the default in `build.sh`; flash 191.7 kB, bss 43.7 kB).
