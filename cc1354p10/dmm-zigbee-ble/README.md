# Can the TI LP-CC1354P10-1 run a Zigbee light and a Bluetooth light at the same time?

Partly shown. TI's `dmm_zr_light_remote_display_app` joined a Home Assistant Zigbee network about 5 to 6 seconds after the join command, and while it stayed on that network a Mac switched the light over Bluetooth LE 4 times out of 4 (about 60 ms per write and read-back). Home Assistant switching the light was not tested: it still held an old record for this chip (see "What did not work").

Terms: [vocab](../../docs/vocab.md) (DMM, ZHA, GATT). Errors and fixes, including the three on this page: [CC1354P10 FAQ](../FAQ.md). 1 kB = 1,024 bytes.

Checked 2026-10-08 on an Intel Mac, macOS 15.7, with SimpleLink CC13xx/CC26xx SDK 8.33.00.16, CCStudio 21.0.1 (headless), SysConfig 1.28.1 and TI Arm Clang 4.0.4.LTS. One board, one run.

## What it is

DMM (Dynamic Multi-protocol Manager) is TI's scheduler that shares one radio between two stacks. This example runs a Zigbee light and a Bluetooth LE peripheral with a "light" service in one image. A phone, or a Mac, can switch the light over Bluetooth while the Zigbee network can switch it too. On Zigbee the light is a router: a mains-powered device that also passes messages on for other devices. Words used below: a PAN ID is the number that names one Zigbee network; an endpoint is a numbered slot on a device that holds one function, and TI's light uses endpoint 8.

## Size

From the linker's memory summary of this build:

| | Used | Of | Left |
|---|---|---|---|
| Flash | 459 kB | 1,014 kB (excluding 10 kB kept for settings) | 555 kB |
| RAM | 33.8 kB | 256 kB | 222 kB |

`tiarmsize` reports 43.6 kB of `.bss`, about 10 kB more than the linker's RAM total. The extra 10 kB is not identified. Use the linker figure.

## Build it from a terminal

TI ships no makefile for the DMM examples, only a CCStudio project. CCStudio's headless command line builds it. The copy of the project in this folder changes two things in SysConfig: the Bluetooth name ("Fox Red DMM") and the Zigbee channel list (all 16 channels instead of 11 only), so it can join a home network on any channel.

```bash
/Applications/ti/ccs2101/ccs/ccs-server.app/Contents/MacOS/ccs-server-cli.sh -noSplash -workspace ~/dmm-ws -application projectImport -ccs.location tirtos7/ticlang/dmm_zr_light_remote_display_app_LP_EM_CC1354P10_1_tirtos7_ticlang.projectspec
```

```bash
/Applications/ti/ccs2101/ccs/ccs-server.app/Contents/MacOS/ccs-server-cli.sh -noSplash -workspace ~/dmm-ws -application projectBuild -ccs.projects dmm_zr_light_remote_display_app_LP_EM_CC1354P10_1_tirtos7_ticlang -ccs.configuration Release
```

Run both from this folder. Import and build took 35 s here and the workspace used 21 MB of disk. Then check that `Release/*.out` exists. **Tool or docs.** The last line, `CCS headless build complete - 0 out of 1 projects have errors`, is the same when SysConfig fails and nothing is built ([../README.md](../README.md), "Building Zigbee and DMM examples from a terminal"; the FAQ has the entry).

## Load it

Load the `.out` with CCStudio or UniFlash. This board's debugger connects only in the first seconds after power is applied, so unplug and replug the USB cable, then start the load at once (to do it hands-free, switch the probe's USB hub port off for 6 s with `uhubctl`, on, then load at once).

## Join the Zigbee network

The image does not join on its own. Use the menu on the board's serial port (TI's CUI, the Common User Interface):

1. In Home Assistant, open joining: **Settings**, **Devices & services**, **Zigbee Home Automation**, **Add device**.
2. Open the first serial port of the board (`/dev/cu.usbmodem<serial>1`) at 115200 baud in a terminal program.
3. Press Esc, right arrow (the menu shows `TI Sample Light`), Enter, right arrow (`<   COMMISSION   >`), Enter.

Success looks like `[State] In the network` on the "ZDO Info" line and a real PAN ID and channel on "NWK Info". The board showed `[PAN ID] 0xde8d [Channel] 20`.

To leave the network later: Esc, right arrow, Enter, left arrow twice (`<  RESET TO FN   >`), Enter.

## Switch it over Bluetooth

The board advertises "Fox Red DMM" (TI's default name is "DMM Zigbee Light RD"). Any Bluetooth LE app works: connect, open the service `F00011A0-0451-4000-B000-000000000000`, and write `01` or `00` to the characteristic `F00011A1-...`. The "APP Info" line on the serial port changes to `[Light State] On` or `Off`. The Mac run wrote 1, 0, 1, 0; each read back the same value in 57 to 61 ms, and the board's line went Off, On, Off, On, Off.

## What did not work

- **Home Assistant did not get a light entity.** **Unknown (suspected: Third-party behavior).** This chip had joined the same Home Assistant before with another image (a temperature sensor). After the new join, Home Assistant still showed the old record for this address ("Temp Sensor", no light). Suspected: it did not read the device's endpoints again on a rejoin; not checked. A toggle sent through `zha.issue_zigbee_cluster_command` to endpoint 8 failed with `HTTP Error 400: Bad Request`. If you see this, remove the old device in Home Assistant, then commission again so Home Assistant reads the new endpoints. The old record was not removed, so a toggle from Home Assistant is not tested.
- **macOS showed an old name.** **Third-party behavior.** The Mac listed the board as "Fox Red Lab CC1354", the name an earlier image used. macOS remembers names by address. Search by the advertised name or by the service, not by the name in a list.

## What to do next

Not done: toggle the light from Home Assistant with a fresh device record, and time 20 toggles from each side, then both at once. Next: [CC1354P10 FAQ](../FAQ.md).
