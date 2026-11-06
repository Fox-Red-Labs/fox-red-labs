# Wi-Fi family porting FAQ: errors seen moving code to and from the LP-EM-CC35X1

Errors hit while moving code onto the CC35X1 and between it and the F3 boards, with the fix and the file behind each. Board-only errors are in `cc35x1/FAQ.md`.

Terms: [docs/vocab.md](../../../docs/vocab.md); the family guide is [README.md](README.md). 1 kB = 1,024 bytes.

## Building

### `make` stops because `imports.mak` names folders a Mac lacks

Pass `SIMPLELINK_WIFI_SDK_INSTALL_DIR`, `SIMPLELINK_WIFI_TOOLBOX_INSTALL_DIR`, `SYSCONFIG_TOOL` and `TICLANG_ARMCOMPILER`, and keep `GCC_ARMCOMPILER=` empty. `cc35x1/AGENTS.md`.

### `[PYI-36117:ERROR] Failed to execute script 'simplelink-wifi-toolbox'` while signing, or `make: *** Terminated: 15` while linking

Free disk, delete each example's build folder after programming, and rerun that example. The disk was down to a few hundred MB and the tools die without saying so. `cc35x1/AGENTS.md`.

### `Path to programming_instructions after path correction couldn't be found`

Rebuild with an absolute build folder, or make the paths absolute. Relative paths in `tool_settings.json` resolve under the Toolbox folder. `cc35x1/AGENTS.md`.

### An F3 example needs a driver the Wi-Fi SDK does not ship

There is no twin to copy. The F3 SDK's AES, NVS, SD, display, log and I2C target examples have no same-name Wi-Fi example. `portability/everywhere/README.md`, matrix X26.

## Running

### `Process is stopped. Life cycle OPERATIONAL is not valid. Get fuse data failed`

The chip is not activated; the tool is fine. Ask before activating. `cc35x1/AGENTS.md`.

### The board is silent after programming over the Wi-Fi CSI program, even TI's own examples

Run `full_flash_erase` (about 16 s), then program as usual. The CSI program lays the flash out differently. Also read the CSI image at 921600; at 115200 it looks dead. `cc35x1/AGENTS.md`.

### Programming finished but the new image never starts

Press S1 or run `xds110reset -s <probe serial>`. The Toolbox does not reset the board. `cc35x1/AGENTS.md`.

### `Timeout expired connecting to Network` with only `[WLAN EVENT] Unexpected event [0x19]` before it

Type the network name. The board never found the name; on macOS 15 `ipconfig getsummary en0` prints `<redacted>`. `ports/wifi-provisioning-ble/README.md`, `cc35x1/AGENTS.md`.

### Apple Home hangs on "Connecting"

Erase, program again and scan within 5 minutes. TI's Matter example keeps the commissioning window open 300 s. `cc35x1/AGENTS.md`.

### OTA menu says `Invalid input` or `Invalid choice`

Send only a carriage return after each answer. The example reads a line feed as an empty answer to the next prompt. `cc35x1/AGENTS.md`.

### OTA `Show components in flash` lists slot 2 as `FAILED`

Menu `3`, then the slot number (5 for the vendor image; about 20 s). A board programmed through the Toolbox starts with its second slots in `FAILED`. `cc35x1/AGENTS.md`.

### `ERROR rxData_DataPacketDisptcher(): Received VLAN packet - drop it`

Ignore it. Seen with a router that sends VLAN-tagged packets; join, ping and iperf work. `cc35x1/AGENTS.md`.

### Zephyr `hello_world` links but the image will not sign

 On TI's fork (v3.7.0-ti-1.00.01_ga) `hello_world` and `blinky` link at 65 kB of flash; the signed-image step fails on Toolbox 4.3.23. No fix recorded. `data/support-matrix.json`.

## Mistakes to avoid

- Do not run the activation step to see what it does: it burns fuses.
- Do not flash a raw `.bin` from address 0.
- Do not expect a backup: the program sits in external flash behind the chip's security.
