# Press the LaunchPad kit's two buttons and a Mac sees a game controller

An LP-EM-CC2340R5 advertises as a Bluetooth LE gamepad. Its two push buttons, BTN-1 and BTN-2, are gamepad buttons 1 and 2. HID is the report format Bluetooth keyboards and gamepads use. It is built from TI's basic_ble example and the report code of the ESP32-BLE-Gamepad library.

The page: [site/project-ble-gamepad.html](../../site/project-ble-gamepad.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Two buttons as gamepad buttons 1 and 2, paired to a Mac

One LaunchPad, its debug probe and a computer with Bluetooth LE. Tested on a Mac.

Status: Works. Boards: cc2340r5.

Folders this version uses (in a sparse clone, fetch them first, then read `ports/esp32-ble-gamepad/AGENTS.md`):

```bash
git sparse-checkout add ports/esp32-ble-gamepad cc2340r5 mac-setup site/bench
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-EM-CC2340R5 LaunchPad development kit | 1 | the gamepad: its radio sends the reports and BTN-1 and BTN-2 are the buttons |
| LP-XDS110ET debugger | 1 | powers and programs the board (an LP-XDS110 also works) |
| A computer with Bluetooth LE | 1 | pairs with the gamepad, builds the image and runs the reader page; tested on a Mac |

### Before you start

- A Mac with the SimpleLink Low Power F3 SDK 9.21.00.36, TI Arm Clang 5.1.1.LTS and SysConfig 1.28.0 (`mac-setup/README.md`). The build script copies TI's basic_ble example into a work folder and patches the copy; the SDK is never changed. `rsync` and `patch` are used by the script and ship with macOS.
- UniFlash 9.6.0 for `dslite.sh`, and your probe's serial in `cc2340r5/CC2340R5.ccxml` (`xdsdfu -e` lists it).
- The code is in this repository: `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git`, then `cd fox-red-labs` and `git sparse-checkout add ports/esp32-ble-gamepad cc2340r5 mac-setup site/bench`. Every command on this page runs from that folder unless it says otherwise.

### Steps

1. **Mac, terminal.** Build the image with the two buttons switched on. `LIVE=1` names the board `CC2340R5 Gamepad` and sends BTN-1 and BTN-2 as buttons 1 and 2. Without it the build sends the library's fixed test pattern as `ESP32 BLE Gamepad`. The script only builds; it never flashes.

   ```text
   LIVE=1 bash ports/esp32-ble-gamepad/boards/cc2340r5/build.sh
   ```

   You should see: The last line is `built: .../basic_ble.hex (NOT flashed)`.

   If `make` stops at once with `sysconfig_cli.sh: not found`, name your folders on the same line, for example `SDK=<sdk folder> SYSCFG=<sysconfig_cli.sh> CLANG=<ti-cgt-armllvm folder> LIVE=1 bash ports/esp32-ble-gamepad/boards/cc2340r5/build.sh`.

2. **Mac, terminal.** Clip the probe onto the board, plug it into the Mac, and flash the `.hex` from the `cc2340r5` folder (`cd cc2340r5`). Leave out `-v`: on CC23xx, verify reads the boot configuration area and reports `Values at address 0x4E020750 do not match`. Do not flash the `.bin`. SWD is the debug connection the probe uses. This command is assembled from the board's flash line and this build's output path.

   ```text
   /Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c CC2340R5.ccxml -f -u ../ports/esp32-ble-gamepad/boards/cc2340r5/build/freertos/ticlang/basic_ble.hex
   ```

   You should see: UniFlash reports success and the board restarts as a Bluetooth gamepad (flash time not recorded for this image).

   If you see `Error -615` or `The target failed to see a correctly formatted SWD header`, press the probe firmly onto the board's debug pins and replug USB. If you see `Values at address 0x4E020750 do not match`, you flashed the `.bin` or used `-v`; flash the `.hex` without `-v`.

3. **Computer, Bluetooth settings.** On a Mac, open System Settings, Bluetooth, and press Connect beside `CC2340R5 Gamepad`. Accept the pairing request when it appears. Then open System Settings, Game Controllers.

   You should see: Bluetooth shows `CC2340R5 Gamepad` as Connected, and Game Controllers lists it with a green Connected status, as in the picture, with no driver or app installed.

   If the connect hangs about 30 seconds and then drops, the pairing was not accepted: macOS waits out a 30 s timeout and passes no input. Remove the device and connect again, and accept the request this time.

4. **Browser (Chrome or Edge).** In a new terminal at the repository root, serve the site folder and open the reader page (`site/bench/gamepad.html`) in Chrome or Edge. The browser hands a gamepad to a page only after a button press, so press BTN-1 once with the page open. No gamepad? The page's Replay the capture button plays the Mac's recorded run.

   ```text
   python3 -m http.server 8000 --directory site
   open -a "Google Chrome" http://localhost:8000/bench/gamepad.html
   ```

   You should see: The page shows a drawn controller and, once it sees the gamepad, its name. [The reader page](../../site/bench/gamepad.html) lists every change it sees.

   If the page says `No gamepad yet`, press a button once with the page in front: the browser reveals a gamepad only after input. Use Chrome or Edge.

5. **LaunchPad.** Press BTN-1, then BTN-2, then both together.

   You should see: Checked by eye: button 1 lit red in the reader while BTN-1 was down, button 2 while BTN-2 was down, and both for both. A HID report logger on the Mac received reports starting `03 01`, `03 02` and `03 03` for these three states and `03 00` on release (`03` is the report ID, the second byte the button bitmask).

   If nothing lights, check the board shows as Connected in Bluetooth settings, then press a button once with the reader in front.

6. **Mac, terminal.** This change is untested. Ask your assistant to change which gamepad button BTN-2 sends, then run steps 1 and 2 again. Nothing else in the build changes.

   ```text
   In ports/esp32-ble-gamepad/boards/cc2340r5/board/hid_gamepad.c, change the LIVE build so BTN-2 sends gamepad button 3 instead of button 2. Then rebuild it with LIVE=1 and tell me the flash command, but do not flash the board.
   ```

   You should see: After you flash and pair again, BTN-2 lights button 3 in the reader instead of button 2.

   If the Mac still shows button 2, it is running the old image: confirm the flash finished and remove and pair the device again.

### Expected result

After flashing and accepting the pairing request, a Mac lists the LaunchPad under Game Controllers and receives BTN-1 and BTN-2 as buttons 1 and 2 of a gamepad.

### What this shows

- A LaunchPad can be a Bluetooth LE game controller: 15 of 15 button changes arrived at a Mac as HID input reports, and macOS lists the board under Game Controllers with no driver.
- The report bytes are the ESP32-BLE-Gamepad library's: the default build's 96-byte descriptor (the report layout the gamepad announces to the host) and four test reports match the library's output on an ESP32-C6, 5 of 5 lines; the LIVE build was not re-run against it.

### Limits

- Latency and battery life were not measured.
- Only the two buttons are live; the report also has joystick axes and a directional pad (hat) that nothing moves.
- Pairing has to be accepted once, or macOS waits a 30 s timeout and receives no input.
- The reader page's live view was checked by eye once, in Chrome, and not recorded.
- Only macOS 15.7.9 on one Intel Mac was tried. Windows, Linux, Android and iOS are not tested, and the reader page needs Chrome or Edge.
- Whether pairing survives a reflash was not tested. The step 6 change is untested.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Button changes received by the Mac | 15 of 15, reports `03 01`, `03 02`, `03 03`, `03 00` |  |
| Report bytes against the ESP32-C6 library output | descriptor and 4 test reports, 5 of 5 lines identical | `docs/experiments/2026-10-19-esp32-ble-gamepad-port-matrix.md` |
| Image size (default build; the LIVE build's size was not recorded) | 189.4 kB flash; 33.8 kB of 36 kB static RAM | `ports/esp32-ble-gamepad/README.md` |
| Button read interval | 20 ms, one notification per change | `ports/esp32-ble-gamepad/boards/cc2340r5/board/hid_gamepad.c` |
| Software | SimpleLink Low Power F3 SDK 9.21.00.36, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0, Intel Mac, macOS 15.7.9 | `ports/esp32-ble-gamepad/README.md` |
