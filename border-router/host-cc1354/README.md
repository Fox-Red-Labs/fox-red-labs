# Can a TI LP-CC1354P10-1 run a Thread border router's host side, with a LP-EM-CC2340R5 as its radio and no Linux?

Yes, the first half. On 2026-10-07 the CC1354P10 ran OpenThread as a full Thread device whose radio was the CC2340R5 (TI's `ot-rcp` image) on a UART wired between the two LaunchPad development kits, formed a Thread network and became leader within 16 s of `thread start` on a fresh dataset, then again in 37 to 39 s after each of three `thread stop` and `thread start` restarts. 61 frames sent, every error counter 0, still leader after a 60 s soak. No Linux, no Raspberry Pi: two TI chips and three jumper wires. It then joined a home Thread network through an Apple TV border router, and an hour of pings from a Mac on Wi-Fi came back 3,584 of 3,600 (0.44% loss, median 28.5 ms). This is milestone M1 of [the border-router plan](../README.md); Wi-Fi and the routing between the two networks come next.

Terms: RCP (radio co-processor), spinel, HDLC, FTD and leader are defined in [docs/vocab.md](../../docs/vocab.md). CCFG is the chip's boot configuration area; EUI64 is a radio's 64-bit factory address; CCStudio is TI's Code Composer Studio IDE and DSLite its command-line flasher. Sizes: 1 kB = 1,024 bytes. Where builders get stuck on this board: docs/ways-in.md.

## What ran

| Board | Image | Role |
|---|---|---|
| LP-CC1354P10-1 (CC1354P10, Cortex-M33, 1 MB flash, 256 kB RAM). SysConfig and the build call this board `LP_EM_CC1354P10_1`: same header and pins, the LP-CC1354P10-1 adds the onboard XDS110 debugger | `otbr-host-cc1354.out`, built here | Host: OpenThread full Thread device with the CLI on the debugger UART. Its own 802.15.4 radio is not used. |
| LP-EM-CC2340R5 (CC2340R5, 36 kB RAM) | TI `ot-rcp` from `ot-ti` thread-v1.4-ti-2.1, UART moved to the header pins, CCFG CRCs filled | Radio: receives spinel commands over HDLC at 921600 baud and runs the 802.15.4 radio |

Wiring: three wires, [../WIRING.md](../WIRING.md) (host DIO23 to radio DIO12, radio DIO18 to host DIO22, ground to ground). The RCP image is built from `ot-ti` with the SysConfig file in [../rcp-cc2340r5-hostpins.syscfg](../rcp-cc2340r5-hostpins.syscfg) as WIRING.md describes, then its CCFG CRC fields are filled with upstream Zephyr's `soc/ti/simplelink/cc23x0/ti_ccfg_crc32_calculate.py` (see [cc2340r5/thread/README.md](../../cc2340r5/thread/README.md)). Built images are not in this repo; build them as described.

Proof that the radio is the other chip: the host reports EUI64 `a02c0020082c0020`, the CC2340R5's address. The CC1354P10's own address starts with TI's `00:12:4B` prefix, so the two cannot be confused.

## Results (measured 2026-10-07)

| What | Number | Basis |
|---|---|---|
| Host image with console logging compiled in (the image that ran) | 437.8 kB of code and constants, 125.9 kB of RAM (18.9 kB initialized plus 107.0 kB zeroed) | `size` output |
| Host image with logging left out | 407.5 kB of code, 117.9 kB of RAM | build output 2026-10-07; not saved |
| `thread start` to leader, fresh dataset | leader at the first check, 16 s after `thread start` (one check, so the true time is anywhere up to 16 s) | console timestamps: start at 8.8 s, `leader` at 24.9 s |
| `thread start` to leader after `thread stop`, three runs | 37.1, 39.4, 37.2 s; each poll of `state` takes about 1.6 s, so the true times are up to 1.6 s shorter | bench script run |
| MAC counters after the three runs | TxTotal 61, RxTotal 3, every error counter 0 | bench script run |
| Soak | still leader 60 s later | bench script run |

Why a restart takes 37 s and the first form 16 s: **Unknown**. Suspected: after `thread stop` the device still holds a network it was leader of, and OpenThread tries to re-attach to that network before deciding to lead it again; on a fresh dataset there is nothing to look for. Not checked: the same stop-and-start on this board with TI's own `ot-cli-ftd` and the on-chip radio, which would separate the stack's behavior from the UART link. For scale, that image formed a fresh network (not a restart) in about 8 s with its on-chip radio, against this page's 16 s polled figure ([board-to-board/thread-two-radios-2026-10-06](../../board-to-board/thread-two-radios-2026-10-06/README.md)).

Not measured: a second Thread device joining this network (the only other radio on the bench is the one being used as the RCP), latency across the UART link, and current. Those come with the CC2755P10 and the next milestones.

## It joined a home Thread network, and a Mac reached it through the Apple TV (2026-10-07)

With the Apple TV's Thread network dataset (read from Home Assistant after the iPhone app synced Apple's credentials), the host joined the home Thread network (its name is kept out of this repo) on channel 25 through the CC2340R5 radio, in six runs: `child` 3.5 to 4.7 s after `thread start`, promoted to `router` within a minute in three of them; parent the Apple border router (link quality 2 in, 1 to 2 out). The home's network data arrived: on-mesh prefix `fd7c:3830:129e::/64`, a default route and a NAT64 prefix via the Apple TV, SRP and DNS services.

Measured from this Mac on the home Wi-Fi, after a route to the Thread prefix was added by hand (macOS does not install the route the Apple TV advertises; `sudo route add -inet6 fd7c:3830:129e::/64 <Apple TV link-local>%en0`):

| What | Number | Basis |
|---|---|---|
| Mac to host, through the Apple TV, 10 pings at 0.5 s | 10 of 10, 20.5 / 50.9 / 142.0 ms min / avg / max | ping output |
| Mac to host, 30 pings at 0.3 s | 30 of 30, 19.4 / 32.1 / 98.9 ms | ping output |
| Service the host registered with SRP (`frl-cc1354`, `_frl._udp`, port 12345), seen from the Mac | `dns-sd -B` lists it; `dns-sd -L` resolves it to `frl-cc1354-host.local.:12345` via the Apple TV's advertising proxy | `dns-sd` output |
| Host to Mac (the Mac's global address), 10 pings | 0 of 10 | host console |
| Host to its parent's mesh address, 5 pings | 1 of 5 (60 ms) | host console |
| Host to the Apple TV's Wi-Fi address and to 8.8.8.8 through NAT64 | 0 of 5, 0 of 3 | host console |
| Task stack high-water mark after all of this | 4.7 kB used of 32 kB (7,026 of 8,192 stack words still free) | host console |

Pings into the host work; pings out of it reach the mesh but not the Wi-Fi side. The 2026-10-08 battery below settles where it stops: the host's pings to its Apple TV parent on the mesh answered 60 of 60, its pings to this Mac 0 of 60. So the host's radio path works both ways, and what fails is a packet from the Thread mesh out to Wi-Fi, in that direction only, while the same hop carries the Mac's pings in and their replies back out. It is not this repo's code: TI's own `ot-cli-ftd` on the CC1354P10's on-chip radio, joined to the same network on 2026-10-08, shows the same pattern (to its Apple TV parent 10 of 10, 21 ms; to this Mac 0 of 30; Mac to board 30 of 30, 29 ms average; saved ping output), and the Mac's firewall is off (`socketfilterfw`: disabled, stealth mode off). Cause: **Unknown (suspected: Third-party behavior, the Apple border router not forwarding echo requests that start on the mesh towards Wi-Fi)**. Not checked: a different border router (Home Assistant's own, once the ZBT-2 carries Thread), and other traffic than ICMP from the mesh out.

The 16 kB stack fault (item 6 below) is gone with 32 kB. A second fault of the same class (`CFSR 0x00100000`) happened twice, on 2026-10-07 and 2026-10-08, both inside `vsnprintf` during a console command in the first seconds after joining, with the stack pointer at the stack base while the high-water mark says only 4.7 kB of 32 kB was ever used, which a slow fill cannot produce. Cause: **Unknown (suspected: a corrupted stack pointer rather than a deep stack)**. The image now dumps every return address on the stack when it faults; it ran the two hours of the battery below without faulting, so there is no backtrace.

## Two hours on the home network (measured 2026-10-08)

The host joined the Apple TV's network as a child and ran an unattended test script, with nothing on the bench touched.

| What | Number | Basis |
|---|---|---|
| Rejoin after `thread stop` / `thread start`, dataset already held | child in 3.7, 3.4 and 3.7 s (polled about every 0.3 s) | test log |
| **One hour of pings from the Mac through the Apple TV, 1 per second** | **3,584 of 3,600 answered (0.44% loss); median 28.5 ms, 95th percentile 70.9 ms, 99th 110.7 ms, min 18.8, max 197.7** | ping output |
| Host's MAC counters across the hour | 3,787 frames sent, 9,007 received, every error and retry counter 0; still a child at every 10-minute check | test log |
| Task stack across the hour | 4.7 kB used of 32 kB, unchanged at every check | test log |
| UDP from the Mac to the host, 200 datagrams at 20 per second | 32 B: 200 of 200; 128 B: 200 of 200; 512 B: 149 of 200; 1 kB: 78 of 200 (about 57 kbit/s delivered) | test summary |
| Pings out of the host, two sets of 3 rounds of 10 | to its parent on the mesh: 60 of 60, 26 and 29 ms average; to this Mac: 0 of 60 | test log |

The UDP drop above 128 bytes is what the link allows, not a fault: a 512 or 1,024 byte datagram is split into 5 or 9 radio frames, and 20 of them a second is more than a Thread link carries (802.15.4 runs at 250 kbit/s on air before any overhead). Not measured: the delivered rate at a send rate the link can sustain, which is the throughput number worth quoting.

Rows in `data/results/cc1354p10.csv`: `packet_loss_1h` 0.44%, `b2b_rtt` to the Mac 28.5 ms median, `radio_join` 3.4 to 3.7 s on restart and 3.5 to 4.7 s on a fresh join.

## How it is built

`./build.sh <build dir>` configures and builds in about 4 minutes on this Intel MacBook, macOS 15.7.9 (watched, not timed). It needs TI's OpenThread port `ot-ti` (release thread-v1.4-ti-2.1, commit `eaac0de`, with its OpenThread submodule at `c9c19aa`) in `~/ot-ti`, GCC 12.2 (Zephyr SDK), CMake 3.31, ninja, SysConfig 1.26.3 and the CC13xx/CC26xx SDK 8.33.00.16, set up as in [cc2340r5/thread/README.md](../../cc2340r5/thread/README.md).

The build is `ot-ti`'s own platform for the CC1354P10 with one part swapped. Line counts are `wc -l`, blank lines included:

| Piece | From | Lines here |
|---|---|---|
| Alarm, settings in flash, entropy, console UART, process loop (`alarm.c`, `settings.c`, `entropy.c`, `uart.c`, `system.c`) | `ot-ti/src`, unchanged, by path | 0 |
| On-chip radio (`radio.c`, `diag.c`) | left out | 0 |
| Radio over spinel: `RadioSpinel`, `SpinelDriver`, HDLC | OpenThread's own libraries (`openthread-radio-spinel`, `openthread-spinel-rcp`, `openthread-hdlc`) | 0 |
| `platform/spinel_uart.cpp` and `.hpp`: the spinel interface over a TI UART2 instance (ring buffer filled by the read callback, drained in the OpenThread task, a semaphore for the wait) | written here, modeled on OpenThread's POSIX `hdlc_interface.cpp` | 328 |
| `platform/radio_spinel_glue.cpp`: init, the process-loop and timer glue, and the `otPlatRadio*` functions | the first 188 lines written here; the 453 lines after the marker comment are OpenThread's POSIX `radio.cpp` wrappers, unchanged | 641 |
| `platform/openthread.syscfg` | `ot-ti`'s, minus the RF core and NCP SPI, plus the second UART and no standby | 1 file |
| `app/freertos_main.c` (`ot-ti`'s FreeRTOS entry with a 16 kB task stack) and `app/die.c` (crash reporter); `main.c` and `cli_uart.cpp` are `ot-ti`'s by path | | 174 |

Config: `platform/openthread-core-host-cc1354-config.h` is `ot-ti`'s CC13xx/CC26xx config with diag off (no on-chip radio to drive), the CSL receiver off (CSL is Thread's scheduled listening for sleepy devices; a mains-powered router never sleeps, and RadioSpinel has no CSL receiver hooks), and logs on the console (`log level 5` shows every spinel frame). `platform/openthread-host-lib-config.h` sets the spinel receive buffer to 2 kB (the POSIX host uses 8 kB).

## How it is run

`python3 m1_bench.py run <host.out> <rcp.hex>` takes the host image from `build.sh` and the RCP hex described above, and does the whole thing with the safety rules of `tools/demo_flash.py` (repo root): backs up the CC2340R5's flash and CCFG with DSLite (length in words), flashes the RCP image, pulses its reset with `xds110reset` (a DSLite flash alone does not start it), backs up the CC1354P10 in a CCStudio scripting connection and loads the host (`tools/ccs_backup_load.py` in this folder; a DSLite flash would not start it without a power cycle), then talks to the host CLI on `/dev/cu.usbmodem<id>` at 921600. `python3 m1_bench.py restore <run dir>` puts both boards back and verifies. Backups go to a folder outside the repo, because they can hold network keys.

Two console tools: `tools/console_capture.py` (this folder) keeps every byte the console prints with a timestamp, including the last words before a hang; `tools/ot_cli.py` (repo root) is enough for commands.

## What went wrong, and the fix

1. **The console went silent within a second of the first radio commands.** `dataset commit active` answered at 4.7 s; `ifconfig up` (5.7 s) and `thread start` (8.8 s) got no echo and no reply, and the log ends with a lone `i` flushed when the capture stopped ; no assert, no fault, no exit from the crash reporter. Cause: **Unknown (suspected: Tool or docs)**. The SysConfig default power policy lets the CC1354P10 enter standby whenever no driver objects. With the policy set to `PowerCC26XX_doWFI` (never standby) in `openthread.syscfg`, the next run formed the network , and a border router has no reason to sleep. Not checked: whether the RCP link or the console dropped first (only the console stall is in the saved log), and whether a pending callback-mode read on the second UART should have kept the chip awake (TI's UART2 documentation does not say).
2. **Two link errors on the first build**: `undefined reference to 'otPlatRadioUpdateCslSampleTime'` and `'otPlatRadioEnableCsl'`. **Our mistake**: `ot-ti`'s config turns the CSL receiver on and RadioSpinel does not implement those hooks. Off in this repo's config.
3. **`"OPENTHREAD_CONFIG_PING_SENDER_ENABLE" redefined [-Werror]`** stopped the build in mbedtls. **Tool or docs**, small: `ot-ti`'s config header sets the ping sender on, OpenThread's CMake derives the same option from `OT_APP_CLI` (off here) and passes `=0`. `-DOT_PING_SENDER=ON` in `build.sh`.
4. **The first crash reporter said nothing.** **Our mistake**: it called `UART2_open` on the console UART, which `ot-ti` already holds, and that returns NULL. `die.c` now writes straight to the UART0 registers.
5. **A debugger connect resets this chip**, so the halted state after a hang is the reset vector, not the hang. **Tool or docs**: the CC1354P10's GEL script (the debugger's board setup script) runs a board reset on connect, printing `GEL Output: Board Reset Complete`. Post-mortems on this board need a reporter in the firmware (`die.c`), not the debugger. The debugger also failed three times with `Error -242 ... A router subpath could not be accessed` while the board's previous Bluetooth demo was running; a power cycle cleared it. Both in docs/ways-in.md.
6. **Stack overflow in the OpenThread task, about 4 s after joining a real network.** **Our mistake.** The Cortex-M33 stack-limit check raised a UsageFault (`CFSR 0x00100000`, STKOF) inside `vsnprintf` with the stack pointer 40 bytes above the task stack's base; the 16 kB task stack is exhausted when a console command formats output while the stack is busy with a live network. Seen three times at the same command, the first console command after joining. The task stack is now 32 kB and `frl status` prints its high-water mark; with it the host ran six joins and twenty minutes on the Apple network with 4.7 kB of stack used (section above).
7. **Every reporter was mute for most of the day.** **Our mistake.** Writing to the UART data register does nothing while TI's UART2 driver has the transmitter disabled (it enables TXE only around its own writes). The fault handler, assert hook and exit hook printed nothing, and the three hangs were wrongly read as "no fault". Now every polled write sets `UART_CTL_TXE` first. Rule: a reporter is tested once on purpose (`frl fault`, `frl spin`) before its silence means anything.
8. **The image's own watchdog killed the next flash load.** **Our mistake.** A report-and-reset watchdog added for diagnosis stayed armed while the debugger's flash loader ran on the chip, timed out, reset the chip mid-write, and left the flash half programmed. The watchdog is gone from the image; the fault handler is enough.
9. **The debugger reaches this board only in the first seconds after a power-on boot, or after it reset the chip itself.** Cause: **Unknown (suspected: the debug power domain drops once the CPU idles unless a debugger connection holds it)**. Observed all day on two images, one that sleeps and one that only idles: connects fail with `Operation was aborted on 'Cortex_M33_0'` (CCStudio) or `-242 A router subpath could not be accessed` (DSLite), while the board answers on its console. Every connect that worked was within about 10 s of a USB replug, or in a run right after a debugger-driven reset. Recovery that works: start `tools/ccs_load_retry.py` (connect loop, load only, no backup) and then replug the board; the first connect inside the window loads the image in about 15 s. A 60 s backup before the load misses the window. The image also sets `Power.enablePolicy = false` so the CPU never idles. That did not change the rule: a connect landed 20 s after a debugger-driven load, and another run got the same -242 against it later while it sat parked after its radio was taken away. Only the loop-then-power-cycle recipe has worked every time.

## What it shows

The Thread border routers TI documents are Linux plus a TI RCP. This is the host side on a TI microcontroller: OpenThread's core plus its own spinel libraries ran on `ot-ti`'s platform with two new source files, and the first image that linked talked to the RCP on its first load. The CC1354P10 stands in for the CC35X1 Wi-Fi chip until that board arrives; the same two files move with it, because they touch only a UART and a timer.
