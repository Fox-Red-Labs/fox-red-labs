<!-- experiment
id: E5
board: LP-EM-CC2340R5
date: 2026-10-05
status: works
reached: verified
line: Four settings changed as text and verified on the board; three more built and booted; one was silently ignored and needed C.
stop: none
-->
# Can you change TI LP-EM-CC2340R5 radio settings without the SysConfig GUI?

**Short answer.** Mostly yes. Four settings changed as text were verified on the board, three more built and booted, and one more built without complaint but was ignored, so it had to be changed in C instead.

Terms: [vocab](../vocab.md) (SysConfig, MCUboot, Zigbee coordinator and router, DMM, heap). Errors and fixes: [CC2340R5 FAQ](../../cc2340r5/FAQ.md).

**Question.** Forum threads report that TI's radio settings live in SysConfig, a graphical tool in CCStudio (it appears throughout TI's forum threads). Can a builder change them as text from a terminal?

**Vendor position.** TI's example READMEs present SysConfig as a GUI: "SysConfig is a GUI configuration tool ... import the SysConfig-enabled project into CCS. Double click the `*.syscfg` file" (`examples/rtos/LP_EM_CC2340R5/zigbee/onoff_light/README.md`, F3 SDK 9.21.00.36). The SDK's make files call the command-line version (`sysconfig_cli.sh`) to generate code.

**What was tried.** The build command for each example is in `cc2340r5/README.md`, "Build" (or the FAQ entry "The SDK's `make` cannot find the compiler or SysConfig"). On 2026-10-05: F3 SDK 9.21.00.36, SysConfig 1.26.3, TI Arm Clang 5.1.1, from Terminal on an Intel Mac with macOS 15.7.9. Edited the `.syscfg` files of TI's examples in a text editor, then ran `make`. Diffs: `cc2340r5/matrix/basic_ble_text.syscfg.diff`, `cc2340r5/zigbee/README.md`.

**How far it got.** Verified on the board (four settings):

- Bluetooth name and advertising interval (100 ms to 20 ms): rebuilt in 31 seconds; the new name was seen from the Mac, and the gaps between packets shortened to a 13 ms median. Why the median gap is shorter than the 20 ms setting is **Unknown.** The board sends each advertisement on three channels, which may explain it; not checked.
- Zigbee role (coordinator to router) and channel list (channel 11 only to all 16): the board joined the house network.
Built and booted, effect not tested (three settings): MCUboot mode, compression and slot size. The bootloader started the client image flashed into its slot and the device joined Home Assistant, but no over-the-air update finished, so these settings were never exercised.

Built only: a two-radio (DMM) configuration's storage addresses and heap size, moved from a CC27xx example. On the CC2340R52 it compiled but did not link (2.1 kB short). On the CC2340R53 it linked after the heap was cut to 24 kB; not run.

**How to find a setting's name.** Each option is a line like `zigbee.deviceType = ["zr"];`. The names and allowed values are in the SDK's `.meta` JavaScript files next to each module, for example `source/ti/zigbee/.meta/zigbee_common.js` (device types) and `source/ti/ble/.meta/broadcaster/advertisement_params_docs.js` (advertising). Searching those files for the word you see in the GUI usually finds it.

**The line.** An option that the text accepts but ignores. Two limits:

- **Unknown (suspected: Tool or docs).** `zigbee.powerModeOperation = "alwaysOn";` was added to the `.syscfg`; it built without complaint, but the generated `ti_zigbee_config.h` still said `ED_RX_ALWAYS_ON ZB_FALSE`. No warning appeared. TI's metadata (`source/ti/zigbee/.meta/pm/zigbee_pm.syscfg.js`) marks the option read-only by default and unlocks it only when the device type changes in the tool; the GUI was never opened to check. The end device's "always on" mode could only be changed in C.
- **TI example default.** The generated files keep the example's defaults. SysConfig's command line did reject a CC27xx flash address in a CC23xx build (`Region Base (0xE6000) is outside the device's usable flash`), but nothing flagged that the Zigbee examples listen on channel 11 only.

Existing TI examples were edited; no `.syscfg` was written from scratch.

**Ask.** Which SysConfig settings have you had to change in C because the tool would not let you?
