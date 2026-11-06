# How do I update a TI LP-EM-CC2340R5 over Bluetooth from a Mac terminal?

Yes for the Bluetooth name and version; the red LED was not watched. The update ran once on 2026-10-07: 31.4 s transfer, 7.8 s to the new name, version 2.0.0 read back. One run.

That is about 41 s from scan to new name (the sum of the saved timings: scan 0.84 s, connect 0.99 s, transfer 31.44 s, enable to new name 7.78 s), and 43 s including the version read, which the run's JSON output does not log separately. Use TI's dual-image OAD example with TI's prebuilt MCUboot bootloader, and send the new image with [`tools/oad/oad_client.py`](../../tools/oad/oad_client.py), a Python script on the bleak Bluetooth library that speaks TI's OAD protocol. Both images build from the terminal in about two minutes, and a dry run checks the image header, SHA-256, signature and the chip's lock fields with no board.

Terms: OAD, dual image, MCUboot, SHA-256 and ECDSA signature, ATT MTU, bleak, imgtool, 0xFF padding, CCFG and CCFG checksums, OTA and the XDS110 probe are defined in [docs/vocab.md](../../docs/vocab.md). Errors and fixes: [FAQ.md](../FAQ.md). 1 kB = 1,024 bytes.

Time to repeat, once the tools in [mac-setup/README.md](../../mac-setup/README.md) are installed: about 2 minutes to build, about 13 s to flash, under a minute for the update, plus the backup and restore reads (not timed).

OAD (over-the-air download) is TI's name for sending new firmware to a running Bluetooth device. TI documents it with a phone app (SimpleLink Connect) or a Windows tool; this page does it from a Mac terminal.

Contents: [why dual image](#which-oad-flavour-and-why), [what you need](#what-you-need), [build](#build-two-minutes), [the files](#the-files), [dry run](#check-the-image-without-a-board), [expected and measured](#what-was-expected-and-what-was-measured), [on the board](#on-the-board), [the protocol](#the-protocol-as-the-sdk-source-implements-it), [guide against source](#where-tis-guide-and-the-sdk-source-disagree), [can it lock the chip](#can-this-lock-the-chip), [not confirmed](#what-could-not-be-confirmed), [still open](#what-is-still-open).

Checked 2026-10-07 on an Intel MacBook, macOS 15.7.9: SimpleLink Low Power F3 SDK 9.21.00.36 LTS, TI Arm Clang 5.1.1 LTS, SysConfig 1.26.3, UniFlash 9.6.0, Python 3.14.7 with bleak 3.0.2.

## Which OAD flavour, and why

The F3 SDK has three single-image flavours for this board. The build uses **dual image** (`basic_ble_oad_dual_image`).

| Flavour | Where the new image waits | Runs during the download | Fits this board? |
|---|---|---|---|
| Off-chip | External SPI flash (a separate flash chip on the board, wired to the SPI bus) | Your app | No: the LP-EM-CC2340R5 has no external flash fitted |
| On-chip | Internal flash, after your app is erased | A small "persistent" app (`basic_persistent`), a second app that stays on the chip only to receive the new image | Yes, but your app is gone during the download and a second app must be built and flashed |
| Dual image | Internal flash, second half | Your app | Yes: your app keeps running, and only two files go on the chip |

Dual image halves the room for your app: each slot is 236 kB. TI's Bluetooth app uses 192 kB of it, so there is room. The fourth example, multi-step OAD, splits one large app into two halves; it needs a bootloader built with a different option and is a separate test.

## What you need

| Thing | Version used | Where |
|---|---|---|
| LP-EM-CC2340R5 board and an LP-XDS110ET (or LP-XDS110) probe | any serial | <https://www.ti.com/tool/LP-EM-CC2340R5> |
| SimpleLink Low Power F3 SDK, SysConfig, TI Arm Clang, UniFlash | as above | install steps in [mac-setup/README.md](../../mac-setup/README.md) |
| Python 3 with bleak, a Bluetooth library for Python | 3.14.7, bleak 3.0.2 | <https://pypi.org/project/bleak/> |
| A Mac with Bluetooth, and Bluetooth permission for your terminal app | macOS 15.7.9 | System Settings, Privacy and Security, Bluetooth |

Set up once. A Python environment with bleak (here in your home folder, outside the repo):

```bash
python3 -m venv ~/ble && ~/ble/bin/pip install bleak
```

The public key compiled into TI's MCUboot, which the client uses to check the signature. Change the SDK path if yours differs:

```bash
export KEYS=/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS/source/third_party/mcuboot/boot/ti/source/mcuboot_app/keys.c
```

## Build (two minutes)

From the repo root:

```bash
cc2340r5/oad/build.sh
```

The script copies TI's example twice into `build/` (git-ignored), applies [`app.patch`](app.patch), and builds each copy with `make` and TI's own post-build step, which signs the image with MCUboot's `imgtool`. The two copies differ in three ways:

| | v1 (flash with the probe) | v2 (send over the air) |
|---|---|---|
| Advertised name | `FRL OAD 1` | `FRL OAD 2` |
| LED | green, toggles every 1,000 ms | red, toggles every 250 ms |
| Signed version | 1.0.0 | 2.0.0 |

The name is one line in the example's `.syscfg`; the blink is a short patch to `app/app_main.c` (a timer that toggles one LED) that reads its LED and rate from a header the script writes. The build took 65 s for v1 and 49 s for v2, as `build.sh` printed them on 2026-10-07 (observed, not saved). TI's unchanged example took 43.6 s (`real 43.59`). Paths to the SDK and tools are at the top of `build.sh`; change them if yours differ.

## The files

From `images.txt`, which the build script writes with SHA-256 sums:

| File | Bytes | What it is |
|---|--:|---|
| `images/mcuboot_dual_image.hex` | 36,716 | TI's prebuilt MCUboot for dual image, copied from the SDK's `prebuilt_hexfiles`. 13.1 kB of code at 0x0, plus the CCFG |
| `images/frl_oad_v1.bin` | 241,664 | v1, flashed at 0x6000. Header 256 + app 196,564 + signature area 152 (196,564 is the image size field; the linker map's APP_SLOT shows 196,560 used), padded with 0xFF to the slot size |
| `images/frl_oad_v2.bin` | 241,664 | v2, the update. Real content 192.4 kB; 43.6 kB are padding |

Each slot is 0x3B000 bytes: the app slot runs from 0x6000 to 0x41000, the download slot from 0x41000 to 0x7C000, and the Bluetooth bond storage (where the board keeps the keys of devices it has paired with) sits at 0x7D000. MCUboot's own table of slots, read from the hex, says the same. RAM: 32,234 of 36 kB used (from the linker map).

The padding is sent too. TI's post-build adds MCUboot's 16-byte "upgrade waiting" marker at the very end of the slot, and TI's own phone app sends the whole padded file when that marker is there. So 18.5% of the bytes sent are 0xFF.

## Check the image without a board

```text
~/ble/bin/python tools/oad/oad_client.py --dry-run --keys-c $KEYS cc2340r5/oad/images/frl_oad_v2.bin
```

`--keys-c` points at the public key compiled into TI's MCUboot. Output on v2, trimmed:

```text
  img_size          196564
  version           2.0.0+0
  tlvs              ['SHA256 (32 B)', 'KEYHASH (32 B)', 'ECDSA_SIG (70 B)']
  sha256_ok         True
  keyhash_ok        True
  signature_ok      True
  trailer_magic     True
  send_bytes        241664
  identify payload  3db8f396000000000001000000b0030000000000020000000000000000000000
  transfer plan     1024 blocks of 236 data bytes (block size 240 with the 4-byte block number), last block 236 bytes, 245760 bytes written in total
dry run OK: the image parses, its hash and signature check out, and it fits the slot
```

`tlvs` is the list of tagged records MCUboot adds after the app, each a type, a length and a value: `SHA256` is the hash of header and app, `KEYHASH` is the SHA-256 of the public key that signed the image (so MCUboot can tell which key to check with), and `ECDSA_SIG` is the signature. MCUboot images carry no CRC: the check is a SHA-256 of header and app, and an ECDSA P-256 signature over it. The dry run recomputes both, verifies the signature with the key from `keys.c` (in plain Python, no extra packages), and confirms TI's prebuilt MCUboot contains that same key (`images.py haskey`). The check was tested on five broken copies of the file: one flipped bit in the app, in the signature, or in the version field each failed, and a wrong header magic failed. A file cut short after the signature area passed with a printed note that the upgrade marker is missing, which is the right answer: the image is valid, but MCUboot may not install it. All five were refused. TI's `oad_image_tool` and `computeCRC32.py` (in `tools/common/oad`) are for TI's older image format; no CC2340R5 example uses them.

The client's Bluetooth path was also run against a software stand-in for the board, [`tools/oad/sim_target.py`](../../tools/oad/sim_target.py), which follows the state machine in the SDK's `oad_profile.c`. All 1,024 blocks arrived in order, the stand-in's slot matched the file byte for byte, and it came back as `FRL OAD 2` version 2.0.0. With the packet size set to 185 the client stopped before sending a block. This tests the client's logic, not the radio or TI's code.

```text
~/ble/bin/python tools/oad/sim_target.py cc2340r5/oad/images/frl_oad_v2.bin
```

## What was expected, and what was measured

The Expected column was written before the board run. The Measured column is the one run on 2026-10-07; the source of each number is in the cell.

| Step | Expected | Where the expectation comes from | Measured |
|---|---|---|---|
| Flash MCUboot and v1 | about 10 to 20 s; green LED toggles every second; `FRL OAD 1` advertising | the 7 s blink flash in [../README.md](../README.md), with about 270 kB more to write; an estimate | 13 s for both files in one command (observed on the console, not saved). `FRL OAD 1` seen at -33 dBm in the first 15 s scan (observed, not saved) |
| Packet size the Mac agrees (ATT MTU, the largest data packet per write) | 243 bytes or more, needed for 240-byte blocks | the board allows 251 (`maxPDUSize = 255`); this Mac agreed 65 with an example capped at 65 | 251 (`att_mtu` in the client's JSON output) |
| Transfer, 1,024 blocks | about 46 s; 31 s to 2 minutes | this Mac's 45 ms median write-and-notify round trip, times 1,024 | **31.4 s**, 1,024 blocks of 236 data bytes, no retries |
| Speed | about 40 kbit/s of image data | 236 kB in 46 s | **61.5 kbit/s**: 236 kB in 31.4 s, packet size 251, connect 0.99 s after a 0.84 s scan |
| Enable to `FRL OAD 2` seen | under 15 s | MCUboot copies 236 kB and checks the signature; estimate, no measurement | **7.78 s** from the enable command to the new name advertising; enable status SUCCESS, the link dropped for the reset |
| Version read after the update | 2.0.0 | the signed header | **2.0.0** (raw `020000000000`); the header at 0x6000 reads byte 20 = `02` |
| LED after the update | red, toggling every 250 ms | the patch | not watched; the name, version and header say the new image runs, but the LED itself was not checked |

What could fail, as listed before the run: the Mac agrees a packet size under 243 (the client stops before sending anything, see the protocol section); the Mac's connection interval is longer than 15 ms (slower, not broken); MCUboot does not install the image (marker or version rule; the board then still runs v1); macOS shows the old cached name for the board (the client reads the name from the advertisement, not the cache). None of these happened on the one run: the packet size was 251, MCUboot installed v2, and the client saw the new name. The connection interval was not read.

## On the board

Run these from the repo root, one at a time. `DSLITE` is UniFlash's command-line program; the connection file picks the probe by serial number (put your probe's serial in the `.ccxml` file). `BK` is a backup folder outside the repo; any path works.

```bash
export DSLITE=/Applications/ti/uniflash_9.6.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite CCXML=cc2340r5/CC2340R5.ccxml BK=$HOME/cc2340r5-oad-backup && mkdir -p $BK
```

1. Back up the whole flash and the CCFG, outside the repo (the board may hold a Zigbee network key). DSLite's `memory -r <address>,<length>` takes the length as a count of 32-bit words, not bytes: 0x20000 is a count of 32-bit words; the file came out 512 kB, the whole 512 kB flash. The CCFG read below, 0x200 words, came out 2 kB. Check the file sizes after your own read.

```text
$DSLITE memory -c $CCXML -r 0x0,0x20000 -o $BK/main_flash.bin
```

```text
$DSLITE memory -c $CCXML -r 0x4E020000,0x200 -o $BK/ccfg.bin
```

```bash
python3 cc2340r5/oad/images.py merge $BK/restore.hex $BK/main_flash.bin@0x0 $BK/ccfg.bin@0x4E020000
```

```bash
python3 cc2340r5/oad/images.py ccfg $BK/restore.hex
```

2. Flash MCUboot and v1 in one command. UniFlash's default for CC23xx chips erases the whole chip at the start of every program load ("Erase Settings during Program Load", default Chip Erase, in `CC23xx_FlashProperties.xml`; see the FAQ), so both files go in the same load. In the command, `-e` is `--verbose` (progress messages), `-f` loads the files to flash, and `-u` runs the program afterwards.

```text
$DSLITE flash -c $CCXML -e -f -u cc2340r5/oad/images/mcuboot_dual_image.hex cc2340r5/oad/images/frl_oad_v1.bin,0x6000
```

Check: the green LED toggles once a second. If the app is flashed without MCUboot, the probe reports error -615 when it tries to run it, as TI's example README describes.

3. Send v2 over the air and check it comes back:

```text
~/ble/bin/python tools/oad/oad_client.py --name "FRL OAD 1" --expect-name "FRL OAD 2" --keys-c $KEYS --json oad-run.json cc2340r5/oad/images/frl_oad_v2.bin
```

It prints the running version, the block size, progress every 5% with kbit/s, the download time, the time from "enable" to the new name, and the new version, and writes the numbers to `oad-run.json`. Then look at the board: red LED, four toggles a second (not watched on the run).

4. Optional second proof, by probe: the image header at 0x6000 holds the version at byte 20. The length 0x8 is 8 words, so the file is 32 bytes.

```text
$DSLITE memory -c $CCXML -r 0x6000,0x8 -o $BK/hdr.bin && xxd $BK/hdr.bin
```

Byte 20 reads `02` after the update and `01` before.

5. Put the board back. The same default chip erase clears the flash first, then the backup is written:

```text
$DSLITE flash -c $CCXML -e -f -u $BK/restore.hex
```

## The protocol, as the SDK source implements it

Service `F000FFC0-0451-4000-B000-000000000000`, on TI's 128-bit base `F000xxxx-0451-4000-B000-000000000000`:

| Characteristic | UUID (short) | Direction | Use |
|---|---|---|---|
| Image identify | 0xFFC1 | write with response; notify | the image header; 1-byte accept or refuse |
| Image block | 0xFFC2 | write | 4-byte block number, then 236 bytes of the file |
| Control | 0xFFC5 | write without response; notify | commands, replies and block requests |

The sequence the client runs: turn on notifications for 0xFFC1 and 0xFFC5; Get Software Version (`07`); Get Block Size (`01`, reply `01 F0 00` = 240); write the 32-byte image header to 0xFFC1 with its size field set to the bytes being sent; Start (`03`); answer each block request `12 <status> <block number>` with that block; on status 14 (download complete) send Enable Image (`04`); the board replies `04 00`, drops the link, resets, and MCUboot installs the image. Sources: `source/ti/ble/profiles/oad/src/oad_profile.c`, `source/ti/ble/services/oad/src/oad_service.c` and `source/ti/ble/app_util/sw_update/src/sw_update.c` in the SDK, and TI's open-source SimpleLink Connect app (`src/components/FWUpdate/FWUpdate_Modal.tsx` on github.com/TexasInstruments/simplelink-connect).

The block size is fixed at 240 in the source, and the board ignores the offset of a long write, so every block must fit in one write. The client checks the agreed packet size first and stops if a 240-byte block would not fit, with this message from [`tools/oad/oad_client.py`](../../tools/oad/oad_client.py) (the numbers in angle brackets are filled in from the connection):

`ATT MTU <mtu> carries at most <mtu - 3> bytes a write, the target wants 240. The target ignores the offset of a long write, so this would corrupt the image. Stopping.`

## Where TI's guide and the SDK source disagree

The guide is the BLE5-Stack User's Guide in the SDK (`docs/ble5stack/ble_user_guide`, OAD chapter). Where they disagree, the client follows the source. Each row is **Tool or docs**.

| Topic | Guide says | Source does |
|---|---|---|
| Block size | chosen from the agreed packet size, 20 to 244 | always 240 (`OAD_DEFAULT_BLOCK_SIZE`) |
| Where block requests arrive | notifications on 0xFFC2 | notifications on 0xFFC5; 0xFFC2 never notifies |
| Enable Image `04` | not in the command table | required after the download, or nothing installs |
| Download complete | block number 0xFFFFFFFF | the block count (1,024 here) |
| Commands before image identify | answered with AUTH_FAIL | block size, version and status answered any time; start, enable and cancel get NOT_STARTED |
| Software version reply | 5 bytes | 7 bytes: major, minor, revision (2), build (2) |
| Bytes to send | the header's image size | `ih_img_size` leaves out the header and signature, so a client must replace it; TI's app does, the guide does not say so |
| Header table | `ih_flags` is "total length", `_pad1` is 2 bytes | MCUboot flags; `_pad1` is 4 bytes |

## Can this lock the chip?

Not from these images, as far as the files show. The CCFG was not read back from the chip after the update (not done). **Setup or handling:** the lock fields are set by the CCFG of whatever you flash. The CCFG that reaches the chip here is MCUboot's (the app `.bin` files have none), and in it `allowDebugPort` and `allowChipErase` are both allow (0xA), debug authorization is open (0x5A), and no flash sectors are write-protected; the two app builds' own CCFGs say the same (from `images.txt`). The update itself writes only to the download slot (0x41000 to 0x7BFFF, enforced by the flash driver's region), and MCUboot writes only the app slot, so neither can change the CCFG.

One oddity: TI's prebuilt MCUboot hex has all four CCFG checksums set to zero. **Tool or docs.** UniFlash's default for this chip family rewrites those checksums from the image while flashing (setting "Overwrite CRC fields in CCFG", default on, in `CC23xx_FlashProperties.xml`), so flash it with UniFlash defaults and do not turn that setting off. This was seen on 2026-10-07 flashing TI's prebuilt hex: UniFlash logged `Updating CRC32 field ccfg.crc32 @ address 0x4E02074C ... Value changes from 0x00000000 to 0xB714DFB7`. It rewrote three of the four fields; the log has no line for the user record. The same settings file shows UniFlash skips the CCFG user record by default, which may explain the verify mismatch at 0x4E020750 described in gotcha 8 of the CC2340R5 start-here page ([../README.md](../README.md)); not tested.

## What could not be confirmed

- **Unknown:** which upgrade mode TI's prebuilt MCUboot uses (overwrite or swap) and whether it refuses older versions. No configuration file ships beside the hex. Suspected overwrite, because the app is linked for the first slot only and TI's post-build adds the upgrade marker. v2 is signed 2.0.0 against v1's 1.0.0, so a version rule would not block this test.
- **Unknown:** the connection interval macOS agrees with this board; the client does not read it. The packet size was measured: 251 bytes.
- **Unknown (suspected: Tool or docs):** the linker gives the app slot 0x3B700 bytes from 0x6100, which runs 2 kB past the 0x41000 start of the download slot. `imgtool` would refuse an image too large for the slot, so a build cannot silently overlap; this app uses 192 kB.
- Not tested: writing blocks without response (`--no-response`), which the guide recommends for speed and TI's app does not use.
- One run only. The red LED was not watched. Only this Intel Mac was used.

## What is still open

1. Run the update twice more, for a median of three.
2. Test `--no-response` (blocks written without response) against the same image.
3. Watch the LED after the update: red, four toggles a second.
4. Run the same steps on an Apple Silicon Mac.
