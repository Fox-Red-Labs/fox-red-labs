# How do I get a TI LP-CC1354P10-1 into Home Assistant as a sleepy Zigbee temperature sensor?

Build TI's `zed_temperaturesensor` example with [one patch](sleepy_sensor.patch) and `./build.sh`, load it, and open joining in Home Assistant's Zigbee integration (ZHA): on 2026-10-07 (1 run) the board joined the house network 6.0 s after boot and Home Assistant showed "Fox Red Labs Temp Sensor" with the chip temperature (22 °C, shown as 71.6 °F) 162 s after boot. As TI ships it, the example joins only from a button or its serial menu, reports a made-up temperature, and answers every attribute read from Home Assistant with an error, so the hub shows `unk_manufacturer unk_model` and no value for up to 15 minutes.

Terms: ZHA, ZBT-2, coordinator, router, end device, poll interval, node descriptor, trust center, NV, factory new, SysConfig and CCStudio are in [docs/vocab.md](../../docs/vocab.md). Errors and fixes: [../FAQ.md](../FAQ.md). Board notes: [../README.md](../README.md). A **sleepy end device** turns its receiver off between polls; its parent router holds messages for it until it asks. The **network key** is the secret the network's trust center (the device that hands out keys, here the coordinator) sends a new device after it associates. 1 kB = 1,024 bytes.

Time to repeat, once the tools in [mac-setup/README.md](../../mac-setup/README.md) are installed: about 21 s to build, 15 s to load, 6 s to join and about 3 minutes until Home Assistant shows the entity.

Checked 2026-10-07 on an Intel MacBook (Core i7-8750H), macOS 15.7.9: the F2 SDK (SimpleLink CC13xx CC26xx SDK 8.33.00.16, with Z-Stack and TI-RTOS 7), CCStudio 21.0.1 headless with its SysConfig 1.28.1 and TI Arm Clang 4.0.4.LTS, Home Assistant 2026.9.2 with ZHA and a Nabu Casa Home Assistant Connect ZBT-2 coordinator on a separate host.

Contents: [expected and measured](#what-was-expected-and-what-was-measured), [what the patch changes](#what-the-patch-changes), [build, load, join](#build-load-and-join), [getting the debugger back in](#before-you-load-it-getting-the-debugger-back-in), [traps](#traps), [what was not confirmed](#what-was-not-confirmed), [files](#files).

## What was expected, and what was measured

The expectations were written before the first build, from the CC2340R5 Zigbee run in [../../cc2340r5/zigbee/README.md](../../cc2340r5/zigbee/README.md).

| Step | Expected | Measured |
|---|---|---|
| Build from the terminal | under a minute | 21 s for `./build.sh` (copy, patch, CCStudio import and build), first try |
| Flash and RAM | about 200 kB flash, 45 to 55 kB RAM | 179.4 kB flash, 45.5 kB RAM of 256 kB of SRAM (the linker's RAM region); TI quotes 296 kB of RAM for the whole chip. TI's example as shipped, same build: 178.9 kB and 45.4 kB. The patch costs 508 B of flash and 152 B of RAM. A further 10 kB of flash is reserved for the stack's non-volatile storage (NV). |
| Boot to joined | within 60 s, most likely 10 to 40 s | **6.0 s** on the clean run (1 run). The board's serial clock shows "In the network" 5.6 s after boot, the coordinator logged the join 5.9 s after boot, and the results file rounds to 6.0 s (row `radio_join` in `data/results/cc1354p10.csv`). Earlier tries were rougher: 4 of 6 associations lost the network key ([trap 5](#traps)). |
| Boot to entity in Home Assistant | not estimated | 162 s. ZHA's setup questions to the sleeping board took 156 s ([trap 6](#traps)). |
| What Home Assistant shows | "Fox Red Labs" "Temp Sensor", end device, one temperature entity, identify button | Same: `EndDevice`, power source "Battery or Unknown", node descriptor MAC capability `0x80`: bit 3, "receiver on when idle", is clear, so the receiver is off between polls, temperature in °F because the house is set to °F, identify button. |
| Temperature value | chip temperature in whole degrees, a few above room | 22 °C on the serial line; Home Assistant read `2200` (22.00 °C) and shows 71.6 °F. Room temperature was not measured. |
| Report interval | ZHA replaces this repo's 30 s default with min 30 s, max 900 s, change about 0.5 °C | **Confirmed.** ZHA set min 30, max 900, change 50 (0.50 °C). At a steady 22 °C the first report arrived 899.9 s after ZHA set it. |
| Poll interval | 3,000 ms, TI's SysConfig default | On the final run the stack reported 3,000 ms in 78 of the 82 status lines (2 at 0 ms before joining, 1 at 440 ms just after joining, 1 at 100 ms while the hub was talking to it). From outside: 20 of 20 reads from Home Assistant, median 1,574 ms, spread 171 to 2,799 ms, which is what a 3 s poll predicts. |
| Sleep current | not measurable here | Not measured: no current meter was used. See [trap 8](#traps). |

## What the patch changes

[`sleepy_sensor.patch`](sleepy_sensor.patch) is a diff against TI's example folder plus the three app files it links from the SDK (`source/ti/zstack/apps/temperaturesensor/`). Everything else is TI's code as shipped.

In `tirtos7/zed_temperaturesensor.syscfg`, edited as text:

```text
var Temperature = scripting.addModule("/ti/drivers/Temperature");
```

```text
zstack.rf.primaryChannels = [11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26];
```

The first adds TI's Temperature driver. The second searches all 16 channels; TI's default is channel 11 only, and this house network is on channel 20. You can list just your network's channel instead ([trap 9](#traps) says why that is better). The poll period stays at TI's SysConfig default, 3,000 ms.

In the C files:

1. **Real temperature.** `Temperature_init()` at start, then `Temperature_getTemperature()` every 10 s, times 100 into the Measured Value attribute (the Zigbee cluster counts in hundredths of a degree). A change goes to the reporting engine with `Zstackapi_bdbRepChangedAttrValueReq`. TI's value started at a fixed 17.00 °C and changed only from the serial menu. The attribute's min and max are now -40.00 and 125.00 °C (TI: 17 and 27).
2. **Names.** Manufacturer "Fox Red Labs" (TI: "TexasInstruments") and a new Model Identifier attribute, "Temp Sensor" (TI's example has none). SysConfig does not expose these for this example; they are in `zcl_sampletemperaturesensor_data.c`.
3. **Attribute reads work.** One line registers a read callback, `zcl_registerReadWriteCB(...)`, the same way TI's light example does ([trap 3](#traps)).
4. **Join without a button.** If there is no saved network at boot, start joining after 1 s, and retry every 15 s after a failure. TI's code resumes a saved network only and waits for the left button (BTN-1) or the serial menu.
5. **Report defaults.** Max interval 30 s (TI: 10) and change 1 °C (TI: 3 °C). ZHA replaces both when it sets up reporting.
6. **Status line.** The serial screen's "APP Info" line shows the temperature, a sample count, the join retries and the poll period the stack reports.

## Build, load, and join

You need: the board, a Mac with the tools in [mac-setup/README.md](../../mac-setup/README.md) (CCStudio 21.0.1 and the F2 SDK), and a Zigbee network in Home Assistant with the board in range of the coordinator or of a Zigbee router. Here the coordinator was out of the board's range, and every join went through a router (a smart plug).

Run these from the repo root. The paths are where TI's installers put the tools on this Mac.

```bash
cc1354p10/zigbee-sleepy-sensor/build.sh
```

It copies TI's example and the three app files out of the SDK, applies the patch, imports the project with CCStudio's headless command line into a fresh workspace and builds it, then copies the image to `out/frl_sleepy_sensor.out` and the map beside it. TI ships no makefile for Zigbee examples on this board ([../README.md](../README.md), "Building Zigbee and DMM examples from a terminal").

### Before you load it: getting the debugger back in

Read this first. On this board the debugger connects only in the first seconds after a power-on boot, whatever image runs (a never-sleeping image locked it out the same way later that day), so once this image is loaded the debugger cannot attach to the CC1354P10. **Unknown (suspected: Chip limit).** Nothing from software reached it once it was in that state: the serial menu's reset, hundreds of connects, BTN-1 held through a reset. What worked, on 2026-10-07: start a loop of `ccs_load_run.py` loads first, then unplug and replug the USB cable. The loop below tries up to 60 times and stops at the first load that gets in; put the image you want back on the board in place of the sleepy sensor's:

```text
for i in $(seq 1 60); do /Applications/ti/ccs2101/ccs/scripting/python/run.sh cc1354p10/tools/ccs_load_run.py cc1354p10/zigbee-sleepy-sensor/out/frl_sleepy_sensor.out && break; done
```

The load went in on the 21st attempt, about 10 s after the replug, in the seconds the chip stays awake at boot before its first sleep. A replug with no loop already running missed that window twice. Keep this recipe, or a non-sleeping image and its backup, before leaving this image on a board you share.

### Join

Open joining in Home Assistant first. In the web page: **Settings**, **Devices & services**, **Zigbee Home Automation**, **Add device**. Joining stays open for about 4 minutes. (Or **Developer tools**, **Actions**, `zha.permit` with `duration: 254`.) If the board is out of the coordinator's range, open joining on the router it can reach: `zha.permit` with `duration: 254` and that router's `ieee` address ([trap 5](#traps)).

Then load the image and start it:

```bash
/Applications/ti/ccs2101/ccs/scripting/python/run.sh cc1354p10/tools/ccs_load_run.py cc1354p10/zigbee-sleepy-sensor/out/frl_sleepy_sensor.out
```

`run.sh` is CCStudio's own scripting Python; if CCStudio is not installed at `/Applications/ti/ccs2101`, change that part of the path. [`cc1354p10/tools/ccs_load_run.py`](../tools/ccs_load_run.py) is a short script in this repo that connects to the board, loads the program and starts it. This board needs a CCStudio scripting connection to start a freshly loaded program; a plain UniFlash flash does not start until you unplug and replug the USB cable ([../README.md](../README.md)). The load took about 15 s.

Watch the serial screen at 115200 baud. Replace `LS45013A1` with your probe's serial: `ls /dev/cu.usbmodem*` lists two ports for the probe, and the board's UART is the one ending in 1.

```text
screen /dev/cu.usbmodemLS45013A1 115200
```

Or log it with timestamps:

```bash
python3 cc1354p10/zigbee-sleepy-sensor/cui_log.py capture /dev/cu.usbmodemLS45013A1 serial.raw 300
```

```bash
python3 cc1354p10/zigbee-sleepy-sensor/cui_log.py decode serial.raw
```

Success looks like `[State] In the network` on the "ZDO Info" line and `[PAN ID] 0x.... [Channel] 20` on "NWK Info". The device then appears in Home Assistant a few minutes later as "Fox Red Labs Temp Sensor". Prove it from Home Assistant's device list, not from the board's screen.

To start over: remove the device in Home Assistant and wait until it is gone, then use the serial menu's **RESET TO FN** (reset to factory new, which forgets the saved network; right arrow to it, Enter) and load the image again ([trap 7](#traps)).

## Traps

1. **TI's example joins only from a button or the serial menu.** **TI example default.** At boot it resumes a saved network and nothing else. Fixed by change 4.
2. **The temperature is made up, and the names are TI's.** **TI example default.** The value starts at 17.00 °C and moves 1 °C per menu key press. Manufacturer "TexasInstruments", no model. Fixed by changes 1 and 2.
3. **Every attribute read answers `0xC1` (`SOFTWARE_FAILURE`).** **Tool or docs.** In this Z-Stack, `zclProcessInReadCmd` in `source/ti/zstack/stack/zcl/zcl.c` sends every read through the endpoint's read callback, and `zclReadAttrDataUsingCB` returns `ZCL_STATUS_SOFTWARE_FAILURE` when none is registered. The comment on `zcl_registerReadWriteCB` says the callback is needed only when an attribute's data pointer is NULL. TI's light example registers one; the temperature sensor does not. What Home Assistant does with it: the device is named `unk_manufacturer unk_model`, and the temperature stays `unknown` until the first report, which at a steady temperature is 15 minutes later. Home Assistant's log shows `failure: [{0: <Status.SOFTWARE_FAILURE: 193>}]`. Fixed by change 3. The cause was read from the source; after the patch, reads succeeded (20 of 20). The one-line change was not tested on its own: the same build also changed the names and the temperature.
4. **Home Assistant decides the report interval, not the device.** **Third-party behavior.** ZHA binds the temperature cluster and sets min 30 s, max 900 s, change 0.50 °C, replacing the device's own defaults. A sensor at a steady temperature reports every 15 minutes. Measured: 899.9 s from setup to the first report.
5. **The network key arrived too late in 4 of 6 associations.** **Unknown (suspected: Radio environment).** The board can only reach the house network through a router (a smart plug); with joining open on the coordinator alone it could not join the house network in 3 minutes (it found only another network, trap 9). Each time, the coordinator's log shows the join and its key answer (`trustCenterJoinHandler ... USE_PRECONFIGURED_KEY`). The two successes got the key in 0.2 and 0.7 s. Why the key was late in the other four was not found; a weak radio link through the router is suspected. Not checked: whether the key reached the plug and waited there, and how another parent would behave. How to avoid it: if the board is out of the coordinator's range, open joining on the router it can reach (`zha.permit` with that router's `ieee`), and keep the board near that router for the first join.
   - **The 3 s wait is TI's setting.** **TI example default.** TI's stack waits 3 s for the key (`BDB_DEFAULT_DEVICE_UNAUTH_TIMEOUT 3000` in `bdb_interface.h`, a plain `#define` that a project cannot override), then gives up and tries again.
6. **Home Assistant took 156 s to finish setting the device up.** **Unknown (suspected: Radio environment).** About half of ZHA's first requests to the sleeping board got no answer; zigpy sent each again about 28 s later, and every resend was answered in 1.8 to 3.0 s, within one poll period. The cause of the lost first tries was not found.
7. **A reset from the serial menu leaves the board silent.** **Unknown.** After **RESET TO FN** (reset to factory new) the board stopped printing and did not rejoin; loading the image again with `ccs_load_run.py` brought it back, factory new. The same happens on this board whenever a program loaded by the debugger restarts itself (for example, a Thread script's `factoryreset` rebooted the chip and the loaded program did not come back). Not checked: whether a USB replug alone would bring it back.
8. **The serial menu may keep the chip out of its deepest sleep.** **Unknown (suspected: TI example default).** The radio sleeps between polls (the node descriptor says so), but TI's UART driver sets a standby constraint while it listens (`UART2_rxEnable` in `source/ti/drivers/UART2.c`), and the menu always listens. TI's Zigbee power page says to define `CUI_DISABLE` and remove `BOARD_DISPLAY_USE_UART` for low power. Not checked: the current with and without `CUI_DISABLE`. That build would have no serial log; it was not made or measured here.
9. **Other open networks nearby get tried first.** **Radio environment.** Searching all 16 channels, the board also associated with two other networks: PAN `0xe99d` on channel 11 (the network this board formed as a coordinator on 2026-10-06; suspected: still kept by the CC2340R5 that joined it then, since the parent address seen here, `0xc8dd`, was that board's address in it) and PAN `0xdc20` on channel 20 (owner not known). Each costs a 3 s key wait. TI's stack joins the first open network it finds. List only your network's channel to avoid most of this.
10. **Removing the device in Home Assistant did not reach the board.** **Unknown.** The coordinator logged `DEVICE_LEFT` from the parent 2 s after the removal, but the board's screen still said "In the network". Use **RESET TO FN** as well.
11. **TI's README for this example lists other LaunchPad development kits.** **Tool or docs.** It names the CC26x2R1, CC1352P, CC1352R1, CC2652RB and CC2652RSIP boards, not this one. The example itself is in the SDK's `LP_EM_CC1354P10_1` folder and works.
12. **CCStudio swaps the compiler.** **Tool or docs.** The project asks for TI Arm Clang 3.2.2; the headless import says `Compiler version 'TICLANG_3.2.2' is not currently installed - defaulting to 'TICLANG_4.0.4.LTS'` and builds with that. The build has one warning, from TI's own linker file (`redefinition of macro 'NVOCMP_NVPAGES'`).
13. **The entity keeps its old name after a re-pair.** **Third-party behavior.** After removing and re-adding the device, Home Assistant reused the entity id `sensor.unk_manufacturer_unk_model` while the device name became "Fox Red Labs Temp Sensor". Rename the entity in Home Assistant if it matters.

## What was not confirmed

- Current and battery life: no current meter was used, and no `CUI_DISABLE` build was made.
- Join time is one clean run. Three runs are needed before quoting a median.
- That the temperature tracks the room: only 22 °C was seen; the chip was not warmed or cooled, and no thermometer was beside it.
- A report on the final build. The first build's report came 899.9 s after ZHA set reporting. On the final build, Home Assistant's whole Zigbee integration went down for a reason outside this run (its radio disappeared from the hub's USB) before the next report; the next report was due about 6 minutes after the outage began. Every Zigbee entity in the house, this one included, turned `unavailable`. The board itself stayed "In the network", polling every 3 s.
- Packet loss over an hour, the board's own signal strength to its parent (ZHA shows only the last hop into the coordinator), and range.
- Over-the-air update of this image.

What to do next: a `CUI_DISABLE` build for a current measurement with a meter, and two more clean joins for a median.

## Files

- [`sleepy_sensor.patch`](sleepy_sensor.patch): all changes against TI's example. [`build.sh`](build.sh): rebuilds from the SDK. [`cui_log.py`](cui_log.py): capture and decode the serial screen.
- Results: rows `radio_join`, `radio_cmd_latency`, `radio_cmd_success`, `radio_flash_used`, `radio_ram_left` and `stack_support` in [../../data/results/cc1354p10.csv](../../data/results/cc1354p10.csv).
- Build output (`out/`) is not committed; `build.sh` makes it.
