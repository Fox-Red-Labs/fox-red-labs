# LP-EM-CC2340R5 as a peripheral for Nordic's phone apps (nRF Blinky and Nordic UART terminals)

**Works** from a Mac; the phone apps did not run. One TI example (`basic_ble`) plus two 8-line service descriptions run through `tools/gatt_gen` give a board that advertises as "FRL Nordic" with Nordic's LED Button Service and Nordic UART Service. A Mac with the Python library `bleak` finds it by either service UUID, writes the LED characteristic and gets the answer back, and every one of 60 lines sent to the UART service came back identical. The phone apps (nRF Blinky, nRF Toolbox) and the physical button have not been tried. A second Mac check on 2026-10-25 (100 UART lines, five timed connects, the LED pin read through the probe, the button driven from the probe) stopped at the first connect: the image rebuilt byte for byte and flashed, the Mac saw it advertise, and then the Mac's own Bluetooth service crashed.

**Where it comes from.** Nordic's service UUIDs and the way their apps scan are public (sources below); no Nordic code is used. License: Nordic's firmware (nRF Connect SDK, `LicenseRef-Nordic-5-Clause`) may only be used with a Nordic chip, so it is read as a behavior spec and none of it is copied; Nordic's phone apps (nRF Blinky, nRF Toolbox, Android-BLE-Library) are BSD-3-Clause and are used as they are. Checked from a Mac.

Terms: [docs/vocab.md](../../docs/vocab.md). A GATT service is a group of values a phone can read, write or be notified of; a UUID is the 128-bit number that names it. 1 kB = 1,024 bytes. Measured 2026-10-12 on an Intel Mac, macOS 15.7.9: SimpleLink Low Power F3 SDK 9.21.00.36 LTS, SysConfig 1.26.3, TI Arm Clang 5.1.1.LTS, UniFlash 9.6.0 (DSLite), one LP-EM-CC2340R5 (probe <probe serial>), `bleak` as the central.

Code lines on this page are non-blank lines that are not comment-only, counted with `grep -vcE '^\s*$|^\s*(//|/\*|\*)' <file>` on 2026-10-12.

Contents: what the two services are; expected against measured; what the port is; run it; honest limits; FAQ; next.

## What the two services are, and where they are documented

| | UUID | Properties | Source |
|---|---|---|---|
| LED Button Service | `00001523-1212-efde-1523-785feabcd123` | service | `include/bluetooth/services/lbs.h` in Nordic's nRF Connect SDK, `BT_UUID_LBS_VAL`; also `BlinkySpec.kt` in Nordic's Android-nRF-Blinky and `BlinkyPeripheral.swift` in iOS-nRF-Blinky |
| Button | `00001524-...` (same base) | read, notify, 1 byte | the same headers; the Android app requires NOTIFY or INDICATE |
| LED | `00001525-...` | read, write, 1 byte | the same; the Android app requires WRITE and reads it once on connect |
| Nordic UART Service | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` | service | `include/bluetooth/services/nus.h`, `BT_UUID_NUS_VAL` |
| RX (phone writes) | `6e400002-...` | write, write without response | `BT_UUID_NUS_RX_VAL` |
| TX (phone is notified) | `6e400003-...` | notify | `BT_UUID_NUS_TX_VAL` |

The sources are GitHub raw files, read 2026-10-12: `raw.githubusercontent.com/nrfconnect/sdk-nrf/main/include/bluetooth/services/lbs.h` and `.../nus.h`; `github.com/NordicSemiconductor/Android-nRF-Blinky` (`blinky/spec/.../BlinkySpec.kt`, `scanner/.../BlinkyScanner.kt`, `blinky/ble/.../LedButtonProfileImpl.kt`); `github.com/nordicsemi/iOS-nRF-Blinky` (`nRFBlinky/Models/BlinkyPeripheral.swift`, `ScannerTableViewController.swift`). The headers and app sources are the reference, not Nordic's prose documentation pages.

**What nRF Blinky looks for.** The Android app lists a device only if it has a name and advertises the LED Button Service UUID (filters "only with names" and "with service UUID 00001523-..." are on by default); the iOS app scans with that UUID as its filter. It then needs the LED characteristic writable and the Button characteristic notifiable, or it disconnects with "not supported". The device name does not matter.

## Expected and measured

Written before the first build: the F3 stack uses the same GATT calls as the F2 stack `gatt_gen` was written for, so the generated file needs only include and macro changes; one 128-bit UUID fits in the advertisement beside the flags, the name and the second UUID go in the scan response; the Mac finds the board within a second or two.

| Check | Expected | Measured |
|---|---|---|
| Generate the two services for the F3 stack | `gatt_gen` needs a small target change | New target `ti-f3` (include paths, `MAX_NUM_BLE_CONNS`, no `MIN` or `CONST` macros) and a new `variable` field for values of 1 to N bytes, which Nordic UART needs. The old target's output for Blinky is byte-identical to before; `test_gatt_gen.py` 15 of 15 (was 11) |
| Generated code | compiles on the first try | Compiled and linked on the first try, no warnings in this repo's files |
| Build from the terminal | under a minute | 38 s, `./build.sh`, one SDK example copy, one patch (3 files, 39 changed lines), 3 files added, 3 objects added to the makefile |
| Flash image | about the size of `basic_ble` (187.4 kB) | 187.3 kB (TI's example service dropped, two added) |
| RAM | a little more than `basic_ble` | 32.0 kB used, 4.0 kB left of 36 kB (`basic_ble`: 5.1 kB left). The difference is the 512-byte send queue and the two 244-byte value buffers |
| Advertisement | flags + LED Button UUID in the advertisement, name + UART UUID in the scan response | As built: 21 bytes in the advertisement, 30 in the scan response (limit 31). The Mac sees both UUIDs and the name "FRL Nordic" together |
| Time from DSLite exit to the first advertisement seen | 1 to 2 s (earlier row: 1.4 s) | 0.69, 0.17, 0.17 s over three flashes (flash itself 12.1 to 12.6 s) |
| Service table on the Mac | both services, characteristics with the right properties | Listed: LED Button (Button: read, notify; LED: read, write) and Nordic UART (RX: write, write without response; TX: notify), plus TI's Device Information service |
| LED write | the board accepts 0 and 1 and reads back what was written | 00, 01, 00 read back after each write; 60 writes in three runs, every one answered |
| Write to answer, 20 writes (median) | about the connection interval, 30 to 50 ms | **24.3 ms** (three runs: 24.2, 24.3, 24.5; min 22.6, max 44.7). The Mac's connection interval is inside the number; the board sets the LED pin in the write callback before it answers |
| LED lit on the board | lit when 1 is written | **Not seen.** The LED could not be told apart from sunlight stripes, so it needs a person's check |
| UART echo, 20 lines (median) | like the write: about 30 to 50 ms | **24.2 ms** (24.3, 23.9, 24.2; min 22.7, max 39.8), 14-byte lines, write without response to the TX notification; 60 of 60 echoes identical |
| A line longer than one notification | chunked by the negotiated size | 101 bytes sent in five 20-byte writes came back in 2 notifications (62 and 39 bytes): the Mac negotiated an ATT MTU of 65; identical |
| Two lines in one write | two echoes, in order | `a\nb\n` returned as sent |

## Phone apps, step by step

The steps follow what the apps' sources check for (above).

1. Flash `out/frl_nordic_cc2340r5.hex` (Run it, below). The board advertises as "FRL Nordic" about a second after the flash ends.
2. Install nRF Blinky and nRF Toolbox (publisher Nordic Semiconductor ASA, free) on an Android or iOS phone. Allow Bluetooth (and, on Android, Nearby devices).
3. nRF Blinky: the scanner lists "FRL Nordic" (it filters on the LED Button Service UUID and a name). Tap it. The LED switch drives the red LED (DIO14); pressing BTN-1 (DIO10) turns the Button indicator on while held.
4. nRF Toolbox: open UART, Connect, pick "FRL Nordic". Type a line and send: the same line comes back in the log. Pressing BTN-1 also sends the line `button`.
5. nRF Connect (the general app) lists both services and lets you write the LED and UART RX values by hand and enable the notifications.

If you see "FRL Nordic" in the list but the connect spins and fails, see the FAQ entry on the Mac's earlier pairings below.

## Mac client check, 2026-10-25

`host/nordic_pack.py` does what the phone apps do, from a Mac with bleak 3.0.2 (Python 3.14, macOS 15.7.9, Intel): scan by name and by each UUID, five timed connects, the service list, LED writes with the LED pin's output register read through the probe after each one (`host/gpio_probe.py`, CCStudio 20.1 scripting), the button stood in for by the probe (DIO10 driven low for 2 s, then let go; the firmware has no timer or console command that does it), then 100 Nordic UART lines of 19 bytes. The run backs up the board, flashes, tests, restores and reads back.

| Check | Expected (written before the run) | Measured |
|---|---|---|
| Rebuild from the patch with SysConfig 1.28.0 (1.26.3 was removed from this Mac) | builds unchanged | Built, 0 errors; the hex is byte-identical to the 2026-10-12 one (sha256 `c05c3e66...e194`), 187.3 kB flash, 4.0 kB RAM left |
| Flash | about 12 s | 18.0, 24.5 and 19.5 s over three flashes (DSLite, the Mac busy with other builds) |
| First sighting with a fresh scanner, board already advertising | about 1 s | by name 3.01 and 3.09 s; by LED Button UUID 2.11 and 1.77 s; by UART UUID 1.81 s (a 0.31 s sighting came from the Mac's cache, RSSI 127). The name and both UUIDs arrive together every time |
| Connect and discover, 5 times | 0.3 to 1.5 s | **Not measured.** First run: the first connect timed out after bleak's 10 s. Second run: the connect hung, and 38 s later the Mac's `bluetoothd` was killed by its XPC watchdog; it came back with the controller off. Cause not found |
| LED pin (DOUT bit 14) follows each write | 1, 0, 1, 0 | Read through DSLite after the link closed (CCStudio scripting resets the chip, so its read shows the boot state): 1 after 01 and 0 after 00, 3 of 3 each |
| Button notify, timed (2026-11-02, test build: the line `btn1` stands in for BTN-1) | `01` within about 25 to 50 ms | **23.8 ms** median of three runs of 20 (worst 29.8); the probe cannot hold DIO10 on a live link, a debugger access drops it |
| 100 UART lines of 19 bytes, median and worst | about 24 ms, worst under 50 ms | **23.9 ms** median of three runs, worst 40.3 ms, 300 of 300 identical (`host/nordic_nolink.py`, no probe during the link; also LED write with response 29.9 ms median, pin read through DSLite after the link: 1 after `01`, 0 after `00`, 3 of 3 each) |

The board was restored after each run and read back identical to the backup (main flash sha256 `c4184cbd...fe2d`, CCFG `fc510514...fae4`).

## What the port is

- `services/led_button.json`, `services/nordic_uart.json`: the two descriptions, 8 lines each. `build.sh` runs `tools/gatt_gen/gatt_gen.py --target ti-f3` on each; the generated C is in `src/` (189 and 180 lines; 160 and 151 code lines). Not edited by hand.
- `app/app_nordic.c` (202 lines, 156 code lines): registers both services, drives the red LED from the LED characteristic, turns the left button into Button notifications and a "button" line on UART TX, and echoes each received line. A line ends at a newline, a carriage return or 120 bytes. Notifications go out in pieces of at most MTU minus 3 bytes; if the stack has no buffer free the send retries 8 ms later.
- `basic_ble.patch`: `freertos/basic_ble.syscfg` (device name "FRL Nordic"; the LED Button UUID in the advertisement; the UART UUID and the name in the scan response; power level off, 16-bit UUID list off; the button instance renamed, see below) and `app/app_main.c` and `.h` (call `Nordic_start()` where TI called `SimpleGatt_start()`).
- `build.sh`: copies TI's `basic_ble` from the SDK, applies the patch, copies the new files in, adds three objects and their rules to the example's makefile, builds, and copies the hex and map to `out/`.
- `host/nordic_test.py`: the Mac test (scan, full, led, button). `host/flash_to_adv.py`: flash, then time to the first advertisement. `host/nordic_pack.py`: the longer client check (timed connects, 100 UART lines, LED pin and button through the probe), with `host/gpio_probe.py` for the probe side (run under CCStudio's Python).
- Pins: the red LED is DIO14 (device pin 9) and the left button is BTN-1 (silkscreen "BTN-1", DIO10, active low), both on the board itself and not on a BoosterPack header, so there is no J-connector pin. Close the LEDs jumpers.

## Run it

```bash
./build.sh
/Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c <abs path>/cc2340r5/CC2340R5.ccxml -e -f -u <abs path>/ports/nordic-apps/out/frl_nordic_cc2340r5.hex
python3 host/nordic_test.py full
```

The Mac asks once whether Terminal may use Bluetooth; allow it. `host/nordic_test.py button 30` prints Button and UART lines for 30 s while you press BTN-1.

On a phone: install nRF Blinky and nRF Toolbox (both Nordic Semiconductor, free), open Blinky and tap "FRL Nordic", or open Toolbox, UART, Connect.

## Take it with you

Copy these files into a folder of your own, keeping the folder names. `out/` and `cc2340r5/` are created by the build.

| File | What it is |
|---|---|
| `build.sh` | Copies TI's `basic_ble` from the SDK, applies the patch, copies the new files in, adds three objects to the example's makefile, builds, and leaves the hex in `out/` |
| `basic_ble.patch` | Against TI's `basic_ble`: `freertos/basic_ble.syscfg` (device name, the advertisement and scan-response contents, the button renamed) and `app/app_main.c` and `.h` (call `Nordic_start()` where TI called `SimpleGatt_start()`) |
| `app/app_nordic.c` | Registers both services, drives the red LED from the LED characteristic, turns BTN-1 into Button notifications and a `button` line on UART TX, echoes each received line |
| `src/led_button_service.c`, `.h`, `src/nordic_uart_service.c`, `.h` | The generated GATT service code; `build.sh` copies them into the example |
| `services/led_button.json`, `services/nordic_uart.json` | The 8-line service descriptions the four files above come from; only needed to regenerate them with `tools/gatt_gen/gatt_gen.py --target ti-f3` from this repo |
| `host/nordic_test.py` | Optional Mac or Linux test client (`scan`, `full`, `led`, `button`); needs `bleak` |

**What it needs.**

- SimpleLink Low Power F3 SDK 9.21.00.36 LTS (`SDK`), SysConfig 1.26.3 or 1.28.0 (`SYSCFG`, the path of `sysconfig_cli.sh`) and TI Arm Clang 5.1.1.LTS (`CLANG`). The script's defaults are the install folders under `/Applications/ti`.
- No defines and no force-include flag. The script adds `-I../../app` and the three object rules to the example's makefile itself.
- Without `tools/gatt_gen/gatt_gen.py` beside it, `build.sh` says so and uses the generated files in `src/`; `GATT_GEN=<path>` points it at your copy of the tool.

**A different board in the same family.** Only the LP-EM-CC2340R5 was built. For another F3 board with a `basic_ble` example, change `EX=` in `build.sh` to that board's `examples/rtos/<LP_board>/ble/basic_ble`, regenerate `basic_ble.patch` against it (the syscfg hunk carries the device name and advertisement; its pin and button instance names differ per board), and set the pins in `app/app_nordic.c`: `CONFIG_GPIO_LED_RED` and `CONFIG_GPIO_LED_ON` (lines 115 and 184) and `CONFIG_NORDIC_BTN_LEFT` (line 196). The LP-CC1354P10-1 needs `--target ti-ble5stack` for the generator and has not been built.

**Build from a fresh folder** (`<proj>` holds the files above):

```bash
cd <proj>
SDK=<sdk> SYSCFG=<sysconfig>/sysconfig_cli.sh CLANG=<cgt> ./build.sh      # about 40 s; image out/frl_nordic_cc2340r5.hex
<uniflash>/dslite.sh --mode flash -c <ccxml> -e -f -u out/frl_nordic_cc2340r5.hex
```

Built from a copy of only these files on 2026-10-25 (the six build files and the sources, no `tools/gatt_gen`, copy outside the repo, SysConfig 1.28.0, zero errors): the hex has sha256 `c05c3e66...e194`, byte-identical to the one built in the repo. The image was not flashed from that build.

## Honest limits

- Mac only. The phone apps and the button are not run; nothing here says they work.
- The LED pin itself was not observed. The callback writes the pin, and the characteristic reads back what was written.
- The latency figures are round trips on the Mac clock with a connection interval macOS chose and that was not read. They are an upper bound on the board's own time, not the board's time.
- The 24 ms figures are a median of three runs of 20; the tails reach 45 ms.
- No security: any phone connects and writes. Fine for Nordic's demo apps, not for a product.
- One image holds both services (the generator makes one file per service; both are linked). The LP-CC1354P10-1 was not done.
- TI's own `0xFFF0` example service is gone from this image; the Device Information service stays.

## FAQ

### The phone app does not list "FRL Nordic"

The board may be running a different image (for example after another project's image). Scan from the Mac with `host/nordic_test.py scan`; if the name is missing, flash `out/frl_nordic_cc2340r5.hex` again.

### SysConfig says `The max allowed data length for Legacy adv type is 31 ,the current length is 33`

The scan response held the name (12 bytes), the UART UUID (18 bytes) and a power-level field (3 bytes) that SysConfig turns on by default. Set `scanRes1.GAP_ADTYPE_POWER_LEVEL = false`. The 16-bit UUID list in the advertisement is also on by default and has to be switched off before a 128-bit list is allowed.

### The phone finds the board but the service UUID in the advertisement is wrong

SysConfig writes a 128-bit UUID into the packet in the order you give it, not low byte first as the air wants. Give it the UUID text with its bytes reversed (the patch does, with a comment). Check with `host/nordic_test.py scan`: the printed service UUIDs must be the Nordic ones.

### The button does nothing in the example's serial menu any more

Intentional. TI's menu opens buttons named `CONFIG_BUTTON_0` and `CONFIG_BUTTON_1`; this image names its button `CONFIG_NORDIC_BTN_LEFT` so the menu leaves it alone.

### `./build.sh` stops with `sysconfig_cli.sh: No such file or directory`

The SysConfig version in the script is no longer installed. Point it at the one you have: `SYSCFG=/Applications/ti/sysconfig_1.28.0/sysconfig_cli.sh ./build.sh`. SysConfig 1.28.0 gave a byte-identical hex.

### The Mac sees "FRL Nordic" but the connect times out, or the Mac's Bluetooth turns itself off

Turn Bluetooth off and on in Control Center (or run `sudo pkill bluetoothd`; it restarts itself), then check `system_profiler SPBluetoothDataType` shows `State: On`. In System Settings, Bluetooth, remove any device this board was paired as with another image (for example "CC2340R5 Gamepad"), then connect again. On 2026-10-25 a bleak connect timed out after 10 s, the next one hung, and macOS's `bluetoothd` was then stopped by its watchdog and came back with the controller off; the same image had connected 3 of 3 times on 2026-10-12. Whether an old pairing for the same chip is the trigger is not proven.

### How do I read the negotiated MTU?

Not with `ATT_GetMTU`: in this SDK's `icall_ble_api.h` it is defined as `(AssertHandler(0,0))` (read in the header; it was not called). Take the MTU from the `ATT_MTU_UPDATED_EVENT`, as `app_nordic.c` does.
