<!-- experiment
id: E189
board: LP-EM-CC2340R5
date: 2026-10-12
status: waiting
reached: run
line: A hand-written HID over GATT service on TI's basic_ble builds to 186.5 kB of flash and 30.9 kB of RAM. It advertises 11.9 s after the flash starts, and macOS calls it a "Compatible LE HID" and asks to pair with Just Works. The pairing was then accepted on the Mac, giving an encrypted link. The link dropped 9 ms later with reason 8 and did not return, so play and pause in Music is not seen.
stop: none
-->
# Can the LP-EM-CC2340R5 be a Bluetooth media remote for a Mac with no app, using TI's Bluetooth stack?

**Short answer.** Close, not finished. With a HID service written by hand on TI's `basic_ble` example, the board advertises as a HID device and macOS treats it as one: it reads the board's vendor id, sets a 15 ms connection interval for it and starts Just Works pairing. The first two pairing boxes timed out unanswered; the third was accepted and the board logged an encrypted link, which ended 9 ms later with reason 8 (supervision timeout) and did not reconnect in the two minutes the logger had left. Whether the buttons pause and skip in Music depends on a link that stays up.

Terms: [vocab](../vocab.md) (HID over GATT, report map, Just Works, bonding are explained in the port's README). The port, with how to build, flash and pair it: [`ports/ble-hid-remote/`](../../ports/ble-hid-remote/README.md). 1 kB = 1,024 bytes.

**Question.** Can the LP-EM-CC2340R5 be a media remote (left button play and pause, right button next track) for a Mac or iPhone over Bluetooth LE, with nothing installed on the Mac or phone, built with TI's own Bluetooth stack?

**Vendor position.** TI ships no HID example or profile for this chip. The SimpleLink Low Power F3 SDK 9.21.00.36's `source/ti/ble/profiles` holds battery, car access, continuous glucose monitoring, data stream, glucose, health thermometer, OAD, ranging and simple GATT; none is HID. The BLE5-Stack user guide's list of SysConfig profiles ("Available GATT service profiles in Sysconfig", `ble-stack-5.x/gatt-cc23xx.html` in the SDK, read 2026-10-12) names five: "Continuous Glucose Monitoring", "Glucose", "Health Thermometer", "Battery Service", "TI Simple GATT Service". The stack's headers do define every HID UUID (`gatt_profile_uuid.h`: `HID_SERV_UUID 0x1812`, `REPORT_MAP_UUID 0x2A4B` and the rest), and the user guide explains how to write a service by hand.

The host side: Apple's Accessory Design Guidelines (release R31, 2026-09-21) say "When implementing HID over Bluetooth Low Energy, the accessory shall advertise the HID Service in the Advertising Data" (58.13) and "The accessory should not request pairing until an ATT request is rejected using the Insufficient Authentication error code" (58.10). The Bluetooth SIG's HID over GATT Profile 1.0: "HID Devices shall bond and use LE Security Mode 1, Security Level 2 or 3".

**What was tried.** Board LP-EM-CC2340R5, probe <probe serial>. F3 SDK 9.21.00.36 LTS, SysConfig 1.26.3, TI Arm Clang 5.1.1.LTS, UniFlash 9.6.0 DSLite, Intel Mac on macOS 15.7.9, `bleak` 3.0.2.

- A new service file, [`ports/ble-hid-remote/app/hid_remote.c`](../../ports/ble-hid-remote/app/hid_remote.c) (315 lines, 234 code), in the attribute-table style of TI's `simple_gatt_profile.c`: HID Information (bcdHID 1.11, normally connectable), a 33-byte consumer-control Report Map (report ID 1: play/pause 0xCD, next 0xB5, previous 0xB6, volume up 0xE9, volume down 0xEA, three spare bits), HID Control Point, Protocol Mode, one input Report with its CCC and Report Reference (1, input). All HID values need an encrypted link. TI's Battery service and the example's Device Information service (with PnP ID) stay. Buttons: TI's Button driver, one notification per press and per release, handed to the Bluetooth task.
- [`ports/ble-hid-remote/basic_ble.patch`](../../ports/ble-hid-remote/basic_ble.patch): SysConfig name, bond manager (wait for the host, no input and no output so Just Works, MITM off, LE Secure Connections allowed, bonding on), advertising flags, appearance 0x03C0, UUIDs 0x1812 and 0x180F, preferred interval 15 to 30 ms; the button menu replaced by prints only; Simple GATT replaced by the HID start call.
- Host checks before the board: `host/report_map.py` decodes the map from the C source and checks each bit against the header (OK); `host/verify_build.py` decodes the advertisement and bond settings from SysConfig's output (all OK).
- `build.sh`, flash with DSLite, `bleak` scan and connect, the board's UART log, and the macOS Bluetooth daemon's log (`log show`, lines for this device only; the quoted lines are in the table below).

**How far it got.** Run: advertising, macOS identified it as a HID device, the pairing was accepted on the Mac and the link encrypted, then dropped at once.

| Step | Result |
|---|---|
| Configure, compile, link | Pass. 34.7 s clean build. 186.5 kB flash, 30.9 kB RAM, 5.1 kB RAM left (`basic_ble` as shipped: 187.4 kB flash) |
| Flash | Pass, 12.6 s |
| Boot | Pass: UART "FRL Remote: HID up, 13 attrs, map 33 B, buttons ok" |
| Run, advertising | Pass: first packet 11.9 s after DSLite started (0.7 s before it exited), 122 packets in 75 s, UUIDs 0x1812 and 0x180F |
| Mac recognises a HID device | Pass, from the Mac's log: "Device "FRL Remote" is Compatible LE HID", "LE HID connection complete for address: 50:F4:71:xx:xx:xx (VID:0x000D/PID:0x0000", "switching to 15.00 ms latency(0) appearanceValue(0x3c0)", then "Received "Just Works" pairing request" and "Displaying pairing request alert" |
| Pairing | Done on the third box (board UART "Pairing Status: Encrypted status = 0"); the link then ended 9 ms later ("Terminated reason = 8") and did not come back in 2 min. The first two boxes timed out unanswered ("SMP timeout", error 4827) |
| Buttons in Music | Not run |

**The line.** Not reached, but moved: the bond is made. What stops it now is the link dropping right after encryption (reason 8). Suspects: macOS reconnecting after a bond and the board not advertising in time, or the 15 ms interval with no peripheral latency timing out. The Mac's own log of the pairing is the next read.

**First proof on this part.** TI has no HID example for the CC23xx.

What existed: TI's `basic_ble` (Simple GATT, Device Information), TI's Battery service, the HID UUIDs in TI's headers, the GATT and bond manager chapters of TI's user guide, and HID samples from Espressif and Nordic for other chips (read for behavior only, no code copied).

Method: the HID service in TI's table idiom, kept TI's Battery and Device Information services, set the bond manager in SysConfig, and checked the report map and the generated advertisement on the Mac before flashing.

| What went wrong | Cause tag | Cost | Fix |
|---|---|---|---|
| Link error: `MIN` undefined in the repo's read callback | Our mistake (the CC13xx headers that the service generator targets define it; the F3 headers do not) | one 24 s build | open-coded the clamp |
| First design sent a pairing request on connect | Our mistake (missed Apple's rule 58.10 until reading the guidelines) | one rebuild | switched to waiting for the host |
| TI's prints-only menu mode returns FAILURE although it works | TI (undocumented, read in `menu_module.c`) | 5 minutes reading | call it and ignore the return |
| A Mac app cannot read the HID service | Apple (macOS reserves 0x1812 for its driver) | none, expected | check the map from the source and on the board's UART |
| Board shared between two tests | Our mistake | 8 minutes waiting; one flash replaced the other test's image | If you share one board, flash only when the other test is done, and note which image is on it |

Effort: 22 minutes from the first read of the SDK to the first flash (8 of them waiting for a shared board), one board run, nothing installed. Verdict: good. Writing a HID service for TI's stack is a short job once the attribute-table idiom is clear; the SDK has every piece except the service itself, and SysConfig covers the bond manager and advertising in a few lines. A builder needs the HID over GATT profile and Apple's pairing rule; the SDK documentation does not cover HID.

- Pair from an iPhone as well as the Mac; iOS is pickier about which HID maps it accepts.
- Ask for a 15 ms interval from the board after bonding (the HID over GATT Profile says the device should request its preferred parameters after service discovery and encryption); the Mac already chose 15 ms here.
- Long press for previous track and the volume keys, already in the map.
- Measure the current while connected and idle, with peripheral latency to let the board skip connection events.
- A generator description so the HID service is one JSON file.

**Ask.** If you have built a HID over GATT device on TI's BLE5-Stack, did you need anything beyond encrypted permissions to make iOS accept it?
