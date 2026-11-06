# How do a TI LP-CC1354P10-1 and an LP-EM-CC2340R5 join one Thread network?

Works: a CC1354P10 leader and a CC2340R5 joiner formed one Thread network over the air, 20 of 20 pings answered (2026-10-06). No Linux, no border router. Both are TI's own OpenThread images from `ot-ti` (release `thread-v1.4-ti-2.1`), on two different TI chip families and two SDKs.

| Board | Role | Image | How it is driven |
|---|---|---|---|
| LP-CC1354P10-1 (CC13x4, F2 SDK) | Leader | `ot-cli-ftd` (full Thread device with command line) | Its own debugger UART at 921600 |
| LP-EM-CC2340R5 (CC23x0, F3 SDK) | Joiner, became a **router** | `ot-rcp` (radio co-processor) with OpenThread's POSIX host `ot-cli` on the Mac doing the Thread work | Its debugger UART at 921600, spinel over HDLC |

The three jumper wires between the boards (for the border-router test) were in place but unused here: both links to the Mac are the debugger UARTs.

## Result (2026-10-06, measured)

Numbers below come from two passes: the first that joined, and a later 10-ping pass. The later pass wins where both measured the same thing.

- **Leader up** 8 s after `thread start` on a fresh image (first pass; 34 s in a later pass that re-checked an already-running leader, which measured polling, not forming).
- **Joiner attached** and the leader's neighbor table lists it as a router at RLOC16 0xf400, average RSSI -30 dBm; the joiner lists the leader at RLOC16 0x8000. Boards about 30 cm apart.
- **Ping, 32 bytes, 10 each way, 20 of 20 answered.** Joiner to leader: median 44 ms (31 to 103). Leader to joiner: median 42 ms (32 to 47). RSSI -30 dBm seen by the leader, -29 by the joiner.
- **Rejoin with the radio already running:** 15.5 s from `thread start` to attached (child first, then router). The first join of the night took longer only because the Mac host was reconnecting after `factoryreset`.
- **An RCP leaves the mesh when its host exits.** Quitting the Mac program dropped the joiner to `disabled`; the Thread state of a radio co-processor lives in the host, not the chip. Its dataset survived (stored by the host), so `ifconfig up` and `thread start` brought it back.

## What it took

1. **The CC2340R5 as an RCP.** Its `ot-rcp` image from the earlier Thread work had never been flashed. Two things made it run: fill the CCFG CRC fields (Zephyr's `ti_ccfg_crc32_calculate.py`; `ot-ti` has no post-build CRC step; CCFG is the chip's configuration area in flash), and reset the board after the DSLite flash (`xds110reset`), which is enough on the CC23xx. The Mac host then reported `OPENTHREAD/1.4.0.0; CC23XX thread-v1.4-ti-1.0-ea-1.0`. If the serial port shows garbage at every common baud rate, set **921600**: `ot-ti` runs its UART there.
2. **The CC1354 as leader.** Loaded with CCStudio scripting (`cc1354p10/tools/ccs_load_run.py`), which starts a program without a power cycle. `factoryreset` reboots the chip and a debugger-loaded program does not come back from that without a power cycle: the script no longer sends it; a fresh image has no network anyway.
3. **Handing over the network.** `dataset active -x` on the leader gives one hex string; `dataset set active <hex>` on the joiner. The joiner's `factoryreset` reboots the RCP and the Mac host reconnects over spinel, which takes a few seconds; a dataset sent in that window is silently lost. Wait for `state` to answer, then send, then confirm with `dataset active` (channel and PAN must match).

Script: `board-to-board/thread_two_radios.py <ot-cli posix binary> <out dir>`.

## What it shows

- Two TI radios from different chip families and SDKs share one Thread mesh. The [Zigbee run](../zigbee-cc1354-cc2340/README.md) did the same on the same two boards.
- The RCP path is the one a Thread border router uses, so the border router's radio link works end to end ([border-router](../../border-router/README.md)).
- The CC2340R5 cannot be a Thread node on its own (36 kB RAM; see [its Thread page](../../cc2340r5/thread/README.md)), but as a radio for a bigger host it is a full router on the mesh.

Terms: [docs/vocab.md](../../docs/vocab.md). **RCP** is a radio co-processor: the chip does the radio, a host does the Thread logic. **RLOC16** is a device's short Thread address. 1 kB = 1,024 bytes.

## Not measured

Range, packet loss over an hour, join time with the RCP already running (the joiner's time was dominated by the Mac host reconnecting after `factoryreset`), and any sleepy-device behavior.
