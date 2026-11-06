# Can the TI LP-EM-CC2340R5 run Thread as an end device or router?

Terms: RCP, MTD, FTD, NCP, NVS, CCFG, `.bss` and linker are defined below and in [docs/vocab.md](../../docs/vocab.md). Errors and fixes: [FAQ.md](../FAQ.md).

Not as a Thread node with TI's configurations on this chip: node builds fail to link for lack of RAM. **As a radio co-processor it works** (2026-10-06). The `ot-rcp` image answers OpenThread's POSIX host on the Mac over the debugger UART at **921600 baud** (`OPENTHREAD/1.4.0.0; CC23XX thread-v1.4-ti-1.0-ea-1.0`) once its CCFG checksums are filled (Zephyr's `ti_ccfg_crc32_calculate.py`), it is flashed with DSLite and it is started with a probe reset.

On the CC2340R52 chip (36 kB of RAM) the RCP links with 8.8 kB to spare. Every Thread node build (MTD, FTD, CLI or NCP) compiles, fits in flash, and then fails to link because RAM runs out, by 7.5 to 23.8 kB depending on build options. **Unknown (suspected: Chip limit):** only TI's configurations were tried, built with this repo's settings (GCC 12.2, board checks removed); a trimmed build might fit; not run. The same builds link on the CC2340R53 (64 kB of RAM); the default builds in the table below leave 7.3 to 20.5 kB to spare.

TI's `ot-ti` UART runs at 921600 baud, not 115200. The unreadable output seen earlier from the radio CLI image at 115200 was probably this; it was not rechecked at 921600.

The RAM numbers line up with TI's listing of this board as RCP only, but TI does not state its reason (**Unknown**). TI enforces the listing with a CMake board check, not a memory check (see "The gate").

Built 2026-10-05 on an Intel Mac (macOS 15.7.9) from TI's `ot-ti`, branch `thread-v1.4-ti` (commit `eaac0deb`, the commit tagged as release `thread-v1.4-ti-2.1`). The build pass flashed nothing. Afterward the radio test CLI (`ot-cli-radio`) was flashed once, with its boot checksums filled by upstream Zephyr's script; it answered with bytes that did not decode at 115200 baud and with nothing at other common rates. **Unknown.** That serial output was observed 2026-10-05; not saved.

The board carries the CC2340R52 chip, 36 kB of RAM; the CC2340R53 is a 64 kB version on a separate board, LP-EM-CC2340R53. 1 kB = 1,024 bytes; 1 GB = 1,024 MB.

Words used on this page (more in [docs/vocab.md](../../docs/vocab.md)):

- **RCP**, radio co-processor: only the radio. A bigger computer runs Thread and drives it over a serial link.
- **MTD**, minimal Thread device: an end device. It does not route.
- **FTD**, full Thread device: can route for other devices.
- **NCP**, network co-processor: runs Thread itself and is controlled by a host.
- **NVS**, non-volatile storage: the flash area where Thread keeps its settings across power cycles.

Contents: [results and link errors](#results), [the gate](#the-gate-what-restricts-the-r52-to-rcp), [local edits](#local-edits), [this repo's toolchain differs from TI's](#this-repos-toolchain-differs-from-tis), [CCFG](#ccfg-debug-and-erase), [exact commands](#exact-commands), [attempts](#attempts-and-wall-times), [a huge .bin file](#trap-a-huge-bin-file), [what to do next](#what-to-do-next), [details: output files, tools, disk](#details).

## Results

| Build (target) | Links on the R52 (36 kB)? | Flash used | RAM used | R52: RAM over or under | R53 (64 kB): RAM under |
|---|---|--:|--:|--:|--:|
| RCP (`ot-rcp`) | yes* | 107.1 kB | 27.2 kB | 8.8 kB under | 36.8 kB under |
| Radio CLI (`ot-cli-radio`) | yes* | 96.1 kB | 25.8 kB | 10.2 kB under | 38.2 kB under |
| CLI MTD (`ot-cli-mtd`) | **no** | 353.3 kB | 46 kB | **10 kB over** | 18 kB under |
| CLI FTD (`ot-cli-ftd`) | **no** | 417.5 kB | 52 kB | **16 kB over** | 12 kB under |
| NCP MTD (`ot-ncp-mtd`) | **no** | 342.5 kB | 43.5 kB | **7.5 kB over** | 20.5 kB under |
| NCP FTD (`ot-ncp-ftd`) | **no** | 404.4 kB | 56.7 kB | **20.7 kB over** | 7.3 kB under |

\* Every image, on both chips, links only after the settings-area fix (edit 3 below). That includes the RCP TI lists as supported.

The GNU linker (`ld`) gives two kinds of link error, shown here with its path cut to the file name:

**Before the settings-area fix**, images stop on the settings area. **Unknown (suspected: Tool or docs)**; see "This repo's toolchain differs from TI's" below. From the R53 run, `ot-rcp`:

```text
ld: section .nvs VMA [0000000000010000,0000000000013fff] overlaps section .text VMA [0000000000000090,0000000000016213]
```

Every image is larger than the 64 kB below the settings area, so every image overlaps it. Seen for `ot-rcp` and `ot-cli-mtd` on the R53 and for `ot-rcp` and `ot-cli-radio` on the R52 (observed 2026-10-05; not saved; the radio CLI also reported a `.rodata` overlap there).

**Node builds on the R52, after the fix**, run out of RAM. **Unknown (suspected: Chip limit).** CLI MTD shown; the other node builds give the same pair with their own byte count:

```text
ld: bin/ot-cli-mtd.out section `.bss' will not fit in region `SRAM'
ld: region `SRAM' overflowed by 10216 bytes
```

Notes on the table:

- Flash counts code, read-only data and the copy of initialized data. RAM is the top of the stack minus the RAM base, so it includes the startup stack (2 kB) and the FreeRTOS heap (3.3 kB). Every number comes from the linker map, and the "over" numbers match the linker's own "overflowed by N bytes" lines.
- Flash is not the problem. The biggest image (CLI FTD) uses 417.5 kB of the 496 kB that sit below the settings area, 84%.
- R52 and R53 images are byte-for-byte the same size. Both build as device `cc23x0r5` with the same FreeRTOS settings. The only difference is the RAM region SysConfig writes into the linker map: `0x9000` (36 kB) for the R52, `0x10000` (64 kB) for the R53.
- The node builds configure on the R52 only after the board check is removed ("The gate"). The NCP rows need `-DNCP_BUILD_ENABLED=ON`.
- `-DNCP_BUILD_ENABLED=ON` also grows the CLI builds (it is a global define). With it on, the R52 CLI MTD is 10.5 kB over and CLI FTD 23.8 kB over. The CLI rows above are from the default build with it off.
- "Radio CLI" is a radio test tool, not a Thread node. It builds by default for this board.

### Where the RAM goes (R52 CLI MTD, from the map)

| Item | Bytes |
|---|---|
| `ot::Instance` (all of OpenThread's state, including 44 message buffers of 128 B = 5,632 B) | 12,256 |
| App task stack (`APP_STACK_SIZE` 2k words in `examples/apps/cli/freertos_main.c`) | 8,192 |
| CLI interpreter | 7,152 |
| FreeRTOS heap (`configTOTAL_HEAP_SIZE` 3400 for CC23X0R5) | 3,400 |
| FreeRTOS timer and idle task stacks and control blocks | 3,056 |
| Startup stack (linker script) | 2,048 |
| UART TX buffer | 1,024 |

The RCP needs none of the Thread state, which is why it fits. Getting an MTD under 36 kB would mean cutting about 7.5 kB from the NCP MTD (smaller app stack, fewer message buffers, smaller tables). That might be possible, but it is untested, it would be a custom configuration, and TI also lists sleepy end devices as unsupported on CC23xx (known issue TIOP-1452).

## The gate (what restricts the R52 to RCP)

**Tool or docs:** the build skips the node apps without a message. It is two CMake checks in `ot-ti`. Nothing in `boards.cmake` or `script/build` blocks it.

1. `examples/apps/cli/CMakeLists.txt`, lines 49 to 56:

   ```cmake
   if((OT_FTD) AND (NOT TI_SIMPLELINK_DEVICE STREQUAL "cc13x2_cc26x2") AND
       (NOT TI_SIMPLELINK_BOARD STREQUAL "LP_EM_CC2340R5"))
       include(ftd.cmake)
   endif()
   if((OT_MTD) AND (NOT TI_SIMPLELINK_DEVICE STREQUAL "cc13x2_cc26x2") AND
       (NOT TI_SIMPLELINK_BOARD STREQUAL "LP_EM_CC2340R5"))
   include(mtd.cmake)
   endif()
   ```

2. `examples/apps/CMakeLists.txt`, line 38: `if(NOT TI_SIMPLELINK_BOARD STREQUAL "LP_EM_CC2340R5")` around `add_subdirectory(ncp)`.

The libraries (`libopenthread-mtd.a`, `libopenthread-ftd.a`) build for the R52 even with the gate in place. Only the app executables are skipped. No warning is printed, so `./script/build LP_EM_CC2340R5` produces just `ot-rcp` and `ot-cli-radio` and says nothing about the rest.

## Local edits

Made in the local `ot-ti` clone only; the full diff is in `ot-ti-local.patch` in this folder.

1. Removed the R52 check from the FTD and MTD `if` lines in `examples/apps/cli/CMakeLists.txt`.
2. Changed the NCP check in `examples/apps/CMakeLists.txt` to `if(TRUE)`.
3. **Settings-area fix, needed for every image:** `src/cc23xx_cc27xx/openthread.syscfg` puts NVS at `0x10000` for all CC23xx parts, under a comment that says "place at the end of Flash". The smallest image is 96.1 kB, and `0x10000` is 64 kB from the start of flash, so code always runs into it. Moved to `0x7C000`, the last 16 kB of the 512 kB flash. Confirmed the original fails the same way on the R53 (`ot-rcp` and `ot-cli-mtd`), so it is not a side effect of the R52 or the gate.

## This repo's toolchain differs from TI's

Any of these could contribute to the settings-area link error above. **Unknown (suspected: Tool or docs).** The smallest image (96.1 kB) is about 32 kB larger than the space below the settings area (64 kB), a gap a compiler change is unlikely to close, but no build with TI's own toolchain was run.

- **Compiler:** GCC 12.2.0 from Zephyr SDK 0.17.4, not the GCC 9 (2020-q2) `ot-ti`'s build script expects, nor the Arm GNU 12.2.rel1 its bootstrap installs on Ubuntu.
- **SysConfig 1.26.3**, not the 1.27.0 `ot-ti` expects.
- **F3 SDK 9.20.00.86**, the submodule `ot-ti` pins (per its license file name), where TI's release notes say lpf3-9.20.00.81. The release notes and the pinned submodule disagree.

Full table under "Details".

## CCFG (debug and erase)

From the generated `ti_devices_config.c`, identical for every build:

- `allowDebugPort`, `allowChipErase`, `allowFlashProgram`, `allowFlashVerify`, `allowReturnToFactory`: all `CCFG_PERMISSION_ALLOW`
- `debugCfg.authorization = CCFG_DBGAUTH_DBGOPEN`, `allowBldr = CCFG_DBGBLDR_ALLOW`
- `chipEraseRetain` keeps no sectors (`0x0` and the required `0xF0000000`)
- Serial ROM bootloader disabled, `pAppVtor` = reset vectors at `0x0`
- **All CCFG CRC fields are 0.** **Tool or docs:** `ot-ti` has no post-build CRC step. The comment says the CRCs must be filled "as a post-build step, or by the flashloader". Whether UniFlash or CCStudio fills them at load time was not tested; the one flash here filled them first with upstream Zephyr's `soc/ti/simplelink/cc23x0/ti_ccfg_crc32_calculate.py`.

The images do not include the NVS area (it is `ALLOC` only), so flashing would not wipe stored settings beyond what the tool's erase does.

## Exact commands

Comments after `#` are notes and wall times, not part of the command. `otbuild.sh` and `mapmem.py` in the clone's `_tools/` folder are local helper scripts, not part of `ot-ti` and not in this repo.

```bash
git clone --depth 1 -b thread-v1.4-ti --recurse-submodules --shallow-submodules \
  https://github.com/TexasInstruments/ot-ti ~/ot-ti                     # 2 min 28 s

# Tools
mkdir -p ~/ot-ti/_tools/gcc/bin && cd ~/ot-ti/_tools/gcc/bin
for f in ~/zephyr-toolchain/arm-zephyr-eabi/bin/arm-zephyr-eabi-*; do
  n=$(basename $f); ln -sf $f ${n/arm-zephyr-eabi/arm-none-eabi}; done
python3 -m venv ~/ot-ti/_tools/venv && ~/ot-ti/_tools/venv/bin/pip install "cmake<4"
export PATH=$HOME/ot-ti/_tools/gcc/bin:$HOME/ot-ti/_tools/venv/bin:$PATH
export TI_SYSCONFIG_ROOT=/Applications/ti/sysconfig_1.26.3

# F3 SDK libraries (driverlib etc.), GCC only                            # 1 min 25 s
make -C ~/ot-ti/third_party/ti_simplelink_sdk/repo_cc23xx_cc27xx CMAKE=$(which cmake) \
  GCC_ARMCOMPILER=$HOME/ot-ti/_tools/gcc TICLANG_ARMCOMPILER= IAR_ARMCOMPILER= \
  SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.26.3/sysconfig_cli.sh GENERATOR=Ninja

# Same configure and build as script/build (wrapper: ~/ot-ti/_tools/otbuild.sh <BOARD> <dir> [-D...])
cmake -GNinja -DOT_COMPILE_WARNING_AS_ERROR=ON -DTI_SIMPLELINK_BOARD=LP_EM_CC2340R5 \
  -DOT_PLATFORM=external -DCMAKE_TOOLCHAIN_FILE=src/arm-none-eabi-m0p.cmake ~/ot-ti -B _build/r5_default
cmake --build _build/r5_default -- -k 0
# NCP: add -DNCP_BUILD_ENABLED=ON. R53: -DTI_SIMPLELINK_BOARD=LP_EM_CC2340R53.

# Memory per image from the map: python3 ~/ot-ti/_tools/mapmem.py <RAM size> <map files>
```

`script/bootstrap` was read and **not run**: it downloads a Linux-only SysConfig `.run` installer, runs OpenThread's own bootstrap (Homebrew on a Mac), and uses `apt-get` and `sudo`. **Tool or docs** (no macOS path).

`script/build` was **not run as is**: it also rebuilds the whole F2 SDK (CC13xx and CC26xx, not needed for this board) and points at `/opt/gcc-arm-none-eabi-9-2020-q2-update`. The F3 SDK make step was run once by hand, then the same CMake command `script/build` uses.

## Attempts and wall times

| # | Attempt | Result | Wall time |
|---|---|---|---|
| 1 | R52 default, upstream source | Configure and compile OK. `ot-rcp` and `ot-cli-radio` fail to link on the `.nvs` overlap (log not saved; attempt 7 shows the same error) | 1 min 34 s |
| 2 | R52 default, NVS moved to `0x7C000` | Both link | 1 min 29 s |
| 3 | R52, FTD and MTD gate removed | Configure and compile OK. `ot-cli-mtd` overflowed by 10 kB, `ot-cli-ftd` by 16400 | 6 s (incremental) |
| 4 | R52, plus NCP gate removed, `-DNCP_BUILD_ENABLED=ON` | `ot-ncp-mtd` over by 7632, `ot-ncp-ftd` over by 21176, `ot-cli-mtd` over by 10768, `ot-cli-ftd` over by 24320 | 1 min 27 s |
| 5 | R53 default (FTD, MTD, RCP, radio CLI) | All four link | 1 min 28 s |
| 6 | R53, `-DNCP_BUILD_ENABLED=ON` | All six link | 2 min 6 s |
| 7 | R53, original NVS at `0x10000` | `ot-rcp` and `ot-cli-mtd` fail with the same `.nvs` overlap | 4 s (incremental) |

The full build logs were not kept.

## Trap: a huge .bin file

**Tool or docs.** Each app's post-build step runs `objcopy -O binary`. On CC23xx the image includes CCFG at `0x4E020000`, so the `.bin` fills the gap from `0x0` with zeros: `ot-rcp.bin` was 1.22 GB (1.2 GB). It was deleted after every build. Anyone building on a full disk will hit this. Use the `.out` or make a `.hex`.

## What to do next

- For a Thread end device or router on this chip family, use the CC2340R53 board (LP-EM-CC2340R53) or a CC27xx part. Neither has been run here.
- To use this board as an RCP, it needs a host running OpenThread's border router. `board-to-board/thread-two-radios-2026-10-06/` ran it with a Mac host: it joined a CC1354P10 leader.
- Find why the flashed radio CLI's serial output does not decode.

## Details

### Output files (LP_EM_CC2340R5, linked)

Built into `out/` in this folder, which is not in the repository. `ot-rcp` was flashed 2026-10-06 (CRCs filled) and ran; `ot-cli-radio` was flashed once, from a copy with its CRCs filled.

| File | SHA-256 |
|---|---|
| `out/ot-rcp.out`, `out/ot-rcp.hex`, `out/ot-rcp.map` | hex `5fc1b18061eadc895787fa236b8a37cb180c51fe394d0b32fe5642e0f629a162` |
| `out/ot-cli-radio.out`, `out/ot-cli-radio.hex`, `out/ot-cli-radio.map` | hex `946c2881649c77915975ba50822e70e6224cd3cb52752ef5e82666f17cfa6085` |

The `.hex` files were made with `arm-none-eabi-objcopy -O ihex` (`ot-ti` does not make them). Maps for the failed and R53 builds were not kept.

### Tools used, and differences from what `ot-ti` expects

| Tool | `ot-ti` expects | Used | Matters? |
|---|---|---|---|
| Arm GCC | `/opt/gcc-arm-none-eabi-9-2020-q2-update` (script default) or Arm GNU 12.2.rel1 (bootstrap, Ubuntu only) | Zephyr SDK 0.17.4 GCC 12.2.0 (`~/zephyr-toolchain/arm-zephyr-eabi`), linked as `arm-none-eabi-*` in `~/ot-ti/_tools/gcc/bin`. Has newlib nano for v6-m. | Compiled clean with warnings as errors. Sizes could differ a little from Arm's build of 12.2. No official toolchain download was needed. |
| SysConfig | 1.27.0 | 1.26.3 (`/Applications/ti/sysconfig_1.26.3`) | No version error or warning. Only warning was the default RNG noise conditioning key. |
| F3 SDK | lpf3-9.20.00.81 (release notes) | The submodule `ot-ti` pins, which is 9.20.00.86 (per its license file name). `/Applications/ti/simplelink_lowpower_f3_sdk_9_21_00_36_LTS` was not used. | The release notes and the pinned submodule disagree on the build number. |
| CMake | any | 3.31.10 from pip in `~/ot-ti/_tools/venv` (no system CMake on this Mac) | CMake 4 was avoided because the SDK uses old `cmake_minimum_required` lines. |
| Ninja, make, python3 | yes | already installed | |

### Disk

| Item | Size |
|---|---|
| Clone with submodules, as cloned | 4.0 GB (1.2 GB of it is `.git`, 2.5 GB the two SDKs) |
| `~/ot-ti` now (one build tree, tools, logs; unused F2 SDK working tree removed with `git submodule deinit`) | 3.4 GB |
| Peak during the run | 5.7 GB, plus 1.22 GB (1.2 GB) per `.bin` before deletion |
| One build tree | 520 to 710 MB |
| CMake venv | 133 MB |
| `thread/out/` | 7.7 MB |

The shallow clone alone is 4.0 GB, so plan for the peak above. To get the F2 SDK back:

```bash
git -C ~/ot-ti submodule update --init third_party/ti_simplelink_sdk/repo_cc13xx_cc26xx
```

To remove everything:

```text
rm -rf ~/ot-ti
```
