# Zephyr on the LP-EM-CC2340R5 with the XDS110 probe

Terms: [docs/vocab.md](../../docs/vocab.md). Errors and fixes: [FAQ.md](../FAQ.md). 1 kB = 1,024 bytes.

Works: Zephyr 4.4.2 `samples/basic/blinky` runs on the LP-EM-CC2340R5. It took 82 seconds from the first command to the program printing on the serial port (2026-10-05). The serial port shows the LED state changing every second, and the green LED was seen blinking.

The LED is the **green** one. Zephyr's board file makes green `led0`, and blinky uses `led0`. The SDK example that was on the board before blinks red, so a green blink also proves the new program is the one running.

## Times

| Step | Seconds |
|---|--:|
| Read the board file, check the workspace, measure disk | 16 |
| Build (clean) | 47 |
| Check the image before flashing | 12 |
| Flash and start | 7 |
| **Total** | **82** |

Nothing new was downloaded: the Zephyr workspace and compiler (944 MB and 1.1 GB) were already on the Mac. This build added 34 MB. The first-time install of both is in [mac-setup/README.md](../../mac-setup/README.md).

## Manual overrides

| # | The official way | What blocked it | What was done instead |
|---|------------------|-----------------|-----------------------|
| 1 | `west flash`, per the Zephyr board page ("flashed in the usual way") | The board's flash setup lists J-Link only. The probe sold for this board is an XDS110. The prebuilt OpenOCD used for the MSPM0 board has no CC23xx support. | UniFlash command line with the connection file from this repo. |
| 2 | Flash `zephyr.hex` | The chip's boot code checks checksums in its configuration area. The build writes a second file, `zephyr_crc32.hex`, with them filled in. `west flash` picks that file on its own; a person flashing by hand has to know. | Flashed `zephyr_crc32.hex`. |
| 3 | Flash with verify | Verify reports one mismatch at 0x4E020750 (expected 0x00, read 0xFF). Same address with the SDK image and the Zephyr image. | Treated as a tool or chip quirk, not a bad flash: both programs run. Cause still open. |

## Safety check before flashing

Zephyr's source for this chip warns that an image built without the chip-erase permission "will LOCK YOUR DEVICE." Before flashing, the built device tree was checked: debug port, flash program, chip erase and return to factory are all allowed, and the build printed no lock warning. The image was also checked to contain only the program area (0x0 to 0x277C) and the configuration area (0x4E020000 to 0x4E020800).

## Repeat it

`./setup-intel-mac.sh` installs Zephyr if it is not there, builds, checks the named probe is connected, flashes and resets. Set `ZP`, `TC` and `PROBE_SERIAL` to choose folders and the board.

## What you see

The build's memory report is flash 9.9 kB and RAM 4 kB. After reset the serial port prints `LED state: ON` and `LED state: OFF` one second apart, and the green LED toggles with it.

Next: Bluetooth on this board needs TI's Zephyr fork; see [zephyr-ti](../zephyr-ti/README.md).
