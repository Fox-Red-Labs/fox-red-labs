# How do I get a TI LP-EM-CC2340R5 into Home Assistant over Zigbee?

Terms: ZHA, ZBT-2, coordinator, router, end device, NVS, LQI, RSSI and tilogger are defined in [docs/vocab.md](../../docs/vocab.md). Errors and fixes: [FAQ.md](../FAQ.md).

Build TI's `onoff_light` example from the SimpleLink Low Power F3 SDK 9.21.00.36 with two lines changed in its `.syscfg` file, flash it, and open joining in Home Assistant's Zigbee integration (ZHA): on 2026-10-05 the board joined 37 seconds after the flash finished, and 20 of 20 on/off commands from Home Assistant landed. The two lines make the board a router and let it search all 16 channels; they are in [the next section](#the-fix-two-lines-in-the-syscfg).

Latency, measured once each: median 95 ms at the first pairing; about 5 s after a re-pairing about two hours later, cause **Unknown (suspected: Radio environment)** ([what was measured](#what-was-measured-and-what-was-not)).

A rerun on 2026-10-16 of the same image did not join (see "Rerun of 2026-10-16" below). Treat the join as seen once.

Contents: [the two-line fix](#the-fix-two-lines-in-the-syscfg), [build and flash](#build-and-flash), [times](#times), [gotchas](#gotchas), [what was measured](#what-was-measured-and-what-was-not), [what to do next](#what-to-do-next), [also tried: Bluetooth](#also-tried-bluetooth), [files](#files).

## The fix: two lines in the `.syscfg`

TI's example searches channel 11 only and starts its own network as a coordinator. **TI example default.** A home network is usually on another channel (this one is on 20), so the board never finds it, and nothing says so: the device is simply absent. In `onoff_light/on_off_light.syscfg`, set:

```text
zigbee.deviceType                    = ["zr"];
```

```text
zigbee.primaryChannels               = [11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26];
```

The first line makes the board a router (it was `["zc"]`, a coordinator that forms its own network). The second searches all 16 channels (it was unset, which means channel 11 only: `DEFAULT_CHANLIST` is `0x00000800`). You can also list just your network's channel. The generated `ti_zigbee_config.h` shows the channel mask. Everything else is TI's example as shipped.

Setup on the other end: a Nabu Casa Home Assistant Connect ZBT-2 (a USB Zigbee and Thread radio) plugged into a separate host, running Home Assistant in Docker with ZHA. The build and flash ran on an Intel MacBook, macOS 15.7.9, with TI Arm Clang 5.1.1, SysConfig 1.26.3 and UniFlash 9.6.0.

The board carries the CC2340R52 chip, 36 kB of RAM; the CC2340R53 is a 64 kB version on a separate board, LP-EM-CC2340R53. 1 kB = 1,024 bytes.

## Build and flash

Run these from the board folder, `cc2340r5/`, where the connection file is.

```bash
cd zigbee/onoff_light/freertos/ticlang
```

```bash
make -j4 SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR=/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS \
         SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh \
         TICLANG_ARMCOMPILER=/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS
```

```bash
cd ../../../..
```

```bash
/Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c CC2340R5.ccxml -e -f -u zigbee/onoff_light/freertos/ticlang/onoff_light.hex
```

The paths are where TI's installers put the tools on a Mac; change them if yours differ. `CC2340R5.ccxml` is the connection file from [../README.md](../README.md). Open the Zigbee network for joining in Home Assistant first (Settings, Devices, Add device, or the `zha.permit` service with a duration of 180). The device joins on its own within 37 to 58 seconds of boot in the tries.

## Times

Every step ran from the terminal; the only hands-on check was looking at the LED.

| Step | Where | Waiting | Notes |
|---|---|--:|---|
| Copy the example, read its README, find the role setting | terminal | about 3 min | The example is a coordinator by default. Changed to router in the `.syscfg`. |
| Build (SysConfig plus TI Arm Clang, `make -j4`) | terminal | 30 s | |
| Flash | terminal | 14 s | UniFlash command line, same connection file as the blink run. |
| First join attempt | radio | about 6 min | Device never appeared. The 6 minutes went to finding out why: the channel-11 default above. |
| Rebuild with all channels, flash | terminal | 31 s build, 15 s flash | 275.7 kB of flash and 35.2 kB of the 36 kB of RAM, from the linker map of the same source rebuilt later that evening. |
| Join | radio | 37 s | From the flash finishing to the device listed in ZHA. |
| 20 toggles from Home Assistant | network | 24 s | All 20 landed. Median 95 ms, min 73, max 292. |

The waiting times in the table add up to about 11.5 minutes, from the first command to the LED switching from Home Assistant. Without the 6-minute channel detour, about 5.5.

## Gotchas

1. **TI's Zigbee examples listen on channel 11 only.** **TI example default.** The fix is the `primaryChannels` line above. Prove the join from Home Assistant's device list, not from the board's log.
2. **The serial log is binary, and TI's decoder `tilogger` fails on a Mac as shipped (`unpack requires a buffer of 8 bytes`).** **Tool or docs.** The one-line fix to the decoder is in [FAQ.md](../FAQ.md).
3. **The device shows up as "unk_manufacturer unk_model".** **TI example default.** The example does not fill the Basic cluster's manufacturer and model strings. Home Assistant lists it under those names and exposes it as a switch, not a light. Harmless, but a newcomer will wonder if it is theirs. The IEEE address starts with `00:12:4b`, TI's prefix.
4. **To start over, remove the device in Home Assistant first, and wait.** **Our mistake**, with **Third-party behavior**. The example keeps its Zigbee network settings in the chip's non-volatile storage (NVS, the `NVRAM` region in its linker map). Remove the device in Home Assistant, wait until it is gone from the device list, then reflash or use the example's reset button. Re-pairing before Home Assistant had finished removing the device left a stale record and `Failed to send request: device did not respond`. Note on erasing: `-e` in the flash command above is `--verbose` (progress messages), not erase. UniFlash's settings file for this chip family sets a chip erase during every program load by default, and this repo's connection file does not change it. Whether a reflash alone made the board forget its network was not checked (**Unknown**).
5. **RAM is nearly full.** **Chip limit.** 799 bytes free with logging on. Turn `loggingEnabled` off in SysConfig before adding anything.
6. **Stale bytes on the serial port.** **Unknown.** After a flash, the first read of the serial port showed lines from the previous program. The cause was not checked. Flush the port first (`termios.tcflush`) or read twice, and wait for the new program's own output.

## What was measured, and what was not

- Measured once: join time; 20 switch commands with success count and latency from this Mac; link quality as reported by the coordinator at the first pairing, LQI 60 and RSSI -85 dBm. (LQI is the coordinator's 0 to 255 link-quality score, higher is better. RSSI is received signal strength in dBm, closer to 0 is stronger; -85 dBm is weak.) The distance between the desk and the coordinator was not measured.
- The latency includes the network hop between the Mac and the Home Assistant host (13 to 43 ms for a plain API call), ZHA, the coordinator, the air, the board, and the response back. The Home Assistant call returns after the board acknowledges the command, so the air round trip is inside the call time.
- Re-measured about two hours later: after other images were flashed and the board was removed and re-paired, 30 of 30 commands landed with a median of about 5 s (3.5 to 7.0 s). Link quality after the re-pairing: the coordinator's readings of this board that evening, none during a transfer: LQI 36 to 48, RSSI -88 to -91 dBm (observed 2026-10-05; not saved). None was taken during the over-the-air update tests. **Unknown (suspected: Radio environment).** The route through other Zigbee devices was not checked. Measure more than once before quoting a latency.
- End device, measured once: TI's on/off switch example, built as an end device, rejoined the network 15 s after flashing. Its device type and sleep behavior were not confirmed; TI's OTA client example, also an end device, joined and was listed as an end device ([../matrix/README.md](../matrix/README.md)).
- Not measured: current, range, packet loss over hours, behavior as a sleepy end device, and anything about Thread. The board was a router here because routers stay awake and are the simplest thing to command.

## Keep it running: automation, restart, update, recovery

What this board controls is its own red LED, through a switch entity in Home Assistant. It is not a mains light fitting. Each line says whether it has been run.

| Part of the job | How | Status |
|---|---|---|
| Parts | The LP-EM-CC2340R5 board, the LP-XDS110ET probe and a Home Assistant Connect ZBT-2 coordinator, plus a computer already running Home Assistant. The probe and the coordinator are reused on later builds | Parts read on the sellers' pages |
| The entity | ZHA lists the board as `unk_manufacturer unk_model` with one switch entity (gotcha 3); its entity id is on its device page | Run 2026-10-05 |
| One automation | [home-assistant/automation-sunset.yaml](home-assistant/automation-sunset.yaml): the switch on at sunset and off at 23:00, as two automations. Paste them into `automations.yaml` with your entity id, or rebuild it in the automation editor | **Not run** |
| Restart: power loss | Unplug the probe's USB cable for 30 s, plug it back in, then switch the entity from Home Assistant. Expected: the board keeps its network settings in NVS (gotcha 4) and answers without being paired again. Pass: 20 of 20 commands land within 2 minutes of power returning | **Not run**: blocked by the join below |
| Restart: Home Assistant | Restart Home Assistant, then switch the entity. Pass: 20 of 20 land | **Not run**: blocked by the join below |
| Update | Over the air from Home Assistant: stalled at about 1 percent in four tries, cause unknown ([experiment](../../docs/experiments/2026-10-05-cc2340r5-zigbee-ota-home-assistant.md)). The update that works is a reflash over the probe (Build and flash above). UniFlash's default load erases the whole chip first ([oad/README.md](../oad/README.md)), NVS included, so after a reflash remove the device in Home Assistant and pair it again (gotcha 4) | Reflash run; keeping the pairing across a reflash not run |
| Recovery | Back up the shipped flash before the first write and put it back with the commands in [AGENTS.md](../AGENTS.md) (Backup, Restore). For copies the browser bench saved: `cc2340r5/restore-from-page.sh <folder>` | The page-copy route passed on 2026-10-16 (see below) |

**Rerun of 2026-10-16: the light did not join.** This README's image (`onoff_light.hex`, rebuilt byte for byte from the committed source) was flashed or reset three times with ZHA's joining open for about 4 minutes each, the device removed from ZHA first once; it never appeared in ZHA's device list. As a control, the same board with the Zigbee alarm image (`../zigbee-alarm/`, an end device) joined in 31 s and then 7 s on the same network, whose coordinator and three routers were up. The light's own log (tilogger, gotcha 2) is inconsistent between tries. In one, "Device (1) STARTED OK" (1 is a router) and "Successful steering" 6.2 s after a reset. In another, with a debug copy that would print the network it joined, steering was cancelled at 6.3 s (ZBOSS signal 55) and the stack reported no active links left at 16 s (signal 24), with no join to print. Home Assistant's network is on channel 20, PAN ID 0xDE8D. Cause not found; which network, if any, the light joins is not established. Until it is found, the automation and restart checks above cannot run. Next: find what cancels steering (the example's start-up and button handling), and repeat with the coordinator's own log open.

**Putting back a copy the browser bench saved, 2026-10-16: passed.** With the alarm image joined, the bench page wrote its Bluetooth light and saved the alarm image as two `.bin` files first. `cc2340r5/restore-from-page.sh` put them back in 30 s. Main flash and CCFG read back identical, except 375 bytes inside the NVS area (0x7F000 to 0x7FFFF) that the alarm wrote as it started. Home Assistant then read the device's IAS Zone state over the air, with the same network address and no re-pairing. The board was then returned to the Bluetooth light it held before, read back identical.

## What to do next

- Keep logging off if you add application code (gotcha 5).
- To test Bluetooth, Thread, or an over-the-air update on the same board, see [../matrix/README.md](../matrix/README.md).

## Also tried: Bluetooth

TI's Bluetooth examples on this board, with flash-to-air, connection and echo times from a Mac: [../matrix/README.md](../matrix/README.md) and the CSV rows in `data/results/cc2340r5.csv`.

## Files

- `onoff_light/`: TI's example with the two-line change. Build outputs are not committed.
- `home-assistant/automation-sunset.yaml`: the sunset automation described above.
- The decoded log of a join and the 20-toggle timings are not in the repository; the numbers are in the tables above and in `data/results/cc2340r5.csv`. The flashes ran without verify (`-v`), so they say nothing about the mismatch at 0x4E020750 seen on other images.
