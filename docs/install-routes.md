# What do I install to blink an LP-MSPM0L1306 LaunchPad kit, and what does each choice put on my computer?

Four tools from TI and xPack, about 0.9 GB to download and 5 GB on disk, then `./start.sh mspm0l1306 blink` in a terminal, from the copy of this repository that [START.md](../START.md) shows how to clone. That is the quick way: no IDE and no AI account. Everything else on this page is optional, either a bigger TI install with an IDE or an AI app that runs the same steps. Nothing below is installed until you run its installer yourself, or agree when an assistant asks.

Terms: [docs/vocab.md](vocab.md). **frl** is this repository's command-line tool (`tools/frl`). **SysConfig** is TI's tool that writes a chip's setup code. **OpenOCD** is the open-source program that talks to the board's debugger. **CCStudio** is TI's IDE, and **EnergyTrace** is its power measurement. **MCP** is the standard way AI apps call tools on your computer; an **MCP server** is the small program that offers them; a **.mcpb** file packages one for Claude Desktop. **myTI** is TI's free account. A **sparse clone** downloads only part of a repository. The **probe** is the debugger built into the board, which the computer sees over USB. **Codex** is OpenAI's coding assistant, which ships inside the ChatGPT desktop app. **Cowork** is a mode of the Claude Desktop app. A **cold run** is a first run on a computer without TI's tools. A tool marked **destructive** is one an app should ask you about before using. **Unvalidated** means a tool version this route was not tested with. Questions about this board: [mspm0l1306/FAQ.md](../mspm0l1306/FAQ.md).

Sizes were measured on Macs on 2026-10-14; the app sizes are `du` on the installed apps. On disk, 1 GB = 1,024 MB; downloads are as the server reports them.

## The TI tools: quick or full

| | Quick (recommended for a first blink) | Full |
|---|---|---|
| What you install | MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, xPack OpenOCD 0.12.0-7 | CCStudio 21.0.1, which includes the compiler, and the MSPM0 SDK |
| Download | about 0.9 GB: SDK 0.4 GB; compiler 0.31 GB (the macOS Apple Silicon and Linux installers; the Intel Mac one was not downloaded); SysConfig about 0.15 GB (measured on the 1.28.1 installers; the route was validated with 1.26.3); OpenOCD a few MB | about 2.3 GB: CCStudio 1.9 GB, SDK 0.4 GB |
| On disk | about 5 GB: SDK 2.2 GB, compiler 1.8 GB (1.9 GB on Linux), SysConfig 0.9 GB, OpenOCD 10 MB. Have 6 GB free | about 10 GB: CCStudio 8.1 GB, SDK 2.2 GB. Have 12 GB free |
| Accounts | a free myTI login, for the SDK only | a free myTI login, for CCStudio and the SDK |
| How to get it | download each from the links in [docs/frl.md, "Before you start"](frl.md#before-you-start) and run its installer; no script installs this set for you | `mac-setup/bootstrap.sh` installs it with no clicks after you log in to myTI twice in the browser and type "I accept" to TI's licenses, or install by hand ([mac-setup/README.md](../mac-setup/README.md)) |
| Where it goes | TI's tools under `/Applications/ti` or `~/ti` (on Linux, `~/ti` or `/opt/ti`); OpenOCD in a folder of its own. Nothing changes in your shell startup files | `/Applications/ti/ccs2101`, with the SDK beside it |
| What you get | build, flash and check from one terminal command | the same, plus a debugger window, EnergyTrace and TI's own AI assistant with its MCP servers ([TI's CCStudio AI page](https://software-dl.ti.com/ccs/esd/documents/users_guide/index_ai.html)) |
| Tested | Intel Mac: the whole route on the board, with SysConfig 1.26.3 and the compiler from inside CCStudio. The standalone compiler was installed only on Apple Silicon and Linux; images built there passed the check on the board, with SysConfig 1.28.1, which `frl` records as unvalidated | on an Intel Mac; the timed first blink ran this way |

`frl` uses CCStudio's compiler when it finds it, so you can start quick and add CCStudio later without reinstalling anything.

## Who drives: you, or an AI app

Each row runs the same `frl` steps: back up, build, flash and check. The check decides the result, not the app.

| Way | What it adds to your computer | Account | Writes to the board | Tried on 2026-10-14 |
|---|---|---|---|---|
| You, in a terminal (`./start.sh`) | nothing beyond the TI tools. It needs Python 3.9 or later and `make`; on a Mac, `xcode-select --install` gives both | none | when you run the command | Yes, on the board: about 13 s of machine time with the tools already installed |
| Claude Code, with the prompt in [START.md](../START.md) | Claude Code itself, about 250 MB | Claude | it runs `frl` commands. In the cold runs it asked before installing; its prompt before a board write was not seen, because no board was plugged in | **Cold run**, no TI tools and no board: it ran `doctor` and offered quick or full with the sizes above. **On the board** through `./start.sh --assistant claude`: 92 s from start to checked result |
| Codex, with the same prompt | the Codex command-line tool: on its own, or inside the ChatGPT desktop app (1.5 GB) | ChatGPT | as for Claude Code | **Cold run**, no TI tools and no board: it recommended the quick install with the same sizes. **On the board** through this repo's MCP server with `codex exec`: 56 s from start to checked result. A flash it had no way to get approved was refused |
| Codex in the ChatGPT app's own window | this repo's MCP server, added with one `codex mcp add` line (Codex's documented form; the runs passed the same settings another way, so that exact line has not been run). The server runs on your computer and talks to nothing on the network | ChatGPT | the board-writing tools are marked destructive; whether the app then asks you has not been seen | no |
| Claude Desktop or Cowork | Claude Desktop (664 MB), then two extensions of about 180 KB each: a read-only host check, then the board tools. Both carry the tool inside them and ask no questions | Claude | same tools and marks as above | No. An earlier build could not start (see the questions below); the current build is not run |
| CCStudio's own AI assistant | nothing beyond CCStudio (Help > Get Started with AI) | the assistant's | It builds and flashes through CCStudio's MCP servers, not through `frl`, so `frl`'s backup and check do not run | The timed first blink used Claude Code with CCStudio's servers, not `frl` |

The app details are in docs/frl-desktop-apps.md. Every result code is explained in [docs/frl-errors.md](frl-errors.md).

## Your computer

The same tool runs on macOS, Linux and Windows. Only the first command, the install folders and one permission step differ.

| | macOS | Linux | Windows |
|---|---|---|---|
| First command | `./start.sh mspm0l1306 blink` | `./start.sh mspm0l1306 blink` | `py start.py mspm0l1306 blink` in PowerShell |
| Python and make | `xcode-select --install` | `sudo apt install python3 make git` (Ubuntu or Debian) | Python 3.9 or later from python.org (it adds `py`); make: the `gmake` inside CCStudio, or set `FRL_MAKE` |
| Where TI's installers put the tools | `/Applications/ti`, or `~/ti` | `~/ti` (also searched: `/opt/ti`) | `C:\ti` (also searched: `%USERPROFILE%\ti`) |
| xPack OpenOCD | unpack to `~/xpack-openocd-0.12.0-7` | unpack to `~/xpack-openocd-0.12.0-7` | unpack the `.zip` to `%USERPROFILE%\xpack-openocd-0.12.0-7` |
| Probe access | nothing to do | one udev rule, below | the XDS110 USB driver from UniFlash or CCStudio |
| Board's serial port | `/dev/cu.usbmodem<serial>1` | `/dev/ttyACM0` (by name: `/dev/serial/by-id/*<serial>*-if00*`) | `COM<n>`, named "XDS110 Class Application/User UART" in Device Manager |
| Tested | the whole route, on the board | the dry run, `doctor`, `detect` and the tests, in a fresh Ubuntu 24.04 container; no board flashed | not run; the reader copy's CI runs the dry run and checks on `windows-latest` |

A tool in another folder: set `FRL_TI_ROOT` to the folder that holds TI's tools, or one variable per tool (`FRL_MSPM0_SDK`, `FRL_MSPM33_SDK`, `FRL_AM13_SDK`, `FRL_F3_SDK`, `FRL_WIFI_SDK`, `FRL_SYSCONFIG`, `FRL_TICLANG`, `FRL_DSLITE`, `FRL_OPENOCD`, `FRL_TI_OPENOCD`, `FRL_MAKE`). Every build and flash script a project page names reads them, through `python3 tools/frl/toolpath.py <tool>`, so none depends on where this bench installed its tools. `python3 tools/frl/frl.py doctor --board mspm0l1306` names each missing tool with the folder it expects on your computer. `mac-setup/bootstrap.sh` installs on macOS only; on Linux and Windows it points you to `doctor`.

### Linux: let your user open the probe

A new USB probe opens only for root until a udev rule allows your user. OpenOCD ships the rule, which covers the XDS110 (`0451:bef3`, `0451:bef4`) and CMSIS-DAP probes. Run once, then unplug and replug the board:

```bash
sudo cp ~/xpack-openocd-0.12.0-7/openocd/contrib/60-openocd.rules /etc/udev/rules.d/ && sudo udevadm control --reload && sudo udevadm trigger
```

Without it, OpenOCD prints `LIBUSB_ERROR_ACCESS` and `frl` stops with `FRL-E006` and this line. Reading the serial port needs the `dialout` group: `sudo usermod -aG dialout $USER`, then log out and back in.

### Windows: what is different

`start.sh` is a bash script; `start.py` beside it does the same job from PowerShell or Command Prompt. If `py` is not found, use `python`, and if that opens the Microsoft Store, install Python from python.org. TI's example makefiles split paths at spaces, so a home folder with a space in its name needs `FRL_PROJECTS` set to a folder without a space (`FRL-E070` says so). Not run on a Windows computer.

## Questions

### How much disk space do I need?

6 GB free for the quick way, or 12 GB for CCStudio. `python3 tools/frl/frl.py doctor --board mspm0l1306` lists what is still missing on your computer and touches no board. With nothing installed, it prints:

```text
FRL-E020 REFUSED: 4 missing: mspm0_sdk not found; sysconfig not found; ticlang not found; openocd not found
```

That is followed by one line per tool, with its download page and size.

### Do I need CCStudio?

Not for the blink. Install it when you want to step through code in a debugger, measure power with EnergyTrace, or use TI's own AI assistant.

### What happens the first time I run `./start.sh` from the START.md clone?

The clone holds only the files at the top of the repository (4 MB). The first `./start.sh mspm0l1306 blink` adds the folders the tool needs from that same clone, about 25 MB of this repository's files and no new software. Then it lists any missing tools with their sizes before it looks for a board. This was tried on 2026-10-14 on a Mac with no TI tools, from a copy of the files this repository publishes.

### `FRL-E021` says my SysConfig is a different version. Is that a problem?

The route was validated with 1.26.3. Install 1.26.3 and the run is validated. Keep yours and point to it with `FRL_SYSCONFIG`, and the run goes ahead, recorded as unvalidated. Builds with 1.28.1 passed the check on the board. The version on your computer differs from the one this route was tested with.

### Do I need an AI account?

No. `./start.sh` runs the whole route with none. An assistant is optional and runs the same commands.

### Which way should I pick?

For a first blink: the quick install and `./start.sh` in a terminal. Add an assistant if you would rather describe what you want than type commands. Add CCStudio when you need its debugger or power measurement.

### macOS refuses to open the xPack OpenOCD because it was downloaded from the internet

Run `xattr -dr com.apple.quarantine ~/xpack-openocd-0.12.0-7` once (use your folder if you unpacked it elsewhere). macOS marks downloaded programs; the wording of its warning varies by version, and it was not seen on the Mac used here.

### `make: command not found`, or `python3` opens an install window

On a Mac, run `xcode-select --install` and accept Apple's command line tools; they include `make` and Python. On Ubuntu or Debian, `sudo apt install python3 make`. The route needs both, and a new computer may not have them.

### Claude Desktop cannot connect to the Fox Red Labs extension, and its log says "Couldn't start for Cowork and Code sessions. Error: Connection closed"

Open its log, `~/Library/Logs/Claude/mcp-server-Fox Red Labs host check.log` on a Mac (on Windows: not run); the lines under that error say why the server stopped. Check that `python3 --version` prints 3.9 or later, then remove the extension and install the newest `.mcpb`. An earlier build asked for the repository folder and could not start when given the wrong one; the current build asks nothing. If the current build fails this way, the cause is **Unknown**: include the log's last lines in a report.

## Limits

- Windows, beyond the checks in the reader copy's CI.
- Flashing from Apple Silicon or Linux.
- Any board other than the LP-MSPM0L1306 through `frl`.
- How long the quick install takes.

Next: install the quick set from [docs/frl.md, "Before you start"](frl.md#before-you-start), then run `./start.sh mspm0l1306 blink`.
