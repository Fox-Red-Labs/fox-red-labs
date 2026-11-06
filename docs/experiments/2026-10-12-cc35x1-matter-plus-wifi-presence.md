<!-- experiment
id: E188
board: LP-EM-CC35X1
date: 2026-10-12
status: in progress
reached: run
line: It fits, boots, pairs and runs. The Matter thermostat and TI's CSI presence pipeline link into one image with 12.1 kB of main RAM and 0.2 kB of TCM to spare. The pipeline's buffers are routed to main RAM, because TI's script sends every .bss to the full 128 kB TCM. Three buffer trims took 147 kB down to 84.5 kB, and the Matter heap is 32 kB smaller. The image has 120.8 kB more flash. Heap free is 119 kB at idle and 94.6 kB with the pipeline running. First pairing, 2026-10-12 23:22: Apple Home paired it and the board joined the house Wi-Fi. The pipeline started 3 s later and gave a window every 2.08 s at 131 to 135 packets a second, and the Occupancy Sensing endpoint followed it. The commissioning was then lost to a console mishap and a reset before Apple's final step was seen. The morning re-pairs it and tests a reboot.
stop: none
-->
# Can one LP-EM-CC35X1 run the Matter thermostat built here and TI's Wi-Fi CSI presence detection at the same time, and what does it cost?

**Short answer.** Yes, it fits and runs. One image holds the Matter thermostat built here and TI's Wi-Fi CSI presence pipeline: 1,862.5 kB of flash (120.8 kB more than the thermostat alone), 12.1 kB of main RAM and 205 B of TCM left after the link. Apple Home paired it over Bluetooth, the board joined the house Wi-Fi, TI's pipeline started 3 s after the join and produced a window every 2.08 s (median; 2.08 to 2.20 s) at 131 to 135 CSI packets a second, and each result reached the Matter Occupancy Sensing endpoint. The pipeline costs 24.7 kB of heap while running.

Three things made it fit, none of them described in TI's documentation: the pipeline's buffers routed to the main RAM by one linker-script rule (TI's Matter script sends every `.bss` to the 128 kB TCM, which the thermostat already fills), three buffer trims that cut the pipeline from 147.3 kB to 84.5 kB without changing the arithmetic, and a 32 kB smaller Matter heap.

Not measured: compute time per window (the cycle counter read zero), a setpoint write with the pipeline running, an empty room, and whether a paired board keeps its pairing after a reboot. The first pairing was lost when the board was reset before Apple's final commissioning step showed in the log (see Mistakes to avoid).

Terms: [vocab](../vocab.md). CSI is channel state information, the per-subcarrier amplitude and phase a Wi-Fi receiver sees for each packet. TCM is the chip's tightly coupled memory, 128 kB beside the 509 kB main data RAM. CDE is the Cortex-M33 custom datapath extension the model's convolutions run on. Related: [Matter thermostat](2026-10-10-cc35x1-matter-thermostat.md), [TI's CSI presence example and what it detects](2026-10-09-cc35x1-wifi-csi-presence.md).

Contents: question, vendor position, expectation (written before the build), what was tried, how far it got (the tables), mistakes to avoid.

**Question.** TI ships Matter over Wi-Fi for the CC3551E (one example, the light) and, separately, a Wi-Fi presence example on the same chip. A thermostat product would want both in one image: the radio joins the house Wi-Fi once, Matter talks to the home apps, and the same radio's CSI feeds an occupancy sensor. Does it fit in the chip's RAM, does Matter still pair and respond, and what does the presence pipeline cost in CPU and memory?

**Vendor position.** Neither TI example mentions the other. The CSI live-preview README (plugin 1.20) lists one board, one image and Edge AI Studio; the Matter release notes for `matter-v1.5-ti-1.2-ea.2` list the lighting app. The Wi-Fi SDK 10.20 ships the CSI driver (`csi.c`) inside the same `wifi_stack` library the Matter image links, so nothing in the SDK rules the combination out. TI gives no RAM figure for either stack.

**Expectation (written before the first build).** From the two images' maps, measured 2026-10-12:

| What | Matter thermostat (thermostat image) | TI CSI live preview | Combined, as shipped |
|---|--:|--:|--:|
| Static RAM, TCM (128 kB) | 127.4 kB used | 65.1 kB used | TCM is full on the Matter side |
| Static RAM, main DRAM (509 kB) | 443.9 kB used, 324 kB of it the FreeRTOS heap | 508.9 kB used, 200 kB of it heap | about 65 kB spare |
| CSI pipeline statics | | about 146 kB of buffers (raw store 62 kB, two 128 by 52 float grids 27 kB each, features 27 kB), 5.5 kB model state, 10 kB of stacks | about 160 kB wanted |

So the first link should fail by roughly 95 kB. The fit path, in the order to try it: store the raw CSI as the int8 pairs the radio delivers instead of floats (62 kB to 31 kB), drop the transposed copy by interpolating straight into the transposed grid (27 kB), alias the model input to the feature grid (7 kB), and trim the Matter heap, which had 147 kB free at idle on the thermostat image, by 40 kB. That leaves the combined image about 10 kB inside the main RAM with the heap at 284 kB. CPU: TI quotes 13.9 ms a window for the model on the CDE path; the feature extraction is 52 FFTs of 128 points plus 64 of 26, a guess of 5 to 15 ms; one window every 2 s is under 2 percent of one core. Matter pairing and setpoint writes should be unaffected; the presence result should reach the Occupancy Sensing endpoint within one window (2 s) plus the 3-window average (6 s). Risks named before the build: the model library was built by TI's compiler and is linked by GCC (same EABI and hard float, should link); the CDE coprocessor must be enabled at boot, which the EdgeAI SysConfig module does in the example and a hook written here does in this image; the CSI solicitation adds a null-data exchange every few beacons and could add jitter to Matter traffic (a setpoint write is to be timed).

**What was tried.** Built from the thermostat's generator with a flag: TI's `csi_presence.c`, feature extraction and model library (byte for byte from the plugin, under `cc35x1/matter/thermostat/csi/ti/`), the CMSIS-DSP subset they need (ten source files from the same plugin), and one glue file: the coprocessor enable from TI's generated code, a watcher task that waits for Matter's network commissioning to join the house Wi-Fi and then issues the same three driver calls the example's `csi_enable`, `csi_config -s 1 -p 7` and `csi_set_mac` console commands make, the example's reader thread, and the result routed into the thermostat's Occupancy Sensing endpoint instead of the DAP serial protocol. The Wi-Fi result drives occupancy only while no radar line has arrived for 5 s; with a radar line present it is logged and the radar wins. Build: `cc35x1/matter/build-thermostat.sh` with the CSI flag set; a second variable sets the FreeRTOS heap size.

**How far it got.** Link and boot at 22:25 on 2026-10-12, pairing at 23:22, the Wi-Fi join and 13 presence windows. The Matter timing and a setpoint write were not seen.

| Step | Result |
|---|---|
| Compile under GCC 12.3 | TI's pipeline files and the CMSIS-DSP subset compile unchanged except two edits: the DAP serial reporting replaced by a callback, the UDP keepalive removed (lwIP sockets are off in the Matter build) |
| Link, as shipped | Fails: TCM over by 147.3 kB, the pipeline's 147.3 kB of buffers on top of a full TCM |
| Link, buffers in main RAM, heap 292 kB, no trims | Fails: main RAM over by 51.1 kB |
| Link, buffers in main RAM, heap 292 kB, three trims | Yes: 12.1 kB of main RAM and 205 B of TCM left; the pipeline's statics are 84.5 kB |
| Flash and boot | Yes: erased and programmed in 76 s, boot line, Wi-Fi start, Matter server, `THERMO csi: Wi-Fi presence pipeline armed`, Bluetooth advertising; heap free 116.5 kB, minimum 113.6 kB, steady over eight heartbeats |
| Pair over Bluetooth, 23:22 | Yes: PASE in 3 steps, fabric added (ACL entry for fabric 1), fail-safe armed 60 s then 117 s |
| Join the house Wi-Fi | Yes: `STA Connected` on channel 157 about 4 minutes after boot, when Apple sent the credentials; heap free fell from 119.3 kB to 112.5 kB |
| Presence pipeline start | Yes: `[CSI] started toward <access point>` 3 s after the join; first window 6 s after the join; heap free 94.6 kB with it running, minimum 90.2 kB, steady over the 13 windows seen |
| Windows | 13 in 27 s before the console was lost: period 2.08 s median (2.08 to 2.20), 131 to 135 packets a second, class present in all (people home, as the CSI presence page predicts), probability 0.62 to 1.00 |
| Occupancy endpoint | Yes: the cluster logged `Occupancy detected` after every window; no radar line, so the Wi-Fi result drove it |
| Compute time per window | Not measured: the DWT cycle counter read 0 in every line (**Our mistake**: counter enable not checked on this part); the next image uses the tick clock |
| Apple's final step, a setpoint write, a reboot after pairing | Not seen: console lost at 23:23, reset at 23:26, board back in commissioning mode with no network |

| Size | Thermostat alone | With the CSI pipeline | Change |
|---|--:|--:|--:|
| Flash (text plus data) | 1,783,408 | 1,907,152 | +123,744 (120.8 kB; CMSIS tables 74 kB, model and feature code) |
| RAM (data plus bss, heap inside) | 593,248 | 647,444 | +54,196 (52.9 kB; pipeline +84.5 kB, heap -32 kB, trims) |
| Heap free at idle, minimum | 150,368 | 116,376 | -33,992 (33.2 kB) |
| Main RAM left after the link | about 65 kB | 12.1 kB | |
| TCM left after the link | about 0.6 kB | 205 B | |

**The line.** No hard stop found. The margin is the limit: 12.1 kB of main RAM and 205 B of TCM after the link, and heap free at 94.6 kB with the pipeline running (minimum 90.2 kB) against 146.8 kB for the thermostat alone. TI's linker script sends every `.bss` to the 128 kB TCM, so the pipeline's 147.3 kB of buffers overflowed it by that amount as shipped; routing them to main RAM, three buffer trims (147 kB to 84.5 kB) and a 32 kB smaller Matter heap made it fit. TI ships no example that combines Matter and CSI on this chip; the combination is a first. Still unmeasured: compute time per window (the cycle counter read 0; the tick clock replaces it in the next image), a setpoint write with the pipeline running, and an empty room.

**Ask.** Does your Matter build on the CC3551E keep lwIP sockets on? If so, TI's keepalive compiles unchanged and the one edit above goes away. And anyone with a CC35X1 and a Home app: pair the combined image and report whether the Occupancy sensor appears beside the thermostat.

## Mistakes to avoid

- Start the console capture once, before programming, with a timeout longer than the run, and do not restart it. After Bluetooth has started, this board's log UART gives a reopened port nothing (0 bytes in 8 s on a direct read, while the Matter service was still advertised on the network).
- Do not reset a paired board until the log shows `Commissioning completed successfully`. A reset before it left `no saved network; empty config`, no fabric and a new commissioning window.
- TI's `csi_presence.c` sends a 1-byte UDP broadcast through lwIP's socket API as a keepalive. Matter's lwIP build has sockets off, so remove it; Matter's own traffic keeps the station alive.
- TI's `arm_math.h` reads the newer CMSIS-DSP `dsp/` header folder. Copy that folder with the ten CMSIS-DSP source files.
- With the buffers in main RAM, a 292 kB FreeRTOS heap and no trims, main RAM overflowed by 51.1 kB. That agrees with the arithmetic (120 kB of Matter statics, 292 kB of heap and 151.5 kB of CSI buffers against 509 kB) to within 3 kB.
