# Does Bluetooth LE work on the TI LP-EM-CC2340R5 with TI's Zephyr fork (simplelink-zephyr)?

Terms: Zephyr, west, CCFG, XDS110 and UniFlash are defined in [docs/vocab.md](../../docs/vocab.md). Errors and fixes: [FAQ.md](../FAQ.md).

**Update 2026-10-08: yes, with Zephyr's C library heap turned on** (`CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=2048` in `prj.conf`; 1024 for the peripheral sample). The silent start below was TI's Bluetooth stack failing to get memory for a timer with the heap at 0. The BTHome beacon and the peripheral sample then ran with the fork's own CCFG byte; the beacon sample hard-faults. Details: [the experiment page](../../docs/experiments/2026-10-05-cc2340r5-ti-zephyr-ble.md). The rest of this page covers the build as first run on 2026-10-05, before the heap setting was found.

As first built on 2026-10-05, the Bluetooth samples printed nothing. TI's fork (tag `v3.7.0-ti-1.00.01_ga`) builds the Bluetooth beacon and peripheral samples with workarounds, and they flash, but nothing printed as built. hello_world printed its boot banner after CCFG byte 0x4E02001C was changed from 0x0F to 0x07 and all four checksums were recomputed; the Bluetooth samples, changed the same way, printed nothing, so it is not known whether they booted. **Unknown (suspected: Tool or docs):** the heap setting above is the suspected cause; it was found after these runs.

Contents: [results](#results), [hello_world ran after a boot-configuration change](#hello_world-ran-after-a-boot-configuration-change), [Bluetooth samples print nothing](#bluetooth-samples-print-nothing), [suspects](#suspects-where-this-setup-differs-from-tis), [versions](#versions), [commands and workarounds](#commands), [times](#times-wall-clock-machine-waiting), [disk](#disk), [lock check](#lock-check).

Tested 2026-10-05 on an Intel Mac (macOS 15.7.9) with the XDS110 probe and UniFlash 9.6.0. This is the Zephyr side of the comparison with TI's own SDK (FreeRTOS) Bluetooth build, which works ([../matrix/README.md](../matrix/README.md)).

The board carries the CC2340R52 chip, 36 kB of RAM; the CC2340R53 is a 64 kB version on a separate board, LP-EM-CC2340R53. 1 kB = 1,024 bytes; 1 GB = 1,024 MB.

> **Which file to flash.** TI's fork runs TI's CRC tool after the build (`ti-simplelink-crc-tool`, a Python program that computes the CCFG checksums, or CRCs, and writes them into the built file), so its `zephyr.hex` carries three of the four checksums: flash `zephyr.hex`. Upstream Zephyr 4.4.2 writes them into a separate `zephyr_crc32.hex`, so flash that one there. **Tool or docs:** the fork handles the checksums differently from upstream Zephyr, and its tool leaves the fourth at zero (see below). Never flash `zephyr.bin`: it is 1.22 GB (1.2 GB; see Disk).

## Results

| Sample | Flash used | RAM used | As built | With CCFG byte 0x4E02001C set to 0x07 |
|---|--:|--:|---|---|
| `samples/hello_world` | 30.6 kB | 8 kB (22.2%) | Flashed, no serial output | **Printed its boot banner:** `*** Booting Zephyr OS build cbababd4c1b4 *** Hello World! lp_em_cc2340r5/cc2340r5` (observed 2026-10-05; not saved) |
| `samples/bluetooth/beacon` | 110.2 kB (21.5%) | 28.4 kB (78.9%) | Flashed, no serial output, nothing on air | No serial output; `no adverts seen` in 30 s |
| `samples/bluetooth/peripheral` | 202.8 kB (39.6%) | 34.3 kB (95.3%) | Flashed, no serial output, nothing on air | No serial output; `no adverts seen` in 30 s |

Flash is out of 512 kB, RAM out of 36 kB. The peripheral sample fits only because TI ships a board file for it (`boards/lp_em_cc2340r5.conf`) that turns off settings, flash storage and the GATT cache. The beacon sample fits with no changes and has no board file. Every flash ended in UniFlash's `Success`.

"Nothing printed" was tested with the probe attached only. A power cycle with no probe was not run.

## hello_world ran after a boot-configuration change

**Unknown (suspected: Tool or docs).** Diffing the configuration area (CCFG, 0x4E020000) against an upstream Zephyr 4.4.2 image that runs on this board: the fork sets `misc` at 0x4E02001C to 0x0F, upstream to 0x07. The two builds also differ in Zephyr version (the fork is based on Zephyr 3.7.0, per its `VERSION` file; upstream was 4.4.2), so this byte is one difference among several, not the only one. The extra bit is `saciTimeoutOverride` (`hw_ccfg.h`), which makes the boot ROM use the configuration's own wait for a debugger command instead of the factory one. hello_world printed its boot banner after CCFG byte 0x4E02001C was changed from 0x0F to 0x07 and all four checksums were recomputed (observed 2026-10-05; not saved). The setting may be meant for a power-up without a probe, which was not tested.

The change, as run here. It uses the `intelhex` Python package, which upstream Zephyr's Python environment includes, and upstream Zephyr's checksum script. Replace `~/zephyrproject` with your upstream Zephyr workspace and `build/zephyr/zephyr.hex` with your build's hex.

```bash
python3 -c "from intelhex import IntelHex; b=IntelHex('build/zephyr/zephyr.hex'); b[0x4E02001C]=0x07; b.write_hex_file('zephyr_misc07.hex')"
```

```bash
python3 ~/zephyrproject/zephyr/soc/ti/simplelink/cc23x0/ti_ccfg_crc32_calculate.py zephyr_misc07.hex zephyr_fixed.hex
```

```bash
/Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c CC2340R5.ccxml -e -f -u zephyr_fixed.hex
```

The other CCFG differences from upstream are the user-record CRC the fork leaves at zero and debug in the boot loader (`allowBldr`, 0x00 here, 0xA5 upstream). The user record is a 128-byte area at 0x4E020750 in the CCFG for the application's own data; its last word is a CRC over the rest. TI's `hw_ccfg.h` says the boot code has "no dependencies" on it and calls that CRC optional. Filling only the user-record CRC, with upstream's script on the unchanged beacon image, did not make it run. The hello_world image that printed its banner had the byte changed and all four CRCs filled by upstream's script, so the byte change with the user-record CRC still at zero was not tested. `allowBldr` was left as the fork sets it.

## Bluetooth samples print nothing

**Unknown.** With the same boot-configuration change, beacon and peripheral print nothing, not even Zephyr's boot banner, and nothing is seen on air in 30 seconds, so it is not known whether they booted. The interrupt control register showed no fault in progress; suspected: the code waits in early start-up (a clock or the radio), but that was not checked.

## Suspects: where this setup differs from TI's

Any of these could be why the Bluetooth samples print nothing. None is proven.

- **Compiler.** GCC 12.2 from Zephyr SDK 0.17.4, reused from the upstream Zephyr run through `cross-compile`, not TI's tested toolchain.
- **lief 1.0.0 instead of 0.12.3.** `lief` is a Python library that reads and rewrites compiled program files; TI's CRC tool uses it to write the checksums into the built file. The tool was written for lief 0.12.3 and ran here with 1.0.0 (workaround in step 5 below). The output CRCs agree with an independent calculation, but lief rewrote the ELF file (the compiled program the hex is made from), and the rest of the image was not compared against an unpatched one.
- **User-record CRC left at 0 by TI's tool.** TI's CRC tool patches three CRCs (boot config, main CCFG, debug config). Upstream Zephyr's script also writes a fourth, the user record's CRC at 0x4E0207CC. The three shared CRCs match the upstream script byte for byte. Filling the fourth alone did not help, and the hello_world image that printed its banner had it filled. TI's `hw_ccfg.h` says the boot code does not depend on the user record; that was not tested on the chip.
- **The objcopy path patch** (step 6 below), a one-line local change to the fork's build. `objcopy` is the GNU tool that copies a built program into other file formats, such as `.hex` and `.bin`.

Other limits:

- **The peripheral sample has 1.7 kB of RAM left.** **Chip limit.** Adding anything (logging, a second service, bonding) will likely overflow.
- **`west flash` supports only J-Link for this board**, per the fork's README ("only JLink is available"). **Tool or docs**, because here it is TI's fork and TI's README. Upstream Zephyr 4.4.2 has the same J-Link-only flash setup for this board; the FAQ tags that one **Third-party behavior** because upstream is not TI's. Flash with UniFlash and the XDS110 probe instead, as above.

## Versions

| Item | Value |
|---|---|
| Repo | <https://github.com/TexasInstruments/simplelink-zephyr> |
| Tag | `v3.7.0-ti-1.00.01_ga` (commit `cbababd4`). Newest GA tag on 2026-10-05. Newer `9.x` tags exist but are all `_ea` (early access) or `-internal`, so they were not used. |
| HAL | `simplelink-hal_ti` at `ab1817da` ("simplelink_lpf3: 218d961-ti-v1.00 release") |
| Other modules | `cmsis` `4b96cbb1`, `mbedtls` `2f24831e`, `tinycrypt` `1012a3eb` |
| Compiler | `~/zephyr-toolchain/arm-zephyr-eabi` (GCC 12.2 from Zephyr SDK 0.17.4), via `cross-compile` |
| Python | 3.12 venv at `~/zephyr-ti/.venv` |
| CRC tool | `ti-simplelink-crc-tool` 1.2.5.0 from PyPI, run with `lief` 1.0.0 (see step 5) |

## Commands

Steps that differ from TI's README are marked OVERRIDE. Comments after `#` are notes, not commands.

```bash
# 1. Python and west. 32 s.
mkdir -p ~/zephyr-ti && cd ~/zephyr-ti
python3.12 -m venv .venv && . .venv/bin/activate
pip install --upgrade pip west cmake

# 2. Workspace at the GA tag, shallow. 31 s.
west init -m https://github.com/TexasInstruments/simplelink-zephyr \
  --mr v3.7.0-ti-1.00.01_ga --clone-opt=--depth=1 ~/zephyr-ti/ws

# 3. Only the modules this build needs. OVERRIDE: project filter. 18 s total (11 s, then 7 s for the two crypto modules the first build asked for).
cd ~/zephyr-ti/ws
west config manifest.project-filter -- '-.*,+simplelink-hal_ti,+cmsis,+mbedtls,+tinycrypt'
west update --narrow -o=--depth=1

# 4. Python packages. 12 s.
pip install -r zephyr/scripts/requirements-base.txt

# 5. OVERRIDE: TI's CRC tool. It is listed only in requirements-extras.txt, and it pins
#    lief 0.12.3, which has no wheel for Python 3.12 on an Intel Mac and fails to build.
#    Install the tool without its pins and use a current lief wheel. Under a minute, not timed.
pip install --only-binary=:all: lief
pip download --no-deps -d ~/zephyr-ti/_pkgs ti-simplelink-crc-tool
pip install --no-deps ~/zephyr-ti/_pkgs/ti_simplelink_crc_tool-1.2.5.0-py3-none-any.whl

# 6. OVERRIDE: the fork's CC23xx post-build step calls objcopy at a fixed Zephyr SDK path.
#    Point it at the active compiler's objcopy instead (one-line local patch, not committed anywhere).
sed -i '' 's#${ZEPHYR_SDK_INSTALL_DIR}/arm-zephyr-eabi/bin/arm-zephyr-eabi-objcopy#${CMAKE_OBJCOPY}#' \
  zephyr/soc/ti/simplelink/cc23x0/CMakeLists.txt

# 7. Build. 60 s and 63 s from clean.
export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile
export CROSS_COMPILE=$HOME/zephyr-toolchain/arm-zephyr-eabi/bin/arm-zephyr-eabi-
cd ~/zephyr-ti/ws/zephyr
west build -p always -b lp_em_cc2340r5 samples/bluetooth/peripheral -d ~/zephyr-ti/build-peripheral
west build -p always -b lp_em_cc2340r5 samples/bluetooth/beacon     -d ~/zephyr-ti/build-beacon
```

The workarounds, with the error each one avoids:

- **Step 5, lief.** **Tool or docs.** Without it, installing the CRC tool with its pins fails with `ERROR: Failed to build 'lief' when getting requirements to build wheel`. Without the CRC tool at all (it is not in `requirements-base.txt`), the build links and then stops at `/bin/sh: crc_tool: command not found`.
- **Step 6, objcopy.** **Tool or docs.** The fork's `soc/ti/simplelink/cc23x0/CMakeLists.txt` runs `${ZEPHYR_SDK_INSTALL_DIR}/arm-zephyr-eabi/bin/arm-zephyr-eabi-objcopy`, a path that exists only with a full Zephyr SDK install. Setting `ZEPHYR_SDK_INSTALL_DIR` to work around it made Zephyr look for a full SDK and stop in `FindZephyr-sdk.cmake` (**Our mistake**). The exact error text of both stops was not saved.
- **Step 3, module filter.** **Our mistake.** The first filter left out `mbedtls` and `tinycrypt`, and Kconfig (Zephyr's build-time settings system, the `CONFIG_...` options) stopped. The filter above includes them. The exact Kconfig error text was not saved.

The manifest has a group meant for TI staff only. It is off by default (`group-filter` in `west.yml`), and the project filter above does not turn it on: `+simplelink-hal_ti` matches only that one project name. Do not add a bare `hal_ti` pattern to the filter.

## Times (wall clock, machine waiting)

| Step | Time |
|---|---|
| venv plus west and CMake | 32 s |
| `west init` (shallow) | 31 s |
| `west update` (4 modules, shallow) | 18 s |
| `requirements-base.txt` | 12 s |
| CRC tool plus lief | under 1 min, not timed |
| Build, peripheral (clean) | 60 s |
| Build, beacon (clean) | 63 s |

Three failed build attempts came first: the Kconfig stop on missing mbedtls and tinycrypt (18 s), `crc_tool: command not found` after a good link (66 s), and the Zephyr SDK lookup triggered by setting `ZEPHYR_SDK_INSTALL_DIR` (11 s). They are not counted above.

## Disk

| Path | Size |
|---|---|
| `~/zephyr-ti/ws` (zephyr plus 4 modules) | 663 MB |
| `~/zephyr-ti/.venv` | 179 MB |
| `~/zephyr-ti/build-peripheral` | 40 MB |
| `~/zephyr-ti/build-beacon` | 33 MB |
| **`~/zephyr-ti` total** | **915 MB** |

The compiler is shared with the upstream run and not counted. `zephyr.bin` in each build folder is 1.22 GB (1.2 GB) because it spans from address 0 to the end of the configuration area, which starts at 0x4E020000. It is a sparse file: `du` shows 208 kB or less on disk for each one here. Do not copy it with a tool that fills sparse files, and do not flash it. Use the hex.

## Lock check

Can these images lock the chip? No: every permission is set to allow. TI's fork sets the chip's configuration area (CCFG) from Kconfig, not devicetree. Both Bluetooth builds have:

```text
CONFIG_CC23X0_ALLOW_DEBUG_PORT=y
CONFIG_CC23X0_ALLOW_ENERGY_TRACE=y
CONFIG_CC23X0_ALLOW_FLASH_VERIFY=y
CONFIG_CC23X0_ALLOW_FLASH_PROGRAM=y
CONFIG_CC23X0_ALLOW_CHIP_ERASE=y
CONFIG_CC23X0_ALLOW_TOOLS_CLIENT_MODE=y
CONFIG_CC23X0_ALLOW_RETURN_TO_FACTORY=y
```

In the hex, the permission word at 0x4E020018 is `aaaaaaaa` (every permission set to allow), and debug authorization at 0x4E0207D0 is `0x5A` (debug open). Main flash sectors are not write-protected (`0xffffffff`). The CCFG sector write-protect mask is 0, the same value as the upstream blinky image that flashed and ran on this board. No "LOCK YOUR DEVICE" warning was printed. The fork's sources do not contain that check at all, so its absence proves nothing by itself; the values above are what was checked.
