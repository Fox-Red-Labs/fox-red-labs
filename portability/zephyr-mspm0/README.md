# Zephyr I2C and flash drivers for the LP-MSPM0G3519

**Short answer.** Zephyr's MSPM0 I2C and flash drivers run on the LP-MSPM0G3519, from open pull requests that are not merged. Zephyr's open MSPM0 I2C pull request applies to current Zephyr main without a conflict. It ran on the board in loopback (one I2C peripheral talking to itself) and against a real device, a touch controller on the bus. The flash driver pull request is out of date against main, so this folder carries a rewritten driver (polling, checked results) that erased, wrote and read back the last 1 kB sector of main flash on the same board. Nothing here is upstream; the last section lists what an upstream pull request still needs.

Terms: [docs/vocab.md](../../docs/vocab.md). Contents: upstream state, what is in this folder, take it with you, set up, build, flash and run, results, what went wrong, what is left. 1 kB = 1,024 bytes. Run 2026-10-24 on an Intel Mac (macOS 15.7.9), one LP-MSPM0G3519 on an XDS110 probe (firmware 3.0.0.43).

| Piece | Version |
|---|---|
| Zephyr | `main` at `1405adc9a` (4.5.0 development, 2026-10-15), shallow clone |
| I2C driver | upstream pull request #117311 head `f78c2dadfd86` (4 commits), applied with `git am`, no conflict |
| Flash driver | `drivers/flash/flash_soc_ti_mspm0.c`, derived from pull request #91500 (head `2bc8317c63bb`), rewritten here |
| MSPM0 DriverLib | Zephyr's `hal_ti` at `53c80140` (the flash driver uses its `dl_flashctl`) |
| Compiler | Zephyr SDK 0.17.4 GCC 12.2.0, `ZEPHYR_TOOLCHAIN_VARIANT=cross-compile`, CMSIS 6 at `30a859f4` |
| Flashing | UniFlash 9.6.0 DSLite and xPack OpenOCD 0.12.0-7 (`flash.sh`) |

## Upstream state (read 2026-10-24)

| What | Where | State |
|---|---|---|
| MSPM0 I2C, controller and target, polling or interrupt, 7 and 10 bit | https://github.com/zephyrproject-rtos/zephyr/pull/117311 (by s-m33r, opened 2026-08-25) | Open, changes requested (maass-hamburg 2026-08-25, parthitce 2026-08-26), last activity 2026-10-16; mergeable, 27 files, 1,777 added lines; enables the `i2c_target_api` test on G3507, G3519 and L2228 |
| Same driver, power management | https://github.com/zephyrproject-rtos/zephyr/pull/122743 | Open, built on #117311, review required |
| Earlier I2C attempt | https://github.com/zephyrproject-rtos/zephyr/pull/94627 | Closed 2026-08-26, replaced by #117311 |
| I2C with DMA | https://github.com/zephyrproject-rtos/zephyr/pull/119175 | Closed (draft, marked stale 2026-10-24); depends on #117311 and the MSPM0 DMA pull request #118085 |
| MSPM0 flash driver (Linumiz and TI) | https://github.com/zephyrproject-rtos/zephyr/pull/91500 (opened 2025-06-12) | Open, merge conflicts with main; approvals from nordicjm, JarmouniA and Ayush1325 in June 2026, changes requested by de-nordic (2026-01) and a TI reviewer (2026-07-15) |
| Flash controller devicetree node and binding | already in `main` | Merged without a driver: `flashctl` node, `ti,mspm0-flash-controller.yaml` |
| TI's own tree | https://github.com/TexasInstruments/msp-zephyr (branches `feature/flash-driver`, `feature/baseline-i2c-support`, `feature/unicomm_i2c`, `device/mspm0g5187-support`) | Carries the same drivers, a UNICOMM I2C variant and a `lp_mspm0g5187` board; the G5187 board needs `hal_ti` files (`mspm0g51xx-pinctrl.dtsi`, the G518x device header) that no public `hal_ti` branch has, so it did not build here |
| BeagleBoard's fork (branch `zephyr-4.1.0`) | https://github.com/beagleboard/zephyr | No MSPM0 I2C or flash driver |

## What is in this folder

| Path | What |
|---|---|
| `patches/0001-i2c-mspm0-pr117311.patch` | The I2C pull request, unchanged, as `git am` input |
| `patches/0002-flash-soc-ti-mspm0.patch`, `drivers/` | The flash driver, its Kconfig and the three lines that hook it into `drivers/flash` and `modules/Kconfig.mspm0` |
| `apps/flash_test/` | Erase, write, read back, erase on the last sector, in the style of `samples/drivers/soc_flash_nrf` |
| `apps/i2c_loopback/` | Controller and Zephyr's EEPROM target on one I2C peripheral, joined inside the chip by the loopback bit |
| `apps/i2c_probe/` | Bus scan, ID registers of every device that answers, one HDC2080 reading when the Sensors BoosterPack is stacked |
| `build.sh`, `flash.sh`, `run.sh` | Build one app, flash the G3519, flash and capture the console |

## Upstream pull requests, prepared

Two Zephyr pull requests are prepared in [upstream/](https://github.com/Fox-Red-Labs/fox-red-labs/tree/main/portability/zephyr-mspm0/upstream). The board one is open as https://github.com/zephyrproject-rtos/zephyr/pull/123497 (2026-10-25, with the pin audit as its first comment); the flash-driver one is pushed to the contributor's fork (branch `flash-final`) and not opened. The test data for the I2C driver was posted on #117311 (`upstream/COMMENT-117311.md`). Each folder holds the `git format-patch` series (without `Signed-off-by`), the pull request text, the review comments it answers, and the build output.

| Folder | What | Built against |
|---|---|---|
| [upstream/flash-driver/](https://github.com/Fox-Red-Labs/fox-red-labs/tree/main/portability/zephyr-mspm0/upstream/flash-driver) | The MSPM0 flash driver (polling, `zephyr,flash` node), `tests/drivers/flash/common` overlays for the G3519, G3507 and L2228, `flash` in the G3519 supported list | Zephyr `main` `bc5d59d1b` (2026-10-25) |
| [upstream/board-lp-mspm0g3519/](https://github.com/Fox-Red-Labs/fox-red-labs/tree/main/portability/zephyr-mspm0/upstream/board-lp-mspm0g3519) | `adc0` enabled on the LP-MSPM0G3519 with an `adc_dt` overlay, BoosterPack I2C2 pin groups (PC2, PC3), board documentation; the `i2c2` node waits for #117311 | same |
| | What a MicroPython `ports/zephyr/boards/lp_em_cc2340r5.conf` and overlay submission contains | MicroPython v1.29.0 |

State on 2026-10-25: #117311 (I2C) and #91500 (flash) are both still open and not merged; #91500 conflicts with `main`; this repo's board pull request is #123497. These builds compile only. The flash driver ran on silicon on 2026-10-24 (table above); nothing in `upstream/` was run on a board.

## Take it with you

Copy these files into a folder of your own, keeping the folder names. Nothing from the rest of this repo is needed to build.

| File | What it is |
|---|---|
| `patches/0001-i2c-mspm0-pr117311.patch` | Zephyr pull request #117311 (MSPM0 I2C driver, 4 commits), unchanged, as `git am` input |
| `patches/0002-flash-soc-ti-mspm0.patch` | The flash driver, its Kconfig and the three hook lines (`drivers/flash/CMakeLists.txt`, `drivers/flash/Kconfig`, `modules/Kconfig.mspm0`), as `patch -p1` input |
| `drivers/flash_soc_ti_mspm0.c`, `drivers/Kconfig.ti_mspm0` | The same two driver files on their own, for copying into `zephyr/drivers/flash/` instead of using the patch |
| `apps/flash_test/` | `CMakeLists.txt`, `Kconfig`, `prj.conf`, `src/main.c`, `boards/lp_mspm0g3519.overlay` (the overlay names `&flashctl` as `zephyr,flash-controller`) |
| `apps/i2c_loopback/` | `CMakeLists.txt`, `prj.conf`, `src/main.c`, `boards/lp_mspm0g3519.overlay`: controller and EEPROM target on I2C2, no wires |
| `apps/i2c_probe/` | `CMakeLists.txt`, `prj.conf`, `src/main.c`, `boards/lp_mspm0g3519.overlay`: bus scan and ID reads on PC2 and PC3 |

`flash.sh` and `run.sh` run each flash under this lab's probe-lock helper (`tools/frl/lock.py`), which the reader copy does not include, and take a `.ccxml` from the repo; they are not part of the take-away. The flash commands to use instead are under "Flash and run" and in [`../arduino/zephyr/README.md`](../arduino/zephyr/README.md#flash-and-run): pulse nRST, flash a `.bin` padded to a multiple of 8 bytes with DSLite, pulse nRST again.

**What it needs.**

- A Zephyr workspace at `main` `1405adc9a6042353ff791573d913fa6b202580a6` with `hal_ti` at `53c80140dd69905cc5e034f637eff9c66b2f9654` and CMSIS 6 at `30a859f44ef8ab4dc8f84b03ed586fd16ccf9d74`, built by the recipe under "Set up once" (replace `<repo>/portability/zephyr-mspm0/patches/` with the folder you copied `patches/` to).
- Zephyr SDK 0.17.4 GCC 12.2.0 as a cross compiler and a venv with `west`.
- Defines: none needed. `-DCONFIG_APP_LEAVE_PATTERN=y` makes `flash_test` stop with its pattern in the last sector; `-DBS_POWER=1` makes `i2c_probe` drive the Sensors BoosterPack power pins.
- No force-include flag. The overlays are picked up by name from each app's `boards/` folder.

**A different board in the same family.** Name its Zephyr board in `-b` and add `apps/<app>/boards/<board>.overlay`. For `flash_test` the overlay only names the flash controller; the flash size and sector size come from the board's `zephyr,flash` node. The I2C apps need the pin groups in their overlay (`i2c2_scl_pc2` and `i2c2_sda_pc3`, PINCM76 and PINCM77 at function 2 on the G3519): take the PINCM numbers and the function from SysConfig's output for the new chip, not from Zephyr's shared pin file. Test overlays for the LP-MSPM0G3507 and LP-MSPM0L2228 flash driver are in `upstream/flash-driver/patches/0003-...`; they have not been run on those boards.

**Build from a fresh folder** (`<proj>` holds `apps/`; `<zephyr workspace>` is the patched workspace):

```bash
. <venv>/bin/activate
export ZEPHYR_BASE=<zephyr workspace>/zephyr ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE=<toolchain>/arm-zephyr-eabi/bin/arm-zephyr-eabi-
cd <zephyr workspace>/zephyr
west build -p always -b lp_mspm0g3519 <proj>/apps/flash_test -d <build>      # or i2c_probe, i2c_loopback
```

The image is `<build>/zephyr/zephyr.hex`.

Built from a copy of only `apps/flash_test/` on 2026-10-25 (copy outside the repo, against the existing patched workspace, zero errors): 14.5 kB of flash and 6.9 kB of RAM. The patch recipe under "Set up once" was not re-run for this build, and `i2c_probe` and `i2c_loopback` were not built from a copy.

## Set up once

The scripts read the variables below, fall back to the default shown, and stop with one line naming the variable when a path is missing. All paths are examples; any folder with room works.

| Variable | What it is | How to get it | Default folder | Size |
|---|---|---|---|---|
| `ZW` | Zephyr workspace with the patches applied (recipe below) | `git fetch` of three commits, then two patches | any folder (the scripts' default is an external drive, so set it) | 0.8 GB |
| `TC` | Zephyr SDK 0.17.4 GCC 12.2.0 (`arm-zephyr-eabi`) and xPack OpenOCD 0.12.0-7 (`xpack-openocd-0.12.0-7` inside it) | `cc2340r5/zephyr/setup-intel-mac.sh` installs the compiler; OpenOCD is the xPack release unpacked into the same folder | `zephyr-toolchain` in your home folder | 1.1 GB |
| `VENV` | Python 3.12 venv with `west` | same script | `zephyrproject/.venv` in your home folder | 0.3 GB |
| `DSLITE` | UniFlash 9.6.0 command-line flasher | install UniFlash 9.6.0 from TI | `/Applications/ti/uniflash_9.6.0/.../DSLite` | 1 GB |
| `OCD`, `CFG`, `CCXML` | OpenOCD folder, the board's OpenOCD config (in `ZW`), the target configuration (`mspm0g3519/MSPM0G3519_<probe serial>.ccxml`) | in this repo and `ZW` | see `flash.sh` | |
| `PROBE`, `PORT` | probe serial of your board (`xdsdfu -e` lists it) and its console, `/dev/cu.usbmodem<probe serial>1` | plug the board in | `PROBE=MG000001` | |

Also needed: `ninja`, `cmake`, `pyserial` (the capture helper), and, for `flash.sh` as written, the lab's probe-lock helper (`tools/frl/lock.py`); without it, use the commands under "Flash and run".

The workspace (Zephyr main plus two modules plus the patches), about 5 minutes with a good connection:

```bash
ZW=<zephyr workspace>; mkdir -p $ZW/.west $ZW/modules/hal/ti $ZW/modules/hal/cmsis_6
printf '[manifest]\npath = zephyr\nfile = west.yml\n\n[zephyr]\nbase = zephyr\n' > $ZW/.west/config
fetch() { git -C "$1" init -q; git -C "$1" remote add origin "$2"; git -C "$1" fetch -q --depth 1 origin "$3"; git -C "$1" checkout -q FETCH_HEAD; }
fetch $ZW/zephyr             https://github.com/zephyrproject-rtos/zephyr   1405adc9a6042353ff791573d913fa6b202580a6
fetch $ZW/modules/hal/ti      https://github.com/zephyrproject-rtos/hal_ti   53c80140dd69905cc5e034f637eff9c66b2f9654
fetch $ZW/modules/hal/cmsis_6 https://github.com/zephyrproject-rtos/CMSIS_6  30a859f44ef8ab4dc8f84b03ed586fd16ccf9d74
cd $ZW/zephyr
git -c user.name=build -c user.email=build@example.com am <repo>/portability/zephyr-mspm0/patches/0001-i2c-mspm0-pr117311.patch
patch -p1 < <repo>/portability/zephyr-mspm0/patches/0002-flash-soc-ti-mspm0.patch    # or copy drivers/* into drivers/flash
```

## Build

```bash
export ZW=<zephyr workspace>
portability/zephyr-mspm0/build.sh flash_test      # or i2c_loopback, i2c_probe, i2c_target_api, hello
```

The image is `<build>/zephyr/zephyr.hex` (default build folder `zephyr-mspm0/<app>` under `$TMPDIR`; pass a second argument to change it). `-DCONFIG_APP_LEAVE_PATTERN=y` makes `flash_test` stop with the pattern programmed in the last sector, so a debugger can read it. `-DBS_POWER=1` makes `i2c_probe` drive the Sensors BoosterPack power pins; leave it off while a display is wired to the board.

## Flash and run

Back up the main flash (512 kB) and data flash (16 kB) before the first write and restore at the end; the Back up and Restore rows in `mspm0g3519/AGENTS.md` give the DSLite steps, and a restore ends with a read back. Then:

```bash
portability/zephyr-mspm0/run.sh <build>/zephyr/zephyr.hex /tmp/flash_test-run.txt 14    # flash, then 14 s of console at 115200
```

`run.sh` holds the console port open while `flash.sh` runs, so the first line is not missed. To reflash without a capture: `portability/zephyr-mspm0/flash.sh <image.hex>`.

## Results on the LP-MSPM0G3519

| Test | Result |
|---|---|
| Flash, last 1 kB sector of the 256 kB bank (offset 0x3FC00), 18 checks | 18 of 18 pass on the first boot after flashing, and the program stays up (idle line every 5 s): erase, blank read, 1,024-byte write, read back equal, direct memory read equal, misaligned offset and length refused (-22), read past the end refused, write from an unaligned source, final erase blank |
| Flash timing (cycle counter, 80 MHz) | Erase of a programmed sector 2,625 us; write of 1 kB (128 words of 64 bits) 8,676 us, 67.8 us per word. Data sheet figures not compared |
| Flash, read by the debugger | Pattern left in the sector by the program (`(i*7+3) & 0xFF`), read with OpenOCD `mdw 0x3fc00 256`: 1,024 of 1,024 byte values equal |
| I2C loopback, controller to the EEPROM target at 0x54 inside one peripheral (I2C2, 100 kHz, no wires) | 12 of 12 checks: preloaded 16 bytes read through the controller, 16 bytes written and read back, single-byte register write and read, 128-byte write and read back, an unused address returns -5. A 17-byte write-read took 1,795 us |
| I2C against a real device, BoosterPack pins PC2 and PC3 | The scan finds one device, 0x38, the FT6336U touch controller of the Waveshare 3.5 inch panel wired to this board: vendor ID 0x11, chip ID 0x64, firmware 0x12, 0 touch points. The same IDs the earlier software-I2C run read. A write, repeated start, read through `i2c_transfer` returned 0x64; an empty address returns -5 after 153 us |
| BP-BASSENSORSMKII HDC2080 and OPT3001 | Not run: the pack is stacked on the LP-MSPM0G5187 with the radio wired. The code for them is in `i2c_probe` and has not seen a sensor |
| LP-MSPM0G5187 | Not run. Zephyr main has no board or SoC for it (it uses the UNICOMM I2C block, not the I2C block the driver programs) |
| Board state | Main flash (512 kB) and data flash (16 kB) backed up before the first write, restored at the end, read back equal (SHA-256 of main starts `84de0e1f17ca`, data `0fbba07a833d`) |

## What went wrong

| What you see | Cause | Do this |
|---|---|---|
| OpenOCD "Could not find MEM-AP" or DSLite "Connection to MSPM0 core failed" while a Zephyr image runs | The running image hides the core from the debugger | Pulse nRST first: `openocd ... -c init -c mspm0_board_reset -c shutdown` (`flash.sh` does) |
| OpenOCD `program` fails with "Unknown Device ID[0x0]" on a board that runs Zephyr | Same state, after `reset halt` | Program with DSLite after the nRST pulse |
| DSLite "Length of block is 14860, but it should be divisible by 8" and a verify error | DSLite writes 64-bit words and refuses a block whose length is not a multiple of 8, and Zephyr's hex has any length | Convert to a `.bin`, pad with 0xFF to a multiple of 8, flash `file.bin,0x0` |
| `i2c_target_register` never runs, the target never answers, `TOAR` still reads 0x4000 | Zephyr's `zephyr,i2c-target-eeprom` driver only registers when the app calls `i2c_target_driver_register()` | Call it once after boot |
| The upstream flash driver does not compile against main's devicetree | The G3519 has three `soc-nv-flash` children (two banks and the data flash) under the controller, and the driver takes the size of all of them; the controller node has no `interrupts` | This folder's driver takes the range from the `zephyr,flash` node and polls the command status |
| Scan finds 0x38 on a bus with nothing stacked | The Waveshare panel's touch chip shares PC2 and PC3 | Expected on this bench; check the board's own README for what is wired |

## What is left for an upstream pull request

- The flash driver (rebased and prepared in `upstream/flash-driver/`): decide between polling and an interrupt (the controller node in main has no `interrupts` property), cover the second bank and the data flash (the devicetree has three `soc-nv-flash` children, the driver serves the `zephyr,flash` one), add a `tests/drivers/flash` board overlay, and answer the open change requests on #91500.
- The I2C driver: nothing to add for the G3519 beyond #117311; this run adds two data points for its review thread (single-peripheral loopback passes, a real device reads correctly at 100 kHz). The pull request points the board file's `i2c1` at PB2 and PB3 and leaves the node disabled; the BoosterPack pins PC2 and PC3 (I2C2, pin function 2, PINCM76 and PINCM77) are in `apps/i2c_probe/boards/lp_mspm0g3519.overlay` and are prepared as board pin groups in `upstream/board-lp-mspm0g3519/`.
- Not covered: 400 kHz and 1 MHz, 10-bit addressing on silicon, interrupt-versus-polling comparison, power management (#122743), the G5187 and any UNICOMM part, and the Sensors BoosterPack run.
- The G3519 blinky stopping in the NMI handler on Zephyr 4.4.2 ([FAQ](../../mspm0g3519/FAQ.md)): a second run on Zephyr main reproduced the stop (SRAM double-error NMI) when DSLite flashed and started the image with no nRST pulse, and the same flash followed by one nRST pulse ran. The four builds here were always followed by that pulse (`flash.sh` step 3), so they did not hit it. The start-after-flash path is the suspect, not the image or the SRAM bank; not tested further.

## Not checked

- The I2C driver at 400 kHz and 1 MHz, 10-bit addressing on silicon, interrupt versus polling, power management (#122743), and the Sensors BoosterPack HDC2080 and OPT3001.
- The LP-MSPM0G5187 and any UNICOMM part.
- The `upstream/` pull request builds were not run on a board. Every other gap is listed under "What is left for an upstream pull request".

## License and source

Zephyr is Apache-2.0. The I2C patch is pull request #117311 by its author, unchanged, as `git am` input. `drivers/flash_soc_ti_mspm0.c` is derived from pull request #91500 (Linumiz and TI) and rewritten here, under the same Apache-2.0 license. `hal_ti` (TI DriverLib) and CMSIS 6 are cloned, not copied. The apps, scripts and `upstream/` texts are this repo's.
