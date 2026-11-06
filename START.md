# From the box to a blinking LED on a TI LaunchPad development kit, with an AI assistant

Follow the steps below in order: find the board, fetch only its folder, read what was learned on it, check the tools, back the board up, then make the LED do what the person asks. This file is written for the assistant; the person only pastes one prompt. The person's overview of the repository (folder map, quick start, how results are recorded) is `README.md`. A person who wants no assistant uses the same steps by hand with `frl` (see "Without an assistant" below); nothing in this file is needed for that path.

**The prompt a person pastes into Claude Code** (any folder, nothing installed from this repository):

```text
Set up my TI LaunchPad with Fox Red Labs. Run git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git, then read fox-red-labs/START.md in full and follow it. Ask me before installing anything or writing to the board.
```

That clone holds only the files at the top of the repository (a few MB). Every later step adds just the folders it needs.

## Without an assistant

The same route runs from a terminal with no AI account. On the LP-MSPM0L1306, the one board with a guided route:

```bash
./start.sh mspm0l1306 blink
```

From the sparse clone above, it first adds the folders it needs from this repository (about 25 MB, no new software). Then it lists any tools still missing, with their sizes, before it looks for the board. With two or more boards plugged in it lists their serials and stops; add `--probe <serial>`. It backs up, builds, flashes and checks the LED pattern through the debugger. Details and every result code: `docs/frl.md` and `docs/frl-errors.md`. Add `--assistant claude` or `--assistant codex` only if you want one to drive it.

## Rules for the assistant

1. **Ask before installing anything and before any write to the board.** Say what the command does and how big the download is.
2. **One board at a time, named by its probe serial.** If several boards are on USB, ask which one.
3. **Read `TRAPS.md` now, and the board's `AGENTS.md` before building.** When a command fails, look for its exact error text in the board's `AGENTS.md`, then `FAQ.md`, then `TRAPS.md` before trying anything else.
4. **Never write the boot configuration area** (NONMAIN, CCFG, SCFG) and never erase all of flash "to start clean".
5. **Stop after two failed tries at the same step.** Tell the person what you saw, the exact error, and what you would try next.
6. **Run each command on its own, and read a script before running it the first time.** Chained commands that fetch code and then run it, or a script nobody has read, are often held for approval or blocked by the person's safety settings. Reading it first also tells you what it touches.
7. **Say what happened, not what should happen.** "Flashed" is not "blinking"; ask the person to look, or read the pin.

## The steps

**0. If the person named a build.** A project page's prompt names one file, for example `docs/builds/blink.md`. Fetch it and read it now:

```bash
git -C fox-red-labs sparse-checkout add docs/builds
```

It has one section per board. Each section starts with one `git sparse-checkout add` line that fetches every folder its steps use: run it, then read the board folder's `AGENTS.md`. Do steps 1 to 4 below, then follow that section's steps in place of step 5. Where the section's first step installs CCStudio, that is the setup the run was made on; the no-IDE set in step 3 builds the same image: the MSPM0 build scripts find the SDK and TI Arm Clang the way `frl` does (`tools/frl/toolpaths.sh`, or `FRL_MSPM0_SDK` and `FRL_TICLANG`). The MSPM33 scripts do the same (`FRL_MSPM33_SDK` for their SDK). Run every later command from inside the `fox-red-labs` folder unless a step says `cd`. If the person's board is not in that file, say so before building anything.

**1. Find the board.** Fetch the detector and the board list, then run it (read-only; it lists USB devices and touches no board):

```bash
cd fox-red-labs && git sparse-checkout add tools data
```

```bash
python3 tools/frl/frl.py detect
```

Most LaunchPad kits share one USB identity, so the detector often says only "a TI probe" and its serial. Ask the person for the name printed on the board and run `python3 tools/frl/frl.py identify "<printed name>"`. A board in the table below has notes and a tested route. A board that is not there gets no guided route: `identify` returns FRL-E010, TI's page for the kit, and a handoff. Say plainly that nothing was tested on this board in this repo, that `tools/frl` will not flash it automatically, and that anything you build from TI's own examples is experimental: back up first, change one thing at a time, and report what you saw instead of calling it working. Do not say the steps below will work on it.

**1b. Pick the probe.** With two or more probes on USB, name one with `--probe <serial>`; `frl` stops (FRL-E002) rather than guess.

**2. Fetch the board's folder and read the notes.**

```bash
git sparse-checkout add <folder>
```

Read `<folder>/AGENTS.md` in full, then `<folder>/README.md` (its ten-minute path is the build you will run). If the person is moving code from another chip or vendor, read `PORTING.md` too. When a file the README names is missing, add its folder the same way.

**3. Check the tools, and give the person two ways to get them.** On a board with a guided route (the list and each board's tools are in `docs/setup-coverage.md`), run `python3 tools/frl/frl.py doctor --board <board id>`; the LP-MSPM0L1306 is the worked example: `python3 tools/frl/frl.py doctor --board mspm0l1306`. It reads nothing on the board; it names each missing tool with its version, download page and size, and the total. Then offer the two ways, in this order, and wait for the person to pick:

- **Quick, for a first blink (recommend this):** only what the build needs: the MSPM0 SDK, the TI Arm Clang compiler, SysConfig and xPack OpenOCD. About 0.9 GB to download and 5 GB on disk; have 6 GB free. No IDE: `frl` builds and flashes from the terminal. Only the SDK download needs a myTI login.
- **Full:** CCStudio, TI's IDE, plus the SDK: 2.3 GB to download and about 10 GB installed; have 12 GB free. It adds a debugger window, EnergyTrace power measurement and TI's own AI assistant with its MCP servers. Worth it when the person wants to step through code or measure power. It can be added later, and `frl` uses CCStudio's compiler when it finds it.

Say where each install goes (TI's tools under `/Applications/ti` or `~/ti`, OpenOCD in a folder of its own; nothing changes in shell startup files) and that nothing is installed until they agree. Never run an installer or setup script you have not read. Both ways, and what each AI app adds, side by side: `docs/install-routes.md`; read it with `git show HEAD:docs/install-routes.md`, since the whole `docs` folder is large. On any other board, its `README.md` names the tools and versions it was built with, and `mac-setup/README.md` (add `mac-setup` with `git sparse-checkout add`) has the sizes.

**4. Back the board up.** If the board's row below says the backup is tested:

```bash
python3 tools/frl/frl.py backup --board <board> --probe <serial>
```

It reads every region twice, compares them and keeps the copy in the per-user data folder outside the clone (macOS `~/Library/Application Support/FoxRedLabs/backups`; `docs/frl.md` lists Linux and Windows). Otherwise use the backup step in the board's `AGENTS.md` and keep the files in that same backups folder, in a subfolder named `<board>-<serial>-<date>`, never in a temporary folder. Tell the person where the backup is. If there is no backup step, say so and ask whether to continue. A debugger backup covers the chip's own flash only: anything on a separate flash chip (a demo video, a file system) is not in it.

**5. Make the LED do something the person chose.** Ask: "What should the LED do? For example, two quick flashes, then a pause." Build the board's blink example as its README says, change the timing to match, flash it, and ask the person to look. On the boards with a guided route, `python3 tools/frl/frl.py verify --board <id> --probe <serial>` checks the LED pattern through the debugger without eyes. On success, tell them which file and which line set the pattern.

**6. Offer a project.** Give three projects from the board's row below, one line each: what they will see and what extra parts it needs. Say which were tested on this board and which are only expected to work.

**7. Put it back.** If the person wants the board as it came:

```bash
python3 tools/frl/frl.py restore <backup folder> --probe <serial>
```

## The boards

<!-- start:boards (written by tools/start/render_start.py; do not edit by hand) -->
Each row: the name printed on the board, the id the tools use, the folder to fetch, how to back it up, and the projects tested on it (or expected to work and not run here) with this route.

| Board | Id | Folder | Backup | Projects |
|---|---|---|---|---|
| IWRL6432AOPEVM | `iwrl6432aop` | `iwrl6432boost` | see its `AGENTS.md` | its README's ten-minute path |
| LAUNCHXL-F2800137 | `f2800137` | `c2000-launchpad` | tested (`backup.py`) | tested: Recognize a handwritten digit on the chip, Build and flash from the terminal in one command |
| LAUNCHXL-F28P65X | `f28p65x` | `c2000-launchpad` | see its `AGENTS.md` | ran on this board: Room-occupancy neural nets (500 to 30k weights), Dhrystone 2.1, GPIO toggle and interrupt latency |
| LP-AM13E230 | `am13e230` | `am13e230` | tested (`backup.py`) | ran on this board: One motor-control step, with and without the TMU, CoreMark, Room-occupancy neural nets (500 to 30k weights), plain C, UART at speed |
| LP-AM263P | `am263p` | `am263p` | see its `AGENTS.md` | its README's ten-minute path |
| LP-CC1354P10-1 | `cc1354p10` | `cc1354p10` | tested (`backup.py`) | its README's ten-minute path |
| LP-EM-CC2340R5 | `cc2340r5` | `cc2340r5` | tested (`backup.py`) | tested: Switch a board's LED from Home Assistant over Zigbee, Measure how fast a chip runs (CoreMark), How fast a chip reacts to a signal, Build and flash from the terminal in one command, Run the FreeRTOS real-time operating system, Asset tracking over Bluetooth, Make a Bluetooth game controller from a LaunchPad; expected, not run here: Recognize a handwritten digit on the chip |
| LP-EM-CC2755P10 | `cc2755p10` | `cc2755p10` | tested (`backup.py`) | its README's ten-minute path |
| LP-EM-CC35X1 | `cc3551e` | `cc35x1` | see its `AGENTS.md` | its README's ten-minute path |
| LP-MSPM0C1104 | `mspm0c1104` | `mspm0c1104` | tested (`backup.py`) | its README's ten-minute path |
| LP-MSPM0G3507 | `mspm0g3507` | `mspm0g3507` | see its `AGENTS.md` | tested: Blink an LED, Show colors and read touch on a small screen, Run a small AI model on the chip, Recognize a handwritten digit on the chip, Run the Zephyr real-time operating system; expected, not run here: Print a message from the board on your computer, Send sensor readings over a sub-1 GHz radio, Measure how fast a chip runs (CoreMark), Build and flash from the terminal in one command |
| LP-MSPM0G3519 | `mspm0g3519` | `mspm0g3519` | tested (`backup.py`) | its README's ten-minute path |
| LP-MSPM0G5187 | `mspm0g5187` | `mspm0g5187` | tested (`backup.py`) | its README's ten-minute path |
| LP-MSPM0L1306 | `mspm0l1306` | `mspm0l1306` | tested (`backup.py`) | tested: Blink an LED, Print a message from the board on your computer, Recognize a handwritten digit on the chip, Measure how fast a chip runs (CoreMark), How fast a chip reacts to a signal, Build and flash from the terminal in one command, Run the FreeRTOS real-time operating system |
| LP-MSPM33C321A | `mspm33c321a` | `mspm33c321a` | tested (`backup.py`) | ran on this board: CoreMark, Dhrystone 2.1, Post-quantum and public-key crypto, TI's examples, Hardware AES and CRC, Room-occupancy neural nets (500 to 30k weights), plain C |
| SK-TDA4VM | `tda4` | `tda4` | see its `AGENTS.md` | ran on this board: Get Linux running from the box, TI's model zoo on the deep-learning accelerator, Video into a model with no CPU copy |
| am62l-evm | `am62l-evm` | `am62l-evm` | see its `AGENTS.md` | its README's ten-minute path |
| beagley-ai | `beagley-ai` | `beagley-ai` | see its `AGENTS.md` | its README's ten-minute path |
<!-- /start:boards -->

## When something goes wrong

The person can file what happened at the repository's Issues tab ("It broke here"), with the exact error text. That is how the next person's notes get better.
