# Can a TI LP-EM-CC2340R5 be a Bluetooth media remote for a Mac, with no app on the Mac?

The LP-EM-CC2340R5 advertises as "FRL Remote", a Bluetooth LE HID device (HID over GATT, consumer control), and macOS accepts it as a compatible HID device and pairs with it. The buttons have not been pressed on a live link. The left button (BTN-1) is play and pause, the right button (BTN-2) is next track.

What happened on 2026-10-12: the Mac's Bluetooth log said `Device "FRL Remote" is Compatible LE HID`, read the vendor id from the board, set a 15 ms connection interval and started Just Works pairing. The first two pairing boxes timed out after 30 s (`SMP timeout`) because nobody clicked Connect. The third was accepted at 23:25 and the board logged an encrypted link. The link dropped 9 ms later (reason 8, supervision timeout) and did not come back on its own.

Neither of TI's Bluetooth SDKs ships a HID profile or a HID example for this chip, so the HID service is written by hand: one C file (315 lines, 234 of them code) and a 28-line header on TI's `basic_ble` example, built from the terminal. The image is 186.5 kB of flash and 30.9 kB of RAM, 1.0 kB less flash than `basic_ble` as TI ships it, because the button menu it replaces is gone.

Terms: [docs/vocab.md](../../docs/vocab.md). 1 kB = 1,024 bytes. Built and run 2026-10-12 on an Intel Mac, macOS 15.7.9: SimpleLink Low Power F3 SDK 9.21.00.36 LTS, SysConfig 1.26.3, TI Arm Clang 5.1.1.LTS, UniFlash 9.6.0 (DSLite), one LP-EM-CC2340R5 (probe <probe serial>), the Python library `bleak` 3.0.2 on the Mac. The port is a TI example, one patch, a `build.sh` and host scripts.

Words used here:

- **HID over GATT** (HOGP): the Bluetooth SIG profile that lets a Bluetooth LE device be a keyboard, mouse or remote. The host's own driver handles it, so no app is needed.
- **Report map**: the HID descriptor, a byte list that tells the host what each bit of a report means. Here: one byte, five buttons (play/pause, next, previous, volume up, volume down) and three spare bits.
- **Consumer control**: the HID usage page (0x0C) for media keys.
- **Just Works**: Bluetooth pairing with no code to type or compare. The board has no screen and no keypad, so it is the only method it can offer.
- **Bonding**: the two sides keep the pairing keys, so later connections are encrypted with no new pairing.

## Expected and measured

Expectations were written before the first flash.

| Step | Expected | Measured |
|---|---|---|
| Report map and report bytes checked on the Mac before the board is touched | the map decodes to the five usages in bit order; left press sends `01`, right press `02`, release `00` | `host/report_map.py`: OK, 33-byte map, report ID 1, 1-byte input report, all five bits on the usage the header names |
| Build from the terminal | under a minute | 34.7 s from a clean copy (`build.sh`); `host/verify_build.py` confirms the advertisement and bond settings in SysConfig's output |
| Flash and RAM | about `basic_ble` as shipped (187.4 kB flash) | 186.5 kB flash, 30.9 kB RAM, 5.1 kB RAM left (linker map) |
| Flash time | about 10 s | 12.6 s (DSLite erase, program, verify) |
| First advertisement on the Mac | within a few seconds of flashing | 11.9 s after DSLite started, 0.7 s before DSLite exited; 122 packets in 75 s, RSSI -60 |
| Advertisement content | flags, appearance 0x03C0 (generic HID), service UUIDs 0x1812 and 0x180F, name in the scan response | as expected; `bleak` lists both UUIDs, macOS tags the device "BLEHID" |
| Mac app (`bleak`) reads the HID service | not expected: macOS hides 0x1812 from apps | hidden, as expected. Device Information and Battery are readable: battery 100, PnP ID `01 0d00 0000 1001` (Bluetooth SIG source, vendor 0x000D, product 0, version 1.1.0) |
| macOS sees it as a HID device | yes, from the advertisement and services | yes: "Compatible LE HID", "VID:0x000D/PID:0x0000", interval set to 15 ms, Just Works pairing request shown |
| Pairing completes | after a click on Connect | yes, once Connect was clicked: board UART "Pairing Status: Encrypted status = 0" at 23:25:44 on 2026-10-12. Then "Conn status: Terminated reason = 8" 9 ms later and no reconnect within 2 min (board UART log of the run). Two earlier boxes with no click timed out after 30 s ("SMP timeout", error 4827) |
| Button press to Music | well under 100 ms (one 15 ms connection interval plus macOS) | not measured: needs the pairing first |
| Current while connected | not estimated | not run (no meter on this Mac for this board) |

## What the port is

| File | What it does |
|---|---|
| [`app/hid_remote.c`](app/hid_remote.c), [`app/hid_remote.h`](app/hid_remote.h) | The HID service (0x1812) in TI's attribute-table style: HID Information, Report Map, HID Control Point, Protocol Mode, one input Report with its CCC and Report Reference (ID 1, input). Every HID value needs an encrypted link. Adds TI's Battery service, sets the GAP appearance, opens both buttons and sends one notification per button edge. Logs each step to the board's UART |
| [`basic_ble.patch`](basic_ble.patch) | SysConfig: name "FRL Remote", bond manager (wait for the host to pair, Just Works, LE Secure Connections allowed, bonding on), advertising data, preferred interval 15 to 30 ms. `app_main.c`: the button menu is replaced by prints only, the Simple GATT service by `HidRemote_start()`. Makefile: two more objects |
| [`build.sh`](build.sh) | Checks the report map, copies TI's example, applies the patch, builds, checks the generated advertisement and bond settings, prints flash and RAM |
| [`host/report_map.py`](host/report_map.py) | Reads the report map and the bit defines from the C sources and decodes them the way a host does. Runs first in `build.sh` |
| [`host/verify_build.py`](host/verify_build.py) | Decodes the advertising and scan response bytes and the bond settings from SysConfig's generated `ti_ble_config.c` |
| [`host/hid_check.py`](host/hid_check.py) | `scan`: first advertisement with its time; `connect`: lists what an app can see and reads Battery and PnP ID |
| [`host/latency.py`](host/latency.py) | Logs the board's UART and the Music app's play state on one Mac clock and pairs each press with Music's change |

Why the board waits instead of asking to pair: Apple's Accessory Design Guidelines (release R31, 2026-09-21, section 58.10) say "The accessory should not request pairing until an ATT request is rejected using the Insufficient Authentication error code." The encrypted HID values do exactly that, and the Mac then pairs. The Bluetooth SIG's HID over GATT Profile asks for an encrypted, bonded link ("HID Devices shall bond and use LE Security Mode 1, Security Level 2 or 3"). Battery and Device Information stay readable without pairing, because Apple's section 58.9 says discovery should not need it.

Behavior, not code, was taken from the public HID samples of Espressif (ESP-IDF `ble_hid_device_demo`) and Nordic (nRF Connect SDK `peripheral_hids_keyboard`): what the HID Information value holds, that a consumer report sits beside a keyboard, which services they advertise. No code was copied from either.

## Run it

```bash
./build.sh
```

Flash (the same probe and `.ccxml` as the other CC2340R5 ports):

```bash
/Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c <abs path>/cc2340r5/CC2340R5.ccxml -e -f -u <abs path>/ports/ble-hid-remote/cc2340r5/freertos/ticlang/basic_ble.hex
```

See it from the Mac (Bluetooth only, no pairing). Install the library first with `python3 -m pip install bleak==3.0.2`:

```text
python3 host/hid_check.py scan 30
```

Pair it: System Settings, Bluetooth, find "FRL Remote" under Nearby Devices, click Connect, and accept the pairing box macOS shows about a second later. There is no code. Then open Music, play a song and press BTN-1 (next to the DIO10 label) to pause and play, BTN-2 (next to DIO9) to skip.

Time the buttons, with the board's serial port free:

```text
python3 host/latency.py 300 press-log.txt
```

## Limits

- **Paired, then dropped.** The bond completed (encrypted link, status 0), and the link ended 9 ms later with reason 8 and did not come back. Whether macOS dropped it on purpose after bonding (it often reconnects) or the board timed out is not known: the Mac's log for that minute has not been read. Whether the Mac turns on notifications for the report, and whether Music reacts, are not seen.
- **No iPhone test.** iOS is expected to behave like macOS for a consumer-control device; Apple's forum notes that iOS ignores HID devices whose map it does not support, so this needs its own check.
- **A Mac app cannot read the HID service.** macOS keeps 0x1812 for its own driver, so the report map can only be checked on the Mac side by `host/report_map.py` (from the source) and on the board's UART log (it prints each read of the map with its offset).
- **One fixed report, two buttons.** Previous track and volume are in the report map but no button sends them.
- **USB powered.** The battery level is fixed at 100. No sleep tuning and no current measured.
- **Vendor id.** The PnP ID is the SDK's default: Bluetooth SIG source, vendor 0x000D (Texas Instruments' company id), product 0. A product would use its own.
- **One connection, five bonds.** The SysConfig defaults keep up to five bonds; a sixth host is refused until one is cleared (the default bond-fail action erases that host's bond).

## FAQ

### Why did my Mac show a pairing box for FRL Remote when I only ran a Python script?

A Mac app that connects (for example `host/hid_check.py connect`) makes macOS notice the HID service, and macOS's own HID driver then asks to pair (its log: "Displaying pairing request alert"). Accept it to pair, or ignore it: pairing times out after 30 seconds and the board advertises again.

### Why can my Python script not see the HID service or read the report map?

macOS hides the HID service (0x1812) from apps; the system's HID driver uses it. `bleak` lists Device Information and Battery only. Check the map with `python3 host/report_map.py` instead.

### FRL Remote is paired but the buttons do nothing in Music

Not seen; the first run stopped before pairing. Things to check first: Music must have a song loaded; the board's UART log (`host/latency.py`) prints `HID tx n=.. bits=0x01` for each press and `HID report CCC=1` once the Mac turned on notifications. If the CCC line never appears, the Mac did not accept the report map.

### How do I pair it again from scratch?

Remove FRL Remote in System Settings, Bluetooth (the info button, Forget This Device), then reflash the board with `-e` (the erase also clears the board's bonds), then Connect again.
