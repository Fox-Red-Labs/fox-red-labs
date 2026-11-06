# Mac setup for TI boards: tools, versions and disk space on an Intel MacBook

The software to install on a Mac before a TI board arrives, with the version, download size and disk space for each. Tier 1 is enough for a first blink; Tier 2 adds the wireless, radar, motor and display kits. Terms: [vocab](../docs/vocab.md).

## This Mac

Captured 2026-10-04 with `sw_vers`, `sysctl` and `df -h`.

| | |
|---|---|
| macOS | 15.7.9 Sequoia (build 24G830) |
| CPU | Intel Core i7-8750H @ 2.20GHz, x86_64 |
| RAM | 16 GB |
| Disk | 233 GB, 13 GB free (94% used) |

Is CCStudio 21.0.1 supported here? Yes. TI's public system requirements page lists macOS 26, 15 and 14 as validated, and an x86_64 multi-core processor. This Mac is x86_64 on macOS 15, so CCStudio runs natively. No Rosetta.

TI's numbers from the same page: minimum 8 GB RAM and 3.0 GB disk, recommended 16 GB+ RAM and 6.0 GB+ disk. RAM meets the recommended line. Disk is the limit: 13 GB free covers Tier 1, and Tier 2 needs space cleared first. `setup.sh` prints free space before anything installs.

Intel rules for this Mac:

- No Rosetta step. Every TI tool is already x86_64.
- Vendor installers over Homebrew. Homebrew does not support this Intel Mac with prebuilt packages and compiles from source, so nothing here uses `brew install`.
- Where a TI download offers two macOS builds, take the Intel one (for the Wi-Fi Toolbox that is `macos_intel`).

Versions below were read off the public download pages on 2026-10-04. Re-check the version line on each page before installing; TI ships often.

Two tiers. Tier 1 is everything a first blink needs. Tier 2 is the rest of the kit. Do Tier 1 first; Tier 2 can wait for the boards it serves.

The one-command path: `./bootstrap.sh` for Tier 1, `./bootstrap.sh --all --zephyr` for everything. You log in to myTI twice in the browser (CCStudio and the MSPM0 SDK). The script waits for each download, installs everything with no clicks, fetches and checksums the six packages that need no login, and ends with the four things only you can do. `--dry-run` shows the plan and changes nothing. It asks you to type "I accept" first, because no-click installs accept TI's licenses for you.

Status: built from the steps below. The Tier 2 half is exactly what was run. The CCStudio no-click install uses TI's documented flags and a smaller component list than the full install below, and has not been run from a clean Mac. `setup.sh` is the older, click-through version.

## Disk space you need

Measured on this Mac, not estimated. One row per install. "Peak" is the most free space the step needs at one moment: the downloaded installer plus the installed files side by side, before the installer is deleted.

| Step | Download | Installed | Peak needed | Notes |
|------|---------:|----------:|------------:|-------|
| CCStudio 21.0.1, every device family | 1.9 GB | 8.1 GB | about 10 GB | TI's page says 3.0 GB minimum, 6.0 GB+ recommended. The full install is larger than both. Of the 8.1 GB, compilers are 2.9 GB, debug and device support 2.1 GB, the IDE app 2.0 GB. |
| MSPM0 SDK 2.11.00.07 | 0.4 GB | 2.2 GB | about 2.6 GB | 90,000 files. Installs to `/Applications/ti/mspm0_sdk_2_11_00_07`. Installed size is 5.5 times the download. |
| C2000Ware 26.02.00.00 LTS (ti.com installer, login needed) | 0.3 GB | 1.6 GB | about 2 GB | Installs to `/Applications/ti/c2000/C2000Ware_26_02_00_00` in about 2 minutes unattended (`installbuilder.sh --mode unattended`). Needed only for CCStudio and its MCP servers to see C2000Ware; the login-free GitHub copy builds from a terminal but the IDE does not register it. Measured 2026-10-08. |
| Wi-Fi Toolbox 4.3.23 | 0.1 GB | 0.7 GB | 0.8 GB | x86_64 build, runs on Intel. |
| AM13E2x SDK 26.01.00.03 | 0.04 GB | 0.2 GB | 0.25 GB | |
| MSPM33 SDK 1.04.00.00 | 0.27 GB | 0.8 GB | 1.1 GB | |
| UniFlash 9.6.0 | 0.3 GB | 1.9 GB | 2.2 GB | Installed size is 6 times the download. |
| Low Power F3 SDK 9.21.00.36 | 0.55 GB | 2.0 GB, plus 2.2 GB of extras | 4.8 GB | Brings its own SysConfig 1.28.0 (0.9 GB) and a second Arm compiler (1.3 GB). |
| Wi-Fi SDK 10.20.00.39 | 0.7 GB | 0.7 GB, plus 1.0 GB of extras | 2.4 GB | Brings its own SysConfig 1.26.3. |
| MMWAVE-L-SDK 05.05.04.02 (IWRL6432 radar) | 0.45 GB | 0.74 GB in the VM; 0.61 GB copied to `~/ti` without its docs | about 1.2 GB plus the VM (20 GB disk on the host) | Linux or Windows installer only: install it in an x86_64 Ubuntu 22.04 VM and copy the tree over (`portability/everywhere/PORTING.md`). myTI login and export approval. |
| Other vendor: ST STM32CubeCLT 1.22.0 (NUCLEO-U083RC run) | 0.76 GB (`.zip`; Safari also leaves a 0.76 GB `.tar.gz` and the 0.77 GB `.tar`) | 3.3 GB | about 5.6 GB with all three download copies on disk | Installs to `/opt/ST/STM32CubeCLT_1.22.0`. The package declares 3.4 GB; ST's release note RN0132 says 1.7 GB. Unsigned on macOS 15. Measured 2026-10-08. |
| Other vendor: STM32CubeU0 v1.3.0 examples (GitHub, sparse) and Zephyr's `hal_stm32` | download is the clone | 0.27 GB and 1.2 GB | 1.5 GB | No login. Measured 2026-10-08. |
| Other vendor: NXP MCUXpresso SDK 26.09.00 by the GitHub route (`west init`, then `west update_board --set board frdmmcxa153`; FRDM-MCXA153 run) | the clone itself, 89 s on this Mac | 2.3 GB in `~/mcuxsdk/mcuxsdk`, plus 10 MB manifests and 19 MB venv | about 2.7 GB (free space fell from 7.09 to 4.43 GB) | No login. The SDK Python requirements failed on `cbor2` (wheel build) the first time. Measured 2026-10-11. |
| Other vendor: NXP LinkServer 26.9.130 macOS package (FRDM-MCXA153 run) | 32,144 kB listed; not downloaded | not installed | not measured | Login wall: the download link redirects to NXP's login page, direct `lgfiles` paths return 404, and the installer needs an administrator password. Checked 2026-10-12. |
| Other vendor: NXP MCUXpresso for VS Code extension 26.9.44 (FRDM-MCXA153 run) | 4.5 MB for the extension; about 30 MB with its three dependencies, 6 s | 4.5 MB (30 MB with dependencies in `~/.vscode/extensions`) | about 30 MB | No login for the extension; its tool downloads and LinkServer were not run. Installed with `code --install-extension` and removed again. Measured 2026-10-12. |
| TI PRU compiler (PRU-CGT) 2.3.3, macOS installer (AM263P PRU display test) | 17.9 MB (`.zip`) | 126 MB in `~/ti/ti-cgt-pru_2.3.3` | about 150 MB | No login. `installbuilder.sh --mode unattended --prefix ~/ti` inside the app; about a minute. Measured 2026-10-11. |
| Other vendor: pyOCD 0.45.1 with the NXP MCXA153 pack (open-source flasher chosen over NXP LinkServer 26.9.130; FRDM-MCXA153 run) | pyOCD wheel 14.8 MB (PyPI), pack 1.3 MB; the pack index download is 33 MB of index plus about 118 MB of descriptor files | 76 MB venv + 1.3 MB pack + 33 MB index = about 110 MB (the descriptors can be deleted after the pack install) | about 230 MB (venv, index and all descriptors) | No login, no password. LinkServer: 32 MB PKG, installed size not stated on the page; not installed (needs an administrator password). `pyocd pack find` takes about 5 minutes the first time. Read test: 128 kB flash dump identical to the OpenOCD dump. Measured 2026-10-11. |
| Other vendor: Arm GNU Toolchain 14.2.Rel1 (the compiler NXP lists for its SDK; FRDM-MCXA153 run) | 138 MB `.tar.xz` from developer.arm.com, no login, checksum matched | 1.0 GB in `~/arm-gnu-toolchain` | about 1.2 GB | Download 10 s. Measured 2026-10-11. |
| Other vendor: Zephyr `hal_nxp` module (`west update hal_nxp`; FRDM-MCXA153 run) | the clone | 1.3 GB in `~/zephyrproject/modules/hal/nxp` | 1.3 GB | 72 s. Needed to build Zephyr for NXP boards. Measured 2026-10-11. |
| Other vendor: STM32duino 3.0.0 core (arduino-cli) | about 0.34 GB | 2.5 GB | about 2.8 GB | `~/Library/Arduino15/packages/STMicroelectronics`. Remove it with `arduino-cli core uninstall STMicroelectronics:stm32` (frees 2.5 GB). Measured 2026-10-09. |
| Other vendor: ESPHome 2026.9.1 (ESP32-C6 proxy) | 7 MB wheel plus 64 MB of pip cache; the first compile fetched ESP-IDF 5.5.5 and its tools into `~/Library/Caches/esphome` | 0.23 GB venv plus 2.7 GB cache | about 3.0 GB | Python 3.12 venv. `pip install --only-binary cbor2 esphome==2026.9.1` (plain pip tries to build cbor2 and wants a Rust compiler). ESPHome does not use PlatformIO's toolchain any more; `~/.platformio` stayed at 24 kB. Measured 2026-10-11. |
| Other vendor: ESP-IDF v6.1 for the ESP32-C6 (GCC path; delete `~/.espressif` to free 4.2 GB when done) | 0.45 GB (GCC 15.2.0 388 MB, gdb 53 MB, OpenOCD 3 MB, ROM ELFs 3 MB) plus the 0.7 GB git checkout | 3.2 GB in `~/.espressif` (tools 2.3 GB, esp-idf 0.73 GB, Python env 0.12 GB) | about 3.7 GB; **6.3 GB and rising** with EIM's non-interactive defaults | EIM 0.20.0 `eim install -n true -t esp32c6` also installs esp-clang (2.8 GB) and keeps the archives in `dist/` (0.9 GB); pass `--idf-tools riscv32-esp-elf,riscv32-esp-elf-gdb,openocd-esp32,esp-rom-elfs` or finish with the tree's own `install.sh esp32c6`. Needs `--skip-prerequisites-check true` without dfu-util (unused by the C6). Measured 2026-10-11. |
| Other vendor: Raspberry Pi pico-sdk 2.3.1, pico-examples sdk-2.3.1, picotool 2.3.1, RISC-V toolchain 16.1.0 (Pico 2) | 0.24 GB downloaded: shallow git clones (SDK 61 MB with TinyUSB, examples 23 MB), picotool zip 1.9 MB, RISC-V toolchain zip 235 MB (pico-sdk-tools release v2.3.1-0, read 2026-10-11) | 0.83 GB in `~/pico` (SDK 61 MB, examples 23 MB, picotool 6 MB, RISC-V toolchain 723 MB); the Arm side reuses the Zephyr SDK's GCC 12.2 through `PICO_GCC_TRIPLE`, 0 MB new | about 1.1 GB with the zips kept; build trees add ~0.1 GB per bench configuration | Versions read from Raspberry Pi's release pages on 2026-10-11 (pico-sdk 2.3.1 of 2026-09-04, pico-sdk-tools v2.3.1-0 of 2026-09-05). `xattr -dr com.apple.quarantine` on the toolchain and picotool after unzipping. Zips in `~/pico/dl` can be deleted |
| Other vendor: CMSIS-DSP, CMSIS_6 and Raspberry Pi's FreeRTOS-Kernel fork (Pico 2 FFT and RTOS runs) | shallow git clones read 2026-10-11: CMSIS-DSP 244 MB (git main of 2026-09-10), CMSIS_6 28 MB, FreeRTOS-Kernel fork 20 MB (RP2350_ARM_NTZ and RP2350_RISC-V ports; upstream FreeRTOS-Kernel has no RP2350 port) | 0.29 GB in `~/pico` | about 0.3 GB | CMSIS-DSP is built from source with the project's own CMake (about 150 files); `list(FILTER ...)` drops the aggregate `.c` files and `_neon` sources |
| **Everything, Tier 1 and Tier 2** | 4.3 GB | 20 GB | | All under `/Applications/ti`. |
| **Running total, Tier 1** | 2.3 GB | 10.3 GB | | Delete each installer after it finishes to get its download size back. |

Rule of thumb: have 12 GB free before starting Tier 1 and 25 GB for the whole kit. Restart the Mac first if it has been up for days; this one had 12 GB of disk tied up in swap. After CCStudio installed, free space on this Mac fell from 13 GB to 2.5 GB with the installer file still on disk; deleting the installer, package caches and old exports gave it back.

**VS Code and a repo that links into the SDKs (2026-10-10).** Microsoft's C/C++ extension (`ms-vscode.cpptools` 1.34.4) indexes every folder it can reach. A repo that symlinks into TI's SDKs, the compiler and the debugger's target database gives it tens of thousands of files outside the repo, and it wrote a browse database at about 10 MB a minute until the disk was full. If you open such a folder in VS Code, turn the indexer off for that workspace before you do anything else: in `.vscode/settings.json` set `"C_Cpp.intelliSenseEngine": "disabled"` and `"C_Cpp.default.browse.databaseFilename": ""`. Check free space with `df -h /System/Volumes/Data` after opening a new folder. This is a Mac setup issue with a third-party extension, not a TI one.

**Moving an SDK you are not using to another machine.** The CC13xx/CC26xx SDK is 2.6 GB in 50,585 files. Copy it to another machine, compare file counts and byte totals, then delete the local copy; do not move anything a running job has open.

## Tier 1: first blink on an MSPM0 LaunchPad

| # | Step | What exactly | Why |
|---|------|--------------|-----|
| 1 | Host check | `sw_vers`, `uname -m`, `df -h /System/Volumes/Data`. Confirm macOS 14, 15 or 26 and at least 6 GB free. | CCStudio and every TI compiler and debug driver are x86_64 on macOS, so an Intel Mac runs them natively. TI validates CCStudio 21.0.1 on macOS 14, 15 and 26. |
| 2 | Command line tools | `xcode-select --install`. Already installed? `git --version` and move on. | Gives you git, which the CCS Claude Code integration requires. No Homebrew on this Mac. |
| 3 | Claude Code | `curl -fsSL https://claude.ai/install.sh \| bash`, then `claude` and log in with your Claude subscription. Already installed? `claude --version` and move on. | The CCS integration installs only the editor extension, not the CLI. Native installer is Anthropic's recommended method; npm install is deprecated. |
| 4 | CCStudio IDE 21.0.1 | <https://www.ti.com/tool/download/CCSTUDIO/21.0.1>, "macOS x86_64 single file installer." Install as your user. On this Mac the default location was `/Applications/ti/ccs2101`, not `~/ti`. | The IDE. Theia-based. Released 2026-08-24. Runs natively on Intel. Use TI's installer, not Homebrew. |
| 5 | Debug probe drivers | Nothing to run on this Mac. The 21.0.1 macOS install has no `install_scripts` folder, and the installer log shows the debug probe pack installed with drivers on. Confirm with `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e` once a board is plugged in. | The earlier checklist carried a driver script step that does not exist in this install. Kept as a row so the check still happens. |
| 6 | MSPM0 SDK 2.11.00.07 | <https://www.ti.com/tool/download/MSPM0-SDK>, the macOS `.app.zip` (Jul 10 2026). Default location so CCS discovers it. | The blink example, DriverLib, SysConfig metadata for every MSPM0 LaunchPad. |
| 7 | Turn on the AI assistant | In CCS: Help > Get Started with AI > Claude Code. Leave Auth Token and Base URL blank, tick "Enable CCStudio ecosystem MCP servers," Apply, then log in in the chat panel. | Four MCP servers (project, debug, SysConfig, serial) let the agent build, flash, and watch the board. |
| 7a | Claude Code outside CCStudio | `ccs-mcp.json` in this folder names CCStudio's four servers (project, debug, SysConfig, serial) for Claude Code in a terminal or VS Code: `claude "<your prompt>" --mcp-config mac-setup/ccs-mcp.json --strict-mcp-config` from the repository root, with CCStudio open (prompt first: the flag takes a list; `--strict-mcp-config` loads only these four servers, so other assistant servers on your Mac stay out of the run). Verified headless on 2026-10-12: the servers load and the assistant answers in 40 s. Paths assume `/Applications/ti/ccs2101`; change them if CCStudio is elsewhere. | How the first blink ran: Claude Code in VS Code calling CCStudio's build and flash servers. The page for a reader: `site/start-with-ai.html`. |
| 8 | Node.js | The macOS installer (`.pkg`) from <https://nodejs.org>, current LTS. Already installed? `node --version` and move on. | `npx` for agent skills and JavaScript tooling. |

Done when: CCS opens, the Claude Code chat panel answers a question, `claude --version` and `node --version` both print, `ls /Applications/ti` shows `ccs2101`, and the MSPM0 SDK folder `mspm0_sdk_2_11_00_07` exists where its installer put it.

## Tier 2: the rest of the kit

Install each one when you get its board. All have macOS installers unless noted.

| # | Step | What exactly | For |
|---|------|--------------|-----|
| 10 | SimpleLink Low Power F3 SDK | <https://www.ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK>, macOS installer, 536 MB. Page showed 9.21.00.36 LTS (Aug 26 2026) on the evening of 2026-10-04. Hubble's integration guide pins 9.20, so the satellite demo may need that older version. | CC2340R53 (BLE), CC2755P10 (BLE, Matter, satellite). |
| 11 | SimpleLink Wi-Fi SDK | <https://www.ti.com/tool/download/SIMPLELINK-WIFI-SDK>, macOS installer, 685 MB. Page showed 10.20.00.39 (Sep 3 2026) on the evening of 2026-10-04. Install the latest; a CC35X1 board revision may need a different SDK version. | CC35X1 Wi-Fi 6. |
| 12 | SimpleLink Wi-Fi Toolbox | <https://www.ti.com/tool/download/SIMPLELINK-WIFI-TOOLBOX>. The live page on 2026-10-04 shows 4.3.23 (Sep 4 2026) with a single macOS file, 95 MB, and no `macos_intel` file. The older 4.2.4 had separate Intel and Apple Silicon files. Whether the 4.3.23 file runs on an Intel Mac is checked after download, before install. | Mandatory. The CC35xx must be "activated" with a root-of-trust key through this tool before CCS can flash it. |
| 13 | MSPM33 SDK 1.04.00.00 | <https://www.ti.com/tool/download/MSPM33-SDK>, macOS `.app.zip` (Jul 6 2026). | LP-MSPM33C321A, the onboard OLED and LVGL. |
| 14 | AM13E2x SDK 26.01.00.03 STS | <https://www.ti.com/tool/download/AM13E2X-SDK>, macOS `.dmg` (Jul 15 2026). A short-term-support (STS) build. | LP-AM13E230 motor control. |
| 15 | SysConfig 1.28.0 standalone | <https://www.ti.com/tool/download/SYSCONFIG>, macOS installer. Optional; CCS bundles one. | Only if a SDK asks for a newer SysConfig than CCS ships. |
| 16 | UniFlash 9.6.0 | <https://www.ti.com/tool/download/UNIFLASH>, macOS installer (Jul 1 2026). | Flashing outside CCS. Radar flashing from macOS is unverified; see the gap below. |
| 17 | CP210x VCP driver | the bridge chip maker's "CP210x VCP drivers" download page. After install, approve it under System Settings > General > Login Items & Extensions > Driver Extensions. | The IWR6843AOPEVM serial port. Without this the radar never shows up. |
| 18 | TI Cloud Agent + browser extension | <https://dev.ti.com>, follow the prompt in Chrome. | The browser tools: mmWave Demo Visualizer, the MSPM0 motor-control GUI, Edge AI Studio cloud. |
| 19 | Edge AI Studio (cloud) | <https://dev.ti.com/edgeaistudio> with your myTI login. No macOS desktop build exists; the cloud version is full-featured. | TinyEngine training path. |
| 20 | Hubble Network account | <https://dash.hubble.com>, Sandbox first, then the Live 45-day trial. | Satellite demo. Start early; a Hubble rep may have to enable satellite on the org. |

### The one real macOS gap

MMWAVE-L-SDK (the low-power radar SDK for the IWRL6432) ships Windows and Linux installers only (05.05.04.00, Aug 2025). The IWRL6432 visualizer is also Windows-only. The IWR6843 family works from a Mac through the browser visualizer. To build radar firmware rather than run the shipped demos, use an x86 Windows or Linux machine or an Ubuntu VM.

## Accounts and access to sort out once

- myTI: a free login that gates every TI download.
- Claude: a Claude subscription login works inside CCS. No API key needed.

## Sources

- CCStudio 21.0.1 system requirements (validated macOS versions, x86_64, RAM and disk): <https://software-dl.ti.com/ccs/esd/documents/users_guide_ccs_21.0.1/ccs_overview.html>
- CCStudio AI chapter: <https://software-dl.ti.com/ccs/esd/documents/users_guide/index_ai.html>
- Claude Code install: <https://code.claude.com/docs/en/>
- TI download pages linked per row, all read 2026-10-04.
