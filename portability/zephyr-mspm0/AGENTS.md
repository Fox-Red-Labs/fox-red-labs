# Zephyr MSPM0 I2C and flash drivers on the LP-MSPM0G3519: notes for AI coding assistants

That neither driver is merged into Zephyr: the I2C driver is an open pull request applied as a patch, the flash driver is a rewrite kept here, and a running Zephyr image hides the chip from the debugger until nRST is pulsed. This file is the short list, each item from runs on one LP-MSPM0G3519.

Terms: [docs/vocab.md](../../docs/vocab.md) and the [README](README.md) in the Fox Red Labs repo. In short: **west** is Zephyr's build front end; **nRST** is the chip's reset pin; **NONMAIN** is the flash area that holds the chip's boot settings; **DSLite** is TI's command-line flasher (UniFlash 9.6.0); **MEM-AP** is the debug port the debugger uses to reach the CPU; **FT6336U** is the touch controller on the Waveshare 3.5 inch panel (I2C address 0x38). 1 kB = 1,024 bytes. Run 2026-10-24 on an Intel Mac, macOS 15.7.9, with Zephyr `main` at `1405adc9a` (4.5.0 development), pull request #117311 head `f78c2dadfd86`, `hal_ti` `53c80140`, CMSIS 6 `30a859f4` and Zephyr SDK 0.17.4 GCC 12.2.0.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths below in backticks starting `portability/` are in the Fox Red Labs repo, where the evidence for each line lives; they are not in your copy, and links in this file only work inside that repo. The files to copy are listed in "Take it with you" in `portability/zephyr-mspm0/README.md`.

## The layer

| Part | Fact |
|---|---|
| I2C driver | Zephyr pull request #117311, applied unchanged with `git am`; open and not merged on 2026-10-25 (`portability/zephyr-mspm0/README.md`) |
| Flash driver | `drivers/flash_soc_ti_mspm0.c` plus `Kconfig.ti_mspm0`, derived from pull request #91500 and rewritten (polling, takes its range from the `zephyr,flash` node); #91500 conflicts with `main` (`portability/zephyr-mspm0/README.md`) |
| Test apps | `apps/flash_test`, `apps/i2c_loopback`, `apps/i2c_probe`, each with a `boards/lp_mspm0g3519.overlay` (`portability/zephyr-mspm0/README.md`) |
| Proven | LP-MSPM0G3519: flash 18 of 18 checks on the last 1 kB sector; I2C loopback 12 of 12; the FT6336U read at 0x38 |
| Not run | 400 kHz and 1 MHz, 10-bit addressing on silicon, power management (#122743), the LP-MSPM0G5187 and any UNICOMM part, the Sensors BoosterPack sensors, everything in `upstream/` on a board (`portability/zephyr-mspm0/README.md`) |

## Start here

| Step | This port |
|---|---|
| Workspace | `<zephyr workspace>/zephyr` at `1405adc9a6042353ff791573d913fa6b202580a6`, `modules/hal/ti` at `53c80140dd69905cc5e034f637eff9c66b2f9654`, `modules/hal/cmsis_6` at `30a859f44ef8ab4dc8f84b03ed586fd16ccf9d74`, then `git am 0001-i2c-mspm0-pr117311.patch` and `patch -p1 < 0002-flash-soc-ti-mspm0.patch` (`portability/zephyr-mspm0/README.md`, "Set up once") |
| Tools | Zephyr SDK 0.17.4 GCC 12.2.0 at `<toolchain>` with `ZEPHYR_TOOLCHAIN_VARIANT=cross-compile`, a Python 3.12 venv with `west`, `ninja`, `cmake` |
| Build | `west build -p always -b lp_mspm0g3519 <proj>/apps/flash_test -d <build>` with `ZEPHYR_BASE=<zephyr workspace>/zephyr`; image `<build>/zephyr/zephyr.hex` |
| Probe serial | `xdsdfu -e` from CCStudio's `ccs_base/common/uscif/xds110/` folder |
| Back up | Main flash (512 kB) and data flash (16 kB) before the first write (`portability/zephyr-mspm0/README.md`) |
| Flash | Pulse nRST (`openocd ... -c init -c mspm0_board_reset -c shutdown`), convert the hex to a `.bin` padded with 0xFF to a multiple of 8 bytes, `DSLite flash` it, pulse nRST again (`portability/zephyr-mspm0/flash.sh`) |
| Check | Console at 115200 on `/dev/cu.usbmodem<probe serial>1`, port held open before the flash; `flash_test` prints `FLASH-TEST: PASS`, `i2c_probe` lists 0x38 with the panel wired |
| Restore | The backup, written back and read back |

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| OpenOCD `Could not find MEM-AP` or DSLite `Connection to MSPM0 core failed` while a Zephyr image runs | A running image hides the core from the debugger | Pulse nRST first | Debugger attaches (`portability/zephyr-mspm0/README.md`) |
| OpenOCD `program` fails with `Unknown Device ID[0x0]` | Same state, after `reset halt` | Program with DSLite after the nRST pulse | Verify passes |
| DSLite `Length of block is 14860, but it should be divisible by 8` and a verify error | DSLite writes 64-bit words; Zephyr's hex has any length | Convert to `.bin`, pad with 0xFF to a multiple of 8, flash `file.bin,0x0` | Verify passes |
| `i2c_target_register` never runs, `TOAR` reads 0x4000 | Zephyr's EEPROM target driver registers only when the app calls `i2c_target_driver_register()` | Call it once after boot | Target answers at 0x54 |
| The upstream flash driver does not compile against `main`'s devicetree | The G3519 has three `soc-nv-flash` children under the controller and the controller node has no `interrupts` | Use this folder's driver | Image links |
| Scan finds 0x38 on a bus with nothing stacked | The Waveshare panel's touch chip shares PC2 and PC3 | Expected on this bench | Chip ID 0x64 |
| Image flashed, nothing blinks, program counter at `z_SysNmiOnReset` | Start-after-flash path leaves SRAM in a bad state; cause not found | Pulse nRST after the flash | Program runs (`mspm0g3519/FAQ.md`) |

## Rules for the assistant

1. **Do not claim upstream support.** Say which pull request a feature comes from and that it is open (`portability/zephyr-mspm0/README.md`, "Upstream state").
2. **Pulse nRST around every flash** and hold the console port open before it, so the first line is not missed (`portability/zephyr-mspm0/flash.sh`).
3. **Take pin groups from SysConfig's output for the chip**, not from Zephyr's shared MSPM0 pin file (`portability/arduino/zephyr/BOARD-mspm0g3519.md`).
4. **Leave the Sensors BoosterPack power pins alone while a display is wired**: build `i2c_probe` without `-DBS_POWER=1` (`portability/zephyr-mspm0/README.md`).
5. **Back up main and data flash before the first write**, restore at the end and read back (`portability/zephyr-mspm0/README.md`).
