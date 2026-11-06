# How do a TI CC1354P10 and a CC2340R5 form one Zigbee network with no home network?

Works: the CC2340R5 joined the CC1354P10's network 6.2 s after boot (2026-10-06). Two TI boards, two different TI Zigbee stacks, one private network on the desk. The LP-CC1354P10-1 runs TI's `zc_light` (Z-Stack, F2 SDK 8.33) as coordinator; the LP-EM-CC2340R5 runs TI's `onoff_light` (TI Zigbee stack, F3 SDK 9.21) as router.

## Result (2026-10-06)

| | |
|---|---|
| Network | Formed by the CC1354P10: PAN 0xe99d, channel 11, coordinator address 0x0000 |
| Join | The CC2340R5 reported "Device (1) STARTED OK" and "Successful steering" 6.2 s after it booted |
| Coordinator view | `FOXRED JOIN 1: short 0xc8dd` (this repo's device-announce report) |
| Stacks | Z-Stack (F2) coordinator with the F3 SDK's Zigbee stack as router: they interoperate |

## What it took

- **Coordinator** (`../../cc1354p10/zigbee-coordinator/`): TI's example forms a network only from its serial menu. `app/zcl_samplelight.c` is a copy of TI's file with two changes: start commissioning with formation and steering at boot, and print every ZDO Device Announce on the status line. Built with CCStudio's command line once, then plain `gmake` (see `../../cc1354p10/README.md`).
- **Stale network settings**: the first runs never formed a network. The board still held Zigbee settings from the earlier DMM image. "RESET TO FN" in the serial menu cleared them and the network formed at once.
- **Starting a new image on the CC1354P10** without a power cycle: `cc1354p10/tools/ccs_load_run.py` (CCStudio scripting: load, run, disconnect).
- **Driving TI's serial menu** from the Mac: `tools/cui.py` (arrow keys and Enter over the backchannel UART).
- **Router**: the earlier CC2340R5 build (`cc2340r5/zigbee/onoff_light`, all 16 channels), flashed by `run_zigbee_b2b.py`, which backs up the CC2340R5 first and writes its previous image back afterward.
- **Decoding the CC2340R5's binary log on a Mac**: TI's `tilogger` from the F3 SDK, installed without its Wireshark stream (that one pins the Windows-only `pywin32` and fails on macOS), with the known `struct.unpack("L")` fix, fed the captured bytes through a pseudo-terminal (`tools/replay_tilog.py`).

Terms: [docs/vocab.md](../../docs/vocab.md). **Coordinator** forms the network; **router** joins and relays; **PAN** is the network's ID.
