# cc2340r5: a Bluetooth game controller from a LaunchPad

**What it is.** The LP-EM-CC2340R5 advertises as a Bluetooth LE gamepad. Its two push buttons are gamepad buttons 1 and 2. A Mac lists it under Game Controllers with no driver. Build steps: [docs/builds/ble-gamepad.md](https://github.com/Fox-Red-Labs/fox-red-labs/blob/main/docs/builds/ble-gamepad.md). The build page status is Works, tested on a Mac.

**What it runs on.** `cc2340r5` (LP-EM-CC2340R5), SimpleLink Low Power F3 SDK 9.21.00.36, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0, UniFlash 9.6.0, on an Intel Mac with macOS 15.7.9.

**Shown by.** On 2026-10-20 the Mac received 15 of 15 button changes as HID input reports, none missed. The default build's report descriptor and four test reports match the library's output on an ESP32-C6, 5 of 5 lines. The live build with the buttons was not compared. Log: [ports/esp32-ble-gamepad/runs/2026-10-20/live-buttons-mac-hid.txt](https://github.com/Fox-Red-Labs/fox-red-labs/blob/main/ports/esp32-ble-gamepad/runs/2026-10-20/live-buttons-mac-hid.txt).

**What did not work.** If `make` stops at once with `sysconfig_cli.sh: not found`, name the SDK, SysConfig and compiler folders on the same line. Flashing with `-v`, or flashing the `.bin`, ends in `Values at address 0x4E020750 do not match`; flash the `.hex` without `-v`. The browser page showing the presses was checked by eye in Chrome and not recorded. Only a Mac was tried as the host.

Built on the lab's bench, not by a reader. If you repeat it, reply with your board, version and result.
