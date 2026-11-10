# Update a board's firmware by radio

A CC2340R5 took a signed update over Bluetooth from a Mac in 43 s; a CC35X1 installed one over Wi-Fi in 8 s. The Zigbee update from Home Assistant has not finished.

The page: [site/project-ota-update.html](../../site/project-ota-update.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Update a CC2340R5 over Bluetooth from a Mac terminal

TI's dual-image OAD example with TI's MCUboot; a Python script sends the signed image. 43 s from scan to the version read back, one run.

Status: Works. Boards: cc2340r5.

Folders this version uses (in a sparse clone, fetch them first, then read `cc2340r5/AGENTS.md`):

```bash
git sparse-checkout add cc2340r5 tools mac-setup
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-EM-CC2340R5 LaunchPad development kit | 1 | the board that gets updated |
| LP-XDS110ET debugger | 1 | flashes the first image and reads the backup |

### Before you start

- A Mac with Bluetooth, and Bluetooth permission for your terminal app (System Settings, Privacy and Security, Bluetooth). Checked on macOS 15.7.9.
- SimpleLink Low Power F3 SDK 9.21.00.36 LTS, SysConfig 1.26.3, TI Arm Clang 5.1.1 LTS and UniFlash 9.6.0; install steps in mac-setup/README.md.
- Python 3 with bleak (steps below). The code is in this repository: `cc2340r5/oad/` and `tools/oad/`.
- Your probe's serial number goes in `cc2340r5/CC2340R5.ccxml`.

### Steps

1. **Terminal (Mac).** Make a Python environment with bleak, then set the path of the public key compiled into TI's MCUboot (change the SDK path if yours differs). The client uses the key to check the signature.

   ```text
   python3 -m venv ~/ble && ~/ble/bin/pip install bleak
   export KEYS=/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS/source/third_party/mcuboot/boot/ti/source/mcuboot_app/keys.c
   ```

   You should see: bleak installs without errors.

   If the build script cannot find Python, it looks for `$HOME/.venvs/ble/bin/python`; set `PY` to your environment's python when you run it.

2. **Terminal (Mac).** From the repository root. The script builds two copies of TI's `basic_ble_oad_dual_image` example and signs them: v1 is named `FRL OAD 1` and toggles the green LED every second, v2 is `FRL OAD 2` and toggles the red LED four times a second.

   ```text
   cc2340r5/oad/build.sh
   ```

   You should see: About two minutes: 'v1 built in 65 s', 'v2 built in 49 s'. `cc2340r5/oad/images/` holds `mcuboot_dual_image.hex`, `frl_oad_v1.bin` and `frl_oad_v2.bin` (236 kB each).

   If it stops on a missing path, the SDK, SysConfig and compiler paths are at the top of `build.sh`; change them to your install.

3. **Terminal (Mac).** A dry run parses the image header, recomputes the SHA-256 and checks the signature against TI's key.

   ```text
   ~/ble/bin/python tools/oad/oad_client.py --dry-run --keys-c $KEYS cc2340r5/oad/images/frl_oad_v2.bin
   ```

   You should see: `sha256_ok True`, `signature_ok True`, `version 2.0.0+0`, a transfer plan of 1024 blocks of 236 data bytes, then `dry run OK`.

   If a line says False, rebuild; the check was tested on five deliberately broken copies and refused all of them.

4. **Terminal (Mac).** Before writing, read the whole flash and the boot configuration to a folder outside the repo (the board may hold a Zigbee network key). DSLite's `memory -r` takes the length as a count of 32-bit words, not bytes. Then merge them into a restore file.

   ```text
   export DSLITE=/Applications/ti/uniflash_9.6.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite CCXML=cc2340r5/CC2340R5.ccxml BK=$HOME/cc2340r5-oad-backup && mkdir -p $BK
   $DSLITE memory -c $CCXML -r 0x0,0x20000 -o $BK/main_flash.bin
   $DSLITE memory -c $CCXML -r 0x4E020000,0x200 -o $BK/ccfg.bin
   python3 cc2340r5/oad/images.py merge $BK/restore.hex $BK/main_flash.bin@0x0 $BK/ccfg.bin@0x4E020000
   ```

   You should see: `main_flash.bin` is 512 kB and `ccfg.bin` is 2 kB.

   If `main_flash.bin` is not 512 kB, you gave the length in bytes; use 0x20000 words.

5. **Terminal (Mac).** One command writes both files, because UniFlash erases the whole chip at the start of every program load.

   ```text
   $DSLITE flash -c $CCXML -e -f -u cc2340r5/oad/images/mcuboot_dual_image.hex cc2340r5/oad/images/frl_oad_v1.bin,0x6000
   ```

   You should see: About 13 s. The green LED toggles once a second and `FRL OAD 1` is advertising.

   If the probe reports error -615 when it runs the app, MCUboot was not flashed with it; run the same command again with both files.

6. **Terminal (Mac).** The client finds the board by name, sends the 1,024 blocks, sends the enable command and waits for the new name.

   ```text
   ~/ble/bin/python tools/oad/oad_client.py --name "FRL OAD 1" --expect-name "FRL OAD 2" --keys-c $KEYS --json oad-run.json cc2340r5/oad/images/frl_oad_v2.bin
   ```

   You should see: The running version, progress every 5% with kbit/s, then the download time, the time from enable to the new name and the new version. Recorded run: 31.4 s, 7.8 s, version 2.0.0, packet size 251.

   If it stops before sending with 'ATT MTU ... would corrupt the image', the Mac agreed a packet size under 243; try again, or see the protocol section of `cc2340r5/oad/README.md`.

7. **Terminal (Mac).** Read the image header at 0x6000; the version is byte 20.

   ```text
   $DSLITE memory -c $CCXML -r 0x6000,0x8 -o $BK/hdr.bin && xxd $BK/hdr.bin
   ```

   You should see: Byte 20 reads 02 after the update and 01 before. The red LED should now toggle four times a second (not watched on the recorded run).

   If it still reads 01, MCUboot did not install the image; it refuses an image that is not newer than the running one.

8. **Terminal (Mac).** The same chip erase clears the flash first, then the backup is written.

   ```text
   $DSLITE flash -c $CCXML -e -f -u $BK/restore.hex
   ```

   You should see: The board is as it was before step 5.

   If it does not start, read `cc2340r5/FAQ.md` before writing again.

### Expected result

The client prints a clean transfer of 1,024 blocks with no retries, then the board resets and advertises `FRL OAD 2`, and the version read back is 2.0.0.

### What this shows

- A CC2340R5 can take a signed firmware update over Bluetooth from a Mac terminal: 31.4 s transfer, 7.8 s to the new name, one run.
- TI's dual-image example keeps your app running during the download; each slot is 236 kB and the app uses 192 kB.
- The image check (SHA-256 and signature) runs on the Mac before any radio is used.

### Limits

- A repeat: one run, one Mac.
- That the red LED toggles: the name, version and header say v2 runs, but the LED was not watched.
- A Zigbee update from Home Assistant: that did not finish (see the Zigbee version below).
- Wi-Fi: the CC2340R5 has none.
- TI documents OAD with a phone app or a Windows tool; the Mac terminal client is this repository's own, written from the SDK source.
- The dual-image layout halves the room for the app (236 kB per slot).
- Checked on one Intel Mac, macOS 15.7.9.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Speed | 61.5 kbit/s of image data: 236 kB in 31.4 s, ATT MTU 251 | `cc2340r5/oad/README.md` |
| Image | 236 kB sent, 18.5% of it 0xFF padding | `cc2340r5/oad/README.md` |

## Update a CC35X1 over Wi-Fi from a Mac

TI's ota_example downloads a signed image from an HTTPS server on the Mac. 8 s to install on the first run, one attempt.

Status: Works. Boards: cc35x1.

Folders this version uses (in a sparse clone, fetch them first, then read `cc35x1/AGENTS.md`):

```bash
git sparse-checkout add cc35x1 site tools
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-EM-CC35X1 LaunchPad development kit | 1 | the board that gets updated |

### Before you start

- A Mac and the board on the same Wi-Fi network. SimpleLink Wi-Fi SDK 10.20.00.39, Wi-Fi Toolbox 4.3.23, SysConfig 1.28.0, TI Arm Clang 5.1.1.LTS, and Node.js for the driver.
- The chip must already be activated (a one-time step that burns a key into its fuses) and the Toolbox must work; see cc35x1/AGENTS.md before programming anything.
- The code is in this repository: `cc35x1/ota/` and `site/flash/cc35-ota.js`.

### Steps

1. **Terminal (Mac).** The script copies TI's `ota_example`, puts the version in its banner, builds it and signs it with the Toolbox. The default output folder is `out/cc35x1-ota`.

   ```text
   cc35x1/ota/build.sh 0.0.1.0
   ```

   You should see: It prints the build folder and lists `primary_vendor_image.sign.bin` and `tool_settings.json`.

   If it prints 'build failed' or 'signing failed', read the log it names. The SDK, Toolbox and compiler paths are at the top of `build.sh`.

2. **Terminal (Mac).** This puts the example on the two-slot flash layout the update needs. Use absolute paths inside `tool_settings.json`. The Toolbox does not reset the board afterward.

   ```text
   cd /Applications/ti/simplelink_wifi_toolbox_macos_4_3_23 && ./simplelink-wifi-toolbox programmer -i XDS110 -param1 <probe serial> programming --tool_settings <absolute path to out/cc35x1-ota/v0.0.1.0/freertos/ticlang/toolbox/tool_settings.json>
   xds110reset -s <probe serial>
   ```

   You should see: After the reset, the serial port (115200) shows `CC35xx OTA Example Application FRL 0.0.1.0`.

   If the Toolbox refuses, the chip may not be activated; stop and follow cc35x1/AGENTS.md. This chip cannot be read back, so keep the signed bundle of the image it ran before.

3. **Terminal (Mac).** The server lists a file as new only when its header version is higher than the running one. Copy the signed file into a folder under the name the server expects, with TI's server script beside it.

   ```text
   cc35x1/ota/build.sh 0.0.2.0
   ```

   You should see: The same listing, this time under `v0.0.2.0`. Copy its `primary_vendor_image.sign.bin` to `vendor_image_0.0.2.0.bin` in a new folder.

   If the server later shows nothing as NEW, check the version is higher and the file is named `vendor_image_<version>.bin`.

4. **Terminal (Mac).** Use TI's own server and playground certificates from the Wi-Fi SDK's `tools/ota_https_server/` folder (copy the folder, or run it in place). With Python 3, run its `ota_server.py` with `--dir <your folder>` from step 3. Find your Mac's Wi-Fi address.

   You should see: The server listens on port 8443 of your Mac's Wi-Fi address.

   If the board cannot reach it later (`Server query failed (HTTPS: -11 ...)`), check the address and that the Mac and board are on the same network.

5. **Terminal (Mac).** The bridge relays the probe's serial port to a local TCP port. Run it under the lab's port lock.

   ```text
   python3 tools/frl/lock.py exec --serial <probe serial> -- python3 cc35x1/ota/bridge.py /dev/cu.usbmodem<probe serial>1 7701 &
   ```

   You should see: The command returns to the prompt with the bridge running in the background.

   If the port is busy, close any serial monitor that has it open.

6. **Terminal (Mac).** The driver walks the example's menu: scan, join your network, query the server, download, install, reboot into the trial image, then keep or go back. Your Wi-Fi name and password come from the environment only. Use `--end keep` to accept the new image or `--end revert` to go back.

   ```text
   FRL_WIFI_SSID=<network name> FRL_WIFI_PASSWORD=<password> node cc35x1/ota/drive.mjs --server https://<Mac Wi-Fi address>:8443 --end keep
   ```

   You should see: Lines for each phase ending '== download', '== install', '== reboot into trial' and 'after: 0.0.2.0'. In the 2026-10-22 runs: join 2.2 to 2.7 s, download 3.6 s, install under 0.1 s, restart 3.1 s.

   If the menu says 'Invalid input', the driver sent a line feed; answers end with a carriage return only. If a slot shows FAILED the board erases it first (download took 20.5 s).

### Expected result

The driver prints each phase with its time, the board reboots into version 0.0.2.0, and the slot table after shows the new version active.

### What this shows

- A CC35X1 downloads, verifies, installs and boots a new signed image over Wi-Fi from a Mac server: install and reboot in 8 s on the 2026-10-09 attempt.
- A reset without accepting rolls the board back to the OTA app, as TI documents.
- Four rounds on 2026-10-22 (update, go back, update and keep, update into the other slot) all ended as expected.

### Limits

- A repeat of the 8 s figure: it is one attempt with a 27.7 kB image.
- Updates from Home Assistant or the internet: the server was a Mac on the home network.
- Safety against power loss mid-update: not tested.
- The chip must be activated first, which burns fuses and cannot be undone.
- The chip cannot be read back, so a restore needs a saved signed bundle.
- Checked on one Intel Mac and one board.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Install | download about 1 s, install and reboot 8 s, 27.7 kB image, one attempt | `data/results/cc35x1.csv` |
| Phases (1.29 MB image) | scan 1.9 s, join 2.2 to 2.7 s, download 3.6 s, restart 3.1 s | `cc35x1/AGENTS.md` |
