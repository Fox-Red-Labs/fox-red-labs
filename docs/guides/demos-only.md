# How do I flash a TI demo onto an LP-EM-CC2340R5 or LP-CC1354P10-1 without building it?

Run one script in Terminal: `python3 tools/demo_flash.py flash LP_EM_CC2340R5 basic_ble_app --yes`. It backs the board up, then flashes TI's ready-made Bluetooth example from the SDK, with no compiler and no CCStudio. `--yes` means go ahead; without it nothing changes.

**Limits.** The list and the dry run below ran on 2026-10-07. The real flash with `--yes` and the restore have not been run, so the backup folder this page describes has not been made on the test Mac. Time: about 10 minutes hands-on once the software is installed (estimate, not measured).

Terms: [docs/vocab.md](../vocab.md). The [SDK](../vocab.md) is TI's kit of examples for one chip family; [UniFlash](../vocab.md) is TI's flashing tool; the [probe](../vocab.md) ([XDS110](../vocab.md)) is the small debugger between the Mac's USB port and the chip; a [hex file](../vocab.md) is a built program ready to flash. Errors and fixes: the FAQ at the end of this page, [cc2340r5/FAQ.md](../../cc2340r5/FAQ.md) and [cc1354p10/FAQ.md](../../cc1354p10/FAQ.md). Checked on an Intel Mac, macOS 15.7.9, Python 3.14.7, UniFlash 9.6.0, SimpleLink Low Power F3 SDK 9.21.00.36 LTS, SimpleLink CC13xx CC26xx SDK 8.33.00.16. 1 kB = 1,024 bytes.

Contents: what you need; step 0, open Terminal; step 1, list the demos; step 2, dry run; step 3, back up and flash; what can go wrong; FAQ; next.

## What do I need first?

- **UniFlash 9.6.0 and the SDK for your board, under `/Applications/ti`.** The LP-EM-CC2340R5 needs the SimpleLink Low Power F3 SDK; the LP-CC1354P10-1 needs the SimpleLink CC13xx CC26xx SDK. Each is a macOS installer from ti.com (`ti.com/tool/download/UNIFLASH`, `ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK`, `ti.com/tool/SIMPLELINK-CC13XX-CC26XX-SDK`); install each to `/Applications/ti`, where the script looks. Room needed while installing: about 2.2 GB for UniFlash and 4.8 GB for the F3 SDK ([mac-setup/README.md](../../mac-setup/README.md) has the disk table). Or run this repo's installer, which adds UniFlash and the F3 SDK along with CCStudio and the rest of the kit (about 20 GB in all; not the CC13xx CC26xx SDK): `./mac-setup/bootstrap.sh --all` from the repo folder, explained in [mac-setup/README.md](../../mac-setup/README.md).
- **The board plugged in by USB.** The LP-EM-CC2340R5 has no USB port of its own; it needs its separate probe. The connection file (`.ccxml`) for each board is in its folder,; its probe serial number is the one `python3 tools/frl/frl.py detect` prints.
- **Python 3.** Open Terminal and run `python3 --version`. If macOS offers to install the Command Line Tools, accept; it takes several minutes.

## Step 0: Open Terminal and go to the repo (Terminal and Finder)

Every command on this page runs from the repo folder. A copy of the repository must be on your Mac ([START.md](../../START.md) shows how to clone it). If you are reading this page from a folder on your Mac, that folder is it.

1. Press Command-Space, type Terminal, press Return. A window opens with a line waiting for you to type.
2. Type `cd` (the letters c and d, then a space) and do not press Return. In Finder, find the repo folder and drag it onto the Terminal window: its path appears after `cd`. Now press Return. The folder can be anywhere.
3. Check you are in the right folder. This prints the folder's path:

```text
pwd
```

This prints `tools/demo_flash.py`:

```bash
ls tools/demo_flash.py
```

If it prints `ls: tools/demo_flash.py: No such file or directory`, you are in another folder; do step 2 again.

4. Check Python 3 is there. This prints a version such as `Python 3.14.7`:

```bash
python3 --version
```

If it prints a line ending in `command not found: python3`, install Python 3 from python.org (its macOS installer), then close Terminal and open it again. If macOS instead offers to install the Command Line Tools, accept; that also gives you Python 3 and takes several minutes.

## Step 1: What demos can I flash? (Terminal)

```bash
python3 tools/demo_flash.py list LP_EM_CC2340R5
```

The script names the boards `LP_EM_CC2340R5` and `LP_EM_CC1354P10_1`, with underscores; type them exactly, and `python3 tools/demo_flash.py list` with no board name shows them all.

The list shows every prebuilt hex file TI ships for that board. Hex size is the size of the hex file on disk. A hex file stores each byte as two text characters plus addresses, so it is about twice the bytes it puts in flash. On 2026-10-07 the LP-EM-CC2340R5 menu was:

| Name | Hex size (kB) | Note |
|---|---|---|
| `basic_ble_app` | 533 | Bluetooth LE example |
| `data_stream_app` | 528 | Bluetooth LE data stream example |
| `host_test_app` | 680 | Bluetooth test image for a host computer |
| four `mcuboot_*` images | 33 to 42 | Boot and update images, refused unless you add `--allow-boot` |

The LP-CC1354P10-1 menu on 2026-10-07 had `project_zero_app` (508), `simple_peripheral_app` (480), `simple_central_app` (480) and `host_test_app` (580), plus boot and update images. The LP-MSPM0G3507 has none: it flashes through [OpenOCD](../vocab.md), which this tool does not drive.

## Step 2: What will it do to my board? (Terminal)

```bash
python3 tools/demo_flash.py flash LP_EM_CC2340R5 basic_ble_app
```

With no `--yes`, this prints the exact UniFlash command and what it would back up, and changes nothing. Read it. On 2026-10-07 it printed this (paths shortened):

```text
board   LP_EM_CC2340R5 (probe LS470GTU)
demo    basic_ble_app  533 kB hex from simplelink_lowpower_f3_sdk_9_21_00_36_LTS
backup  would read: 0x0 for 524288 bytes, 0x4e020000 for 2048 bytes
command /Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c .../cc2340r5/CC2340R5.ccxml -e -f -u .../prebuilt_hexfiles/basic_ble_app.hex

DRY RUN: nothing was read or written. Add --yes to back up, flash and print the restore command.
```

In that command, `-e` is `--verbose` (it prints progress; it does not erase), `-f` loads the file into flash, and `-u` runs it afterward. UniFlash's default settings for the CC23xx chips are believed to erase the whole chip during every load, so what is on the board now is gone unless it is backed up; that is not checked (the CC2340R5 FAQ entry "How do I make a TI Zigbee example forget my network?" says the same). The setting on the CC1354P10 is not checked either, so treat it the same way. That is why the following step backs it up.

## Step 3: Back up and flash (Terminal)

```bash
python3 tools/demo_flash.py flash LP_EM_CC2340R5 basic_ble_app --yes
```

The script reads the chip's flash and its configuration area to a new folder under `<scratch>/runs/demo-flash/`, outside the repo because the backup can hold network keys. It stops without flashing if a read fails, if a file is the wrong size, or if the backup looks blank. Then it flashes.

What the output should look like (from the script's code; not run): the same four lines as the dry run, then `backup ok in <folder>`, then the end of UniFlash's own log, then `flash ok in <seconds> s`, then `to put the board back:` and one long command. Success is `flash ok`. Copy the restore command somewhere safe: select it in Terminal with the mouse, press Command-C, and paste it into a text file with Command-V. Leave the board's backup folder where it is; the restore command points to it.

To check the demo runs, scan with a phone Bluetooth app such as nRF Connect or LightBlue. TI's source for `basic_ble_app` names it `Basic BLE project` (in `basic_ble.syscfg`), and it advertises the 16-bit service `0xFFF0`; not checked: that the prebuilt file uses the same name. On the LP-CC1354P10-1, `project_zero_app` advertises as `Project Zero`. Bluetooth examples advertise; they do not blink.

The backup reads use UniFlash's `memory --range`, whose length counts 32-bit words, not bytes. The script divides by four and checks the file size, so a unit slip is caught.

## What can go wrong?

- **The connection fails.** **Setup or handling.** Check the probe's serial number in the `.ccxml`, press the board into its probe fully, and try another USB port. The CC2340R5 [FAQ](../../cc2340r5/FAQ.md) lists each UniFlash error.
- **Error -242, `A router subpath could not be accessed`, on the LP-CC1354P10-1.** **Unknown (suspected: Chip limit).** UniFlash allowed one connection per power-up while a Bluetooth program was running on this board. Unplug and replug the board before the backup, and again before the flash if it fails. Not run through this script.
- **A demo you flash replaces what was running.** **Setup or handling.** On a board that runs something you care about, such as a Zigbee light, keep the restore command and run it when you are done. The restore has not been run.
- **The demo does nothing you can see.** **TI example default.** Bluetooth examples advertise; they do not blink. Use a phone Bluetooth scanner, and read the example's own README in the SDK.

## FAQ

### The script says "REFUSED: ... is a boot manager or update image"

Leave the `mcuboot_*` images alone unless you are setting up over-the-air updates; if you are, add `--allow-boot`. The script refuses them on purpose with `REFUSED: <name> is a boot manager or update image. It can replace the part of flash that decides what boots. Add --allow-boot if you mean it.` (seen 2026-10-07 in a dry run).

### The script stops with "REFUSED: backup looks blank. Nothing was flashed."

The chip read back as empty, so there was nothing to back up. Not seen. A new or erased board reads this way, and so could a read that failed without an error. If the board really is empty, run the UniFlash command the dry run printed yourself; there is nothing to lose.

### The script stops with "REFUSED: memory read at 0x0 failed"

The full line ends `(wanted 524288 bytes, got ...)`. Fix the connection and run it again; nothing was flashed. The backup file came out the wrong size, which usually means the probe lost the board during the read. Not seen with this script.

### Flashing the LP-CC1354P10-1 fails with error -242, "A router subpath could not be accessed"

Unplug the board's USB cable, plug it back in, and run the command again. While a Bluetooth program runs, the debugger reached this chip once per power-up in the tests; a pin reset through the probe did not help, and a different probe was not run. More in [cc1354p10/FAQ.md](../../cc1354p10/FAQ.md) and [tools/gatt_gen/README.md](../../tools/gatt_gen/README.md).

### How do I open Terminal and get to the repo folder?

Press Command-Space, type Terminal, press Return, type `cd` with a space, drag the repo folder from Finder onto the window, and press Return. Every command on this page assumes you are in that folder; from anywhere else the script is not found. `ls tools/demo_flash.py` checks it. Step 0 above has the details.

### Will flashing a demo erase what is on my board now?

Assume yes, and let the script back the board up first. UniFlash's default settings for the CC23xx chips are believed to erase the whole chip during every load, but that is not checked. The setting on the CC1354P10 was not checked either. The backup and the restore command are there so nothing is lost.

### How do I put the original firmware back?

Run the `to put the board back:` command the script printed after the flash; it writes the backup to the board. The restore has not been run. To put TI's own demo back instead, flash `basic_ble_app` with this script; the CC2340R5 FAQ entry "How do I put TI's own Bluetooth demo back on the board?" has the details.

### How long does flashing a prebuilt demo take?

Not measured. The estimate is about 10 minutes hands-on once UniFlash and the SDK are installed, most of it reading the output; the installs themselves take longer (see [mac-setup/README.md](../../mac-setup/README.md)).

### Can I use Web Bluetooth on my phone to see the demo?

On Android, Chrome has Web Bluetooth; on an iPhone, no browser does. MDN's compatibility data lists Safari, including Safari on iPhone, and Firefox as not supporting it (checked 2026-10-07). To see a demo on an iPhone, use a scanner app such as nRF Connect or LightBlue. The test page is ports/ecosystem/webble/README.md.

### I flashed a demo and nothing blinks

That is expected for the Bluetooth demos: they advertise, they do not blink. To see one, scan with a phone app such as nRF Connect or LightBlue. `basic_ble_app` advertises the name "Basic BLE project" and the 16-bit service UUID 0xFFF0, both set in TI's `basic_ble.syscfg` (F3 SDK 9.21.00.36 LTS). Not run on this board with this script.

### My Mac shows the old services after I flash a new demo

Restart Bluetooth on the Mac, or forget the device on a phone. macOS keeps a board's list of services per Bluetooth address and does not reread it after a reflash.

## Where do I go next?

Step 3 is the next move: run it on a board and time it. To change a demo you have to build it: [how to blink an LED on an LP-EM-CC2340R5 from a Mac](../../cc2340r5/README.md).
