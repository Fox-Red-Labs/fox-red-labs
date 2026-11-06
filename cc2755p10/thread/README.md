# OpenThread on the LP-EM-CC2755P10

Terms: [docs/vocab.md](../../docs/vocab.md) and the [CC2755P10 FAQ](../FAQ.md). 1 kB = 1,024 bytes. FTD is a full Thread device, MTD a minimal one.

**Works, one board, 2026-10-09:** `ot-cli-ftd` runs on the LP-EM-CC2755P10 and forms its own Thread network. It reached `leader` within 12 s of `thread start` (6 to 8 s on the evening run). Flash it from the `.hex` that `make_flash_hex.py` writes, not from the `.out`: the build leaves the CCFG and SCFG checksums at 0, and a CC27xx with bad checksums keeps its debug port closed. `cc2755p10/restore.sh` puts the shipped image back. TI's `ot-ti` repository, release `thread-v1.4-ti-2.1`, builds the full Thread device (`ot-cli-ftd`) and the minimal Thread device (`ot-cli-mtd`) for `LP_EM_CC2755P10` with no edits.

## What was built

| Item | Value |
|---|---|
| Source | `https://github.com/TexasInstruments/ot-ti`, tag `thread-v1.4-ti-2.1`, commit `eaac0deb4b3369742f43416c26531cc16938d8c6` |
| OpenThread | `c9c19aa9fa5877cf1532c35a584618900e5c99c7` |
| TI SDK (submodule) | `simplelink-lowpower-f3-sdk` `lpf3-9.20.00.81-1-gfa5ded4a` (not the installed 9.21 SDK) |
| FreeRTOS, mbedTLS | `460e953076b9ceff74fc5583e3da12140ca05690`, v2.28.0 |
| Compiler, SysConfig | Arm GNU Toolchain 12.2.Rel1 (GCC 12.2.1), SysConfig 1.27.0 |
| Build host | An arm64 Mac, `ubuntu:22.04` container forced to `linux/amd64`, built 2026-10-09 |

| Image | Flash (text + data) | RAM (data + bss) | Of 928.0 kB flash / 163.0 kB SRAM (linker regions) |
|---|--:|--:|--:|
| `ot-cli-ftd.out` | 435.0 kB | 109.9 kB | 46.9% / 67.4% |
| `ot-cli-mtd.out` | 372.9 kB | 103.8 kB | 40.2% / 63.7% |

Sizes are `tiarmsize` on the ELF; bss includes the reserved heap and stacks, as on the CC1354P10. Both ELFs carry the `.ccfg` and `.scfg` sections. The ELF files are 50.9 MB (`ot-cli-ftd.out`) and 39.1 MB (`ot-cli-mtd.out`) and are not in the repository. The `.bin` files `ot-ti` also writes are 1.22 GB each (they span the CCFG address) and were discarded; flash the `.out` ELF.

Times (a Mac under x86 emulation): the successful run took 3 min 59 s end to end, with the clone, submodules (60 s), bootstrap (66 s) and the full build (111 s; SDK libraries plus Thread images). The first, cold run took 11 min 12 s but stopped at the SysConfig trap below, so a cold start to images is about 11 min 12 s plus a re-run of the build step (an estimate, not measured). The work folder was 7.1 GB; delete it after the build.

## One-command build

On any Ubuntu 22.04 x86-64 machine or container with the work folder mounted at `/work`:

    docker run --rm --platform linux/amd64 -v "$PWD/work":/work ubuntu:22.04 bash /work/build-on-ubuntu.sh

Copy `cc2755p10/thread/build-on-ubuntu.sh` into `work/` first. It installs the packages, SysConfig 1.27.0 to `/opt/ti`, clones the tag, runs `./script/bootstrap` and `./script/build LP_EM_CC2755P10`, and prints the commits and sizes. Images land in `work/ot-ti/build/bin/`.

Trap: TI's `bootstrap` installs SysConfig under `/opt/ti` when run as root, but `script/build` only checks `~/ti/` and `/opt/ti/` by directory test and then passes an empty path to CMake. The script exports `TI_SYSCONFIG_ROOT=/opt/ti/sysconfig_1.27.0` to avoid the failure `sysconfig_cli.sh: not found` that the first run hit.

## Flash with DSLite

The board sits on a standalone LP-XDS110; put its serial number in `cc2755p10/CC2755P10.ccxml`. From the repo root:

    python3 cc2755p10/thread/make_flash_hex.py <ot-cli-ftd.out> <shipped CCFG backup> <SDK SCFG> ot-cli-ftd-flash.hex
    /Applications/ti/uniflash_9.6.0/dslite.sh --mode flash -c cc2755p10/CC2755P10.ccxml -e -f -u ot-cli-ftd-flash.hex

The script takes main flash from the ELF and the CCFG and SCFG from files with their checksums filled (the board's own CCFG backup and the SDK's default SCFG, named in `cc2755p10/restore.sh`). It refuses unless the ELF's configuration matches them byte for byte apart from the checksums and the CCFG user record. 19 s on 2026-10-09.

## What was tested, and what is not

The console is the backchannel UART at **921600 baud** (`ot-ti/src/uart.c`); 115200 prints garbage. Use `tools/ot_cli.py <port> <cmd> ...` (it sets the speed with `IOSSIOSPEED`).

1. **Form a network alone: done.** `version`, `dataset init new`, `dataset commit active`, `ifconfig up`, `thread start`, wait 12 s, then `state` (`leader`), `panid`, `channel`, `networkname`, `ipaddr`. The same sequence as `cc1354p10/thread/run_thread_leader.sh`. The board became leader within 12 s on 2026-10-09 (6 to 8 s on the evening run). The version string is `OPENTHREAD/1.4.0.0` with a CC27xx build tag. The CC1354P10 console went silent when that chip entered standby; if this one does the same, set the power policy to never-standby in `openthread.syscfg`.
2. **Join a second device: not run.** No second Thread node was on the bench for this chip, so no router join was recorded. The CC1354P10 and CC2340R5 pair is tested in [board-to-board/thread-two-radios-2026-10-06](../../board-to-board/thread-two-radios-2026-10-06/README.md).
3. **Join an ESP32-C6 running OpenThread: not run.** It is the only test that checks TI's stack against a different vendor's.
4. **Matter over Thread: see [matter-smoke](../matter-smoke/README.md).** Apple Home and Google Home pairing are not run; they need TI's Matter fork and a Thread border router the home hub can reach.

Every test row goes in `data/results/cc2755p10.csv`, with a row and reason for any test that cannot run.

## Build safety

The build runs as root inside a throwaway `ubuntu:22.04` container (`docker run --rm`), never on a host. The script checks TLS on every download, refuses to run TI's SysConfig installer unless its SHA-256 matches `SC_SHA256` (take the value from TI's SysConfig download page; with no value set the script prints the file's hash and stops), and checks out ot-ti at commit `eaac0deb4b3369742f43416c26531cc16938d8c6` even though it clones the release tag. TI's own `script/bootstrap` still downloads the Arm toolchain and Python packages its own way; that part is TI's script, unchanged, and is one reason it stays inside the throwaway container.
