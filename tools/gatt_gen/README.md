# Generate a Bluetooth GATT service for TI's LP-CC1354P10-1 and Zephyr from an 8-line description

Yes: `blinky.json` (8 lines) generates 223 lines of TI BLE5-Stack C (173 code lines) and 152 lines of Zephyr C (111 code lines), and the LP-CC1354P10-1 Blinky image built with the generated service is 100 bytes smaller in flash than the hand-written one, with the same RAM. It also ran: after a USB replug the generated image loaded and a Mac listed the service, wrote the LED and read the button, with the same output as the hand-written image. Not checked: a Mac that had cached the old table.

Terms: [docs/vocab.md](../../docs/vocab.md). A [GATT service](../../docs/vocab.md) is a group of values a Bluetooth device offers to a phone; each value is a [characteristic](../../docs/vocab.md), named by a 128-bit [UUID](../../docs/vocab.md); a phone can read it, write it, or be [notified](../../docs/vocab.md) when it changes. On the device, the services live in an [attribute table](../../docs/vocab.md): a numbered list of entries that the phone reads and writes by number. A [CCCD](../../docs/vocab.md) (client characteristic configuration descriptor) is the entry a phone writes to turn notifications on. BLE5-Stack is TI's Bluetooth Low Energy [radio stack](../../docs/vocab.md) in the SimpleLink CC13xx CC26xx SDK. The [XDS110](../../docs/vocab.md) is the debug probe built into the LaunchPad development kit. Errors and fixes: the FAQ at the end of this page and [cc1354p10/FAQ.md](../../cc1354p10/FAQ.md). The description format is in service.schema.md. 1 kB = 1,024 bytes.

Checked 2026-10-07 on an Intel Mac, macOS 15.7.9, Python 3.14.7 (standard library only). TI side: SimpleLink CC13xx CC26xx SDK 8.33.00.16, SysConfig 1.26.3, TI Arm Clang 5.1.1.LTS, CCStudio's GNU Make 4.4.1. Zephyr side: upstream Zephyr 4.4.2 (commit `dccb0959`), GCC 12.2.0 from Zephyr SDK 0.17.4 through `cross-compile`.

Contents: why generate it; how to run it; expected against measured; the API the TI output gives the application; traps; limits; FAQ; to repeat this on a board.

## Why generate it

A custom service means the same thing on every chip: three UUIDs, two one-byte values, who may read or write. Each Bluetooth stack wants that meaning in its own C dialect: TI wants an attribute table plus read and write callbacks, Zephyr wants a `BT_GATT_SERVICE_DEFINE` block. Written by hand, that layer is the biggest part of the port and the easiest place to get a byte order wrong. portability/driver-layering.md explains the split; this tool is its step "When the service must be custom, write it once as data and generate the stack's table."

## Run it

```bash
python3 tools/gatt_gen/gatt_gen.py tools/gatt_gen/blinky.json --target ti-ble5stack --out <folder>
```

```bash
python3 tools/gatt_gen/gatt_gen.py tools/gatt_gen/blinky.json --target zephyr --out <folder>
```

A third target, `--target ti-f3` (added 2026-10-12), writes the TI output for the SimpleLink Low Power F3 SDK 9.21 (CC2340R5 and other CC23xx): it ran on an LP-EM-CC2340R5 in [ports/nordic-apps](../../ports/nordic-apps/README.md).

Each writes `blinky_service.c` and `blinky_service.h` into `<folder>` and prints their line counts. Its own code-line count keeps code lines that start with a pointer `*`, such as `*len = BLINKY_LED_LEN;`, which the command below leaves out, so it prints a few more (154 and 97 code lines for the TI and Zephyr `.c` files on 2026-10-07). The counts on this page come from the command. The Blinky port's build.sh runs the first command itself, so the files in its `src/` are always the generator's output; the hand-written ones are kept in `src/handwritten/` for comparison. Host checks, no board needed:

```bash
python3 tools/gatt_gen/test_gatt_gen.py
```

It checks the UUID bytes against the hand-written header that a Mac read correctly from the board, checks the generated table has the same attributes in the same order, and checks that bad descriptions are refused. 11 of 11 pass.

## What was expected, and what was measured

Code lines on this page are non-blank lines that are not comment-only (lines starting with `//`, `/*` or `*`), the same basis as `portability/driver-layering.md` and `ports/bthome/README.md`. Each count is what this command printed on 2026-10-07, with the file in place of `<file>`:

```text
grep -vcE '^\s*$|^\s*(//|/\*|\*)' <file>
```

Because a line starting with `*` counts as a comment, the command also leaves out the few code lines that start with a pointer `*`: 3 in the generated TI `.c`, 2 in the Zephyr `.c`, 1 in the hand-written `.c`. The Zephyr files were generated into a scratch folder outside the repo for the count; the TI files are the ones in `ports/phone-app-demos/blinky-cc1354p10/src/` and `src/handwritten/`.

| Check | Expected | Measured |
|---|---|---|
| Description | about 10 lines | `blinky.json`: 8 lines |
| TI output | more code lines than the hand-written 117, since it adds a get and set per characteristic | 223 lines, 173 code lines: `.h` 43 lines (22 code), `.c` 180 lines (151 code). Hand-written: 176 lines, 117 code lines: `.h` 40 lines (14 code), `.c` 136 lines (103 code) |
| Zephyr output | shorter than TI's | 152 lines, 111 code lines: `.h` 31 lines (16 code), `.c` 121 lines (95 code) |
| The generator | no estimate made | `gatt_gen.py`: 393 lines, both targets |
| TI build of Blinky | builds clean, first try | Built in 10 s with `build.sh`, no warnings |
| Flash, generated against hand-written | a little larger, from the extra get and set functions | 192.1 kB against 192.2 kB: 100 bytes smaller. The service object: code 424 bytes in both, read-only data 68 against 184 (the hand-written file's log strings; the generated file does not log), read-write data 115 against 110. The linker dropped the three get and set functions Blinky never calls. `project_zero.c` grew 24 bytes of code for the changed calls |
| RAM | the same | 17.6 kB in both (the [RTOS](../../docs/vocab.md) heap is fixed, so small static changes do not move it) |
| TI output with every property | compiles | `examples/all_properties.json` (write, write without response, notify, indicate, a 20-byte value, two descriptions): compiles with the Blinky build's own compiler flags plus `-Wall -Wextra`, no warnings from the generated file |
| Zephyr output | compiles in principle | Compiles **and links** in an upstream Zephyr 4.4.2 Bluetooth peripheral for `lp_mspm0g3507` (both services, 39.0 kB flash, 14.6 kB RAM): [zephyr_check/build.sh](zephyr_check/build.sh), 40 s. Not flashed and not run: that board has no Bluetooth radio, so the build uses a controller on a serial port that is not there |
| On the board, from the Mac | same as the hand-written build: service listed; LED 00, then 01, then 00; button 00; no notifications | Ran 2026-10-07 after a USB replug: loaded in 15 s with `ccs_load_run.py`, seen after 1.3 s as "Blinky FRL", the service listed, LED read 00, write 01 read 01, write 00 read 00, button 00, no notifications with nobody pressing: the same output as the hand-written image. The first load right after the replug failed with `invalid XDS110 serial number, or faulty USB cable`; three seconds later it worked |

Sizes are the "used" column of each linker map: `ports/phone-app-demos/blinky-cc1354p10/out/frl_blinky_cc1354p10.map` (generated) and `out/handwritten/frl_blinky_cc1354p10.map` (hand-written, same SysConfig settings). The port's README quotes 192.2 kB for the hand-written build from before the random Bluetooth address was added; the 192.2 kB here is the later build with the same settings as the generated one.

The baseline: the hand-written image on the board, same test (mac_test.py), 2026-10-07:

```text
seen after 2.2 s: {'490CE289-74E5-2E4A-B57A-C241FFED032C': ('Blinky FRL', -39)}
services: ['0000180a-0000-1000-8000-00805f9b34fb', 'f0001110-0451-4000-b000-000000000000', 'f0001120-0451-4000-b000-000000000000', 'f0001130-0451-4000-b000-000000000000', 'de8a5aac-a99b-c315-0c80-60d4cbb51224']
LED before: 00
LED after write 01: 01
LED after write 00: 00
button value: 00
button notifications in 4 s (nobody pressing): []
```

## What the TI output gives the application

Everything is named from the description. For Blinky:

- `BlinkyService_AddService(rspTaskId, changeCb)` registers the table. `changeCb(connHandle, charId, len, pValue)` runs after a phone writes; `charId` is `BLINKY_LED_ID` or `BLINKY_BUTTON_ID`.
- `BlinkyService_SetLed`, `BlinkyService_GetLed`, `BlinkyService_SetButton`, `BlinkyService_GetButton` copy a value in or out; the length must equal `BLINKY_LED_LEN` or `BLINKY_BUTTON_LEN`.
- `BlinkyService_NotifyButton()` sends the current button value to every phone that turned notifications on.

This differs from the hand-written file, which had a callback taking one `on` byte and a `SetButton(pressed)` that also notified. `project_zero.patch` now calls the generated functions: it adds 17 code lines to `project_zero.c` instead of 10 (the lines the patch adds to that file, counted with the same command). One behavior change: the hand-written file stored any nonzero LED write as 1; the generated one stores the byte the phone wrote. The phone app only writes 0 and 1.

The generated table has the same six attributes in the same order as the hand-written one, so the handles do not change and a Mac that cached the old table should not need a new board address. The Mac in the test had never cached the old table at that address.

## Traps

1. **Byte order.** Bluetooth sends a 128-bit UUID low byte first, so the C array is the text's bytes reversed. Python's `uuid.bytes_le` is a different order (only the first three groups are swapped) and gives a wrong UUID. The generator reverses `uuid.bytes` and the host test checks the result against bytes a Mac read correctly from the board. Zephyr's `BT_UUID_128_ENCODE` does the reversal itself, so the Zephyr output passes the text groups instead. **Tool or docs:** the SDK has no helper that turns UUID text into this array; TI's own Project Zero headers spell the bytes out.
2. **Line endings in the patch.** The SysConfig part of `project_zero.patch` has Windows line endings, because TI's `.syscfg` file does. Editing the patch with a tool that rewrites line endings made all three SysConfig hunks fail (`3 out of 3 hunks failed--saving rejects to 'tirtos7/project_zero.syscfg.rej'`). Edit only the `project_zero.c` part, or keep the bytes as they are. **Our mistake.**
3. **The debugger cannot reach this board while its Bluetooth program runs.** Error -242, `A router subpath could not be accessed`, came back seven times, including after a pin reset through the probe (`xds110reset`). A USB replug clears it ([cc1354p10/README.md](../../cc1354p10/README.md)). **Unknown (suspected: Chip limit).** The chip's debug path seems to stay closed until a power cycle. Only TI's loader script and DSLite (UniFlash's command line), both through the board's own XDS110, were tried; a different probe and an SWD-only attach were not, so this may be the setup rather than the chip.

## Limits

- A Mac that already cached the hand-written image's table at the same address was not checked. The handles match, so no address change should be needed.
- No user description (0x2901) has been read from a board. TI's own `simple_gatt_profile.c` never handles it in its read callback, so the stack seems to answer it alone; the generated read callback answers it too.
- Indications and write without response on TI compiled and did not run.
- The Zephyr output built for a chip with no radio and did not run on one. TI's Zephyr fork has no working Bluetooth on the CC2340R5 (ports/bthome/zephyr/README.md).

## FAQ

### `3 out of 3 hunks failed--saving rejects to 'tirtos7/project_zero.syscfg.rej'`

Put the original `project_zero.patch` back from git and edit only its `project_zero.c` part. The SysConfig part of the patch has Windows line endings because TI's `.syscfg` file does, and an editor that rewrote them broke all three hunks.

### The loader fails with error -242, `A router subpath could not be accessed`

Unplug the LP-CC1354P10-1's USB cable, plug it back in, and load again. While a Bluetooth program runs, the debugger could not reach the chip again until a power cycle; a pin reset through the probe did not help. A different probe and an SWD-only attach were not run. More in [cc1354p10/FAQ.md](../../cc1354p10/FAQ.md).

### The first load after a replug says `invalid XDS110 serial number, or faulty USB cable`

Wait a few seconds and run the same command again. It happened once, right after the replug, and the next try three seconds later worked. The probe was probably still starting up, but that was not checked.

### The generated UUID is wrong: the Mac shows the service with its bytes in a strange order

Reverse the bytes of the UUID text; do not use Python's `uuid.bytes_le`. Bluetooth sends a 128-bit UUID low byte first, so the C array is the text's 16 bytes in reverse, and `bytes_le` swaps only the first three groups. The SDK has no helper for this. `python3 tools/gatt_gen/test_gatt_gen.py` checks the generator against bytes a Mac read correctly from the board.

### The Mac still shows the old services after you flash the new image

Give the build a new random address in SysConfig, or restart Bluetooth on the Mac. macOS keeps a board's attribute table per address and does not reread it after a reflash.

## To repeat this on a board

Unplug and replug the LP-CC1354P10-1, then load the image, from the repo root (the loader needs the full path to the image). `/Applications/ti/ccs2101` is where CCStudio 21.0.1 installs; change it if you have another version.

```bash
/Applications/ti/ccs2101/ccs/scripting/python/run.sh cc1354p10/tools/ccs_load_run.py "$PWD/ports/phone-app-demos/blinky-cc1354p10/cc1354p10/tirtos7/ticlang/project_zero.out"
```

The Mac test needs the `bleak` Bluetooth library for Python. Make a Python environment for it once, from the repo root (it makes a `ble` folder there; leave it out of git):

```bash
python3 -m venv ble && ble/bin/pip install bleak
```

Then run the test:

```text
ble/bin/python ports/phone-app-demos/blinky-cc1354p10/mac_test.py
```

The first time, macOS asks whether Terminal may use Bluetooth. Allow it, or turn it on later in System Settings, Privacy and Security, Bluetooth; without it the scan finds nothing. The output should match the log above: the service listed, LED 00, 01, 00, button 00.

If the service is listed but reads fail, change the last byte of `ble.randomAddress` in the patch to `:02`, rebuild and reload.
