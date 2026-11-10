# Which boards have a guided setup, and what each one needs

<!-- written by tools/start/route_summary.py from data/routes.json, data/results, tools/benchmark/recipes.json, data/recovery.json and site/data/code.json; do not edit by hand -->

13 kits have a guided route: `./start.sh <board id> <job>` (Windows: `py start.py <board id> <job>`) backs up the board, builds TI's example with your change, writes it and checks the result on the board. The other 13 boards are set up by hand from their folder's `README.md`.

Works means the guided route has a hardware run in the board's results file. Could work means the route is in the registry but that run is not recorded; the row says what is missing. A build or a test with a fake probe is never counted as a hardware run.

Contents: [guided routes](#guided-routes), [what each guided board needs](#what-each-guided-board-needs), [computers](#computers-the-guided-route-has-run-on), [boards set up by hand](#boards-set-up-by-hand). Terms: [docs/vocab.md](vocab.md).

## Guided routes

| Board | Id | Job | Status | Evidence | Check |
|---|---|---|---|---|---|
| LP-AM13E230 | `am13e230` | `blink` | Works | 3 runs on hardware, 2026-10-24 ([row](../data/results/am13e230.csv)) | LED pin timed through the debugger |
| LP-CC1354P10-1 | `cc1354p10` | `blink` | Works | 3 runs on hardware, 2026-10-25; route marked partial in data/routes.json, limits in docs/frl.md ([row](../data/results/cc1354p10.csv)) | LED pin timed through the debugger |
| LP-EM-CC2340R5 | `cc2340r5` | `blink` | Works | 3 runs on hardware, 2026-11-02; route marked partial in data/routes.json, limits in docs/frl.md ([row](../data/results/cc2340r5.csv)) | LED pin timed through the debugger |
| LP-EM-CC2340R5 | `cc2340r5` | `print-a-message` | Works | 3 runs on hardware, 2026-10-24 ([row](../data/results/cc2340r5.csv)) | message read on the serial port |
| LP-EM-CC2755P10 | `cc2755p10` | `blink` | Works | 3 runs on hardware, 2026-11-02; route marked partial in data/routes.json, limits in docs/frl.md ([row](../data/results/cc2755p10.csv)) | LED pin timed through the debugger |
| LP-EM-CC35X1 | `cc35x1` | `print-a-message` | Works | 3 runs on hardware, 2026-10-25; route marked partial in data/routes.json, limits in docs/frl.md ([row](../data/results/cc35x1.csv)) | message read on the serial port |
| LAUNCHXL-F2800137 | `f2800137` | `blink` | Works | 3 runs on hardware, 2026-10-24 ([row](../data/results/f2800137.csv)) | LED pin timed through the debugger |
| LAUNCHXL-F2800137 | `f2800137` | `print-a-message` | Works | 3 runs on hardware, 2026-11-08 ([row](../data/results/f2800137.csv)) | message read on the serial port |
| LP-MSPM0C1104 | `mspm0c1104` | `blink` | Works | 3 runs on hardware, 2026-10-24 ([row](../data/results/mspm0c1104.csv)) | LED pin timed through the debugger |
| LP-MSPM0G3507 | `mspm0g3507` | `blink` | Works | 1 run on hardware, 2026-11-09 ([row](../data/results/mspm0g3507.csv)) | LED pin timed through the debugger |
| LP-MSPM0G3519 | `mspm0g3519` | `blink` | Works | 3 runs on hardware, 2026-10-24 ([row](../data/results/mspm0g3519.csv)) | LED pin timed through the debugger |
| LP-MSPM0G5187 | `mspm0g5187` | `blink` | Works | 3 runs on hardware, 2026-10-24 ([row](../data/results/mspm0g5187.csv)) | LED pin timed through the debugger |
| LP-MSPM0L1306 | `mspm0l1306` | `blink` | Works | 3 runs on hardware, 2026-10-24 ([row](../data/results/mspm0l1306.csv)) | LED pin timed through the debugger |
| LP-MSPM0L1306 | `mspm0l1306` | `print-a-message` | Works | 3 runs on hardware, 2026-11-02 ([row](../data/results/mspm0l1306.csv)) | message read on the serial port |
| LP-MSPM0L2228 | `mspm0l2228` | `blink` | Works | 1 run on hardware, 2026-11-09 ([row](../data/results/mspm0l2228.csv)) | LED pin timed through the debugger |
| LP-MSPM33C321A | `mspm33c321a` | `blink` | Works | 3 runs on hardware, 2026-10-24 ([row](../data/results/mspm33c321a.csv)) | LED pin timed through the debugger |
| LP-MSPM33C321A | `mspm33c321a` | `print-a-message` | Works | 3 runs on hardware, 2026-11-08 ([row](../data/results/mspm33c321a.csv)) | message read on the serial port |

## What each guided board needs

### LP-AM13E230 (`am13e230`)

- Probe: XDS110 on the board; transport `openocd`.
- `blink` tools, versions the route was validated with: sdk 26.01.00.03, sysconfig 1.28.0, compiler 5.1.1.LTS, openocd any.
- Before the first write: LED4 is wired to PA5 on the LaunchPad as shipped; the XDS110's debugger jumpers on.
- Backup: `tools/benchmark/backup.py` has a recipe; the route backs up before writing.
- Recovery: 3 method(s) with a run behind them; a restore is recorded (`am13e230/AGENTS.md`, When the board stops answering).
- First command: `./start.sh am13e230 blink --dry-run`, then without `--dry-run`.

### LP-CC1354P10-1 (`cc1354p10`)

- Probe: XDS110 on the board; transport `ccs13`.
- `blink` tools, versions the route was validated with: sdk 8.33.00.16, sysconfig 1.28.0, compiler 5.1.1.LTS, debug any.
- Before the first write: The board on its own USB cable, or on an LP-XDS110 with the target powered from the probe. This chip accepts a debugger only just after power-on: if a step says the connect was refused, unplug the USB cable for 6 s and run it again at once.
- Backup: `tools/benchmark/backup.py` has a recipe; the route backs up before writing.
- Recovery: 5 method(s) with a run behind them; a restore is recorded (`cc1354p10/AGENTS.md`, When the board stops answering).
- First command: `./start.sh cc1354p10 blink --dry-run`, then without `--dry-run`.

### LP-EM-CC2340R5 (`cc2340r5`)

- Probe: separate XDS110 probe; transport `openocd`.
- `blink` tools, versions the route was validated with: sdk 9.21.00.36, sysconfig 1.28.0, compiler 5.1.1.LTS, openocd any.
- `print-a-message` tools, versions the route was validated with: sdk 9.21.00.36, sysconfig 1.28.0, compiler 5.1.1.LTS, openocd any.
- Before the first write: The LP-EM board on an LP-XDS110ET with the TGT VDD jumper on XDS (as shipped), and the LED jumper on.
- Backup: `tools/benchmark/backup.py` has a recipe; the route backs up before writing.
- Recovery: 1 method(s) with a run behind them (`cc2340r5/AGENTS.md`, When the board stops answering).
- First command: `./start.sh cc2340r5 blink --dry-run`, then without `--dry-run`.

### LP-EM-CC2755P10 (`cc2755p10`)

- Probe: separate XDS110 probe; transport `openocd`.
- `blink` tools, versions the route was validated with: sdk 9.21.00.36, sysconfig 1.28.0, compiler 5.1.1.LTS, openocd any.
- Before the first write: The LP-EM board on an LP-XDS110ET with the TGT VDD jumper on XDS (as shipped), and the LED jumper on.
- Backup: `tools/benchmark/backup.py` has a recipe; the route backs up before writing.
- Recovery: 3 method(s) with a run behind them; a restore is recorded (`cc2755p10/AGENTS.md`, When the board stops answering).
- First command: `./start.sh cc2755p10 blink --dry-run`, then without `--dry-run`.

### LP-EM-CC35X1 (`cc35x1`)

- Probe: separate XDS110 probe; transport `toolbox`.
- `print-a-message` tools, versions the route was validated with: sdk 10.20.00.39, toolbox 4.3.23, sysconfig 1.28.0, compiler 5.1.1.LTS.
- Before the first write: The chip must already be activated (cc35x1/README.md step 3): frl never activates it. The board on an LP-XDS110ET or LP-XDS110 with the TGT VDD jumper on EXT, J4 and J6 on the lower two pins, and the board's own USB-C cable plugged in for power.
- Backup: no `backup.py` recipe; the route's own backup is described in the board's `AGENTS.md`.
- Recovery: 3 method(s) with a run behind them (`cc35x1/AGENTS.md`, When the board stops answering).
- First command: `./start.sh cc35x1 print-a-message --dry-run`, then without `--dry-run`.

### LAUNCHXL-F2800137 (`f2800137`)

- Probe: XDS110 on the board; transport `dss`.
- `blink` tools, versions the route was validated with: sdk 26.02.00.00, sysconfig 1.28.1, compiler 25.11.1.LTS, debug any.
- `print-a-message` tools, versions the route was validated with: sdk 26.02.00.00, sysconfig 1.28.1, compiler 25.11.1.LTS, debug any.
- Before the first write: S3 boot switches set for flash boot (GPIO24 and GPIO32 as shipped); LED4 on GPIO20.
- Backup: `tools/benchmark/backup.py` has a recipe; the route backs up before writing.
- Recovery: no recovery method with a run behind it yet (`c2000-launchpad/AGENTS.md`, When the board stops answering).
- First command: `./start.sh f2800137 blink --dry-run`, then without `--dry-run`.

### LP-MSPM0C1104 (`mspm0c1104`)

- Probe: XDS110 on the board; transport `openocd`.
- `blink` tools, versions the route was validated with: sdk 2.11.00.07, sysconfig 1.26.3, compiler 5.1.1.LTS, openocd 0.12.0+dev-02228.
- Before the first write: J7 must be on for the LED to light (SLAU908B Table 2-1), and J101 11:12 and 13:14 stay on (SWD to the XDS110, as shipped). Never mass erase this chip.
- Backup: `tools/benchmark/backup.py` has a recipe; the route backs up before writing.
- Recovery: 3 method(s) with a run behind them; a restore is recorded (`mspm0c1104/AGENTS.md`, When the board stops answering).
- First command: `./start.sh mspm0c1104 blink --dry-run`, then without `--dry-run`.

### LP-MSPM0G3507 (`mspm0g3507`)

- Probe: XDS110 on the board; transport `openocd`.
- `blink` tools, versions the route was validated with: sdk 2.11.00.07, sysconfig 1.28.0, compiler 5.1.1.LTS, openocd 0.12.0+dev-02228.
- Before the first write: LED2 needs its jumpers on (as shipped) and the XDS110's debugger jumpers on.
- Backup: `tools/benchmark/backup.py` has a recipe; the route backs up before writing.
- Recovery: 2 method(s) with a run behind them; a restore is recorded (`mspm0g3507/AGENTS.md`, When the board stops answering).
- First command: `./start.sh mspm0g3507 blink --dry-run`, then without `--dry-run`.

### LP-MSPM0G3519 (`mspm0g3519`)

- Probe: XDS110 on the board; transport `openocd`.
- `blink` tools, versions the route was validated with: sdk 2.11.00.07, sysconfig 1.28.0, compiler 5.1.1.LTS, openocd 0.12.0+dev-02228.
- Before the first write: LED2 needs its jumpers on (as shipped) and the XDS110's debugger jumpers on.
- Backup: `tools/benchmark/backup.py` has a recipe; the route backs up before writing.
- Recovery: 1 method(s) with a run behind them; a restore is recorded (`mspm0g3519/AGENTS.md`, When the board stops answering).
- First command: `./start.sh mspm0g3519 blink --dry-run`, then without `--dry-run`.

### LP-MSPM0G5187 (`mspm0g5187`)

- Probe: XDS110 on the board; transport `openocd`.
- `blink` tools, versions the route was validated with: sdk 2.11.00.07, sysconfig 1.28.0, compiler 5.1.1.LTS, openocd 0.12.0+dev-02228.
- Before the first write: J12 [2:1] on for LED2 red (TI's example README), and the XDS110's debugger jumpers on (as shipped).
- Backup: `tools/benchmark/backup.py` has a recipe; the route backs up before writing.
- Recovery: 1 method(s) with a run behind them; a restore is recorded (`mspm0g5187/AGENTS.md`, When the board stops answering).
- First command: `./start.sh mspm0g5187 blink --dry-run`, then without `--dry-run`.

### LP-MSPM0L1306 (`mspm0l1306`)

- Probe: XDS110 on the board; transport `openocd`.
- `blink` tools, versions the route was validated with: sdk 2.11.00.07, sysconfig 1.28.0, compiler 5.1.1.LTS, openocd 0.12.0+dev-02228.
- `print-a-message` tools, versions the route was validated with: sdk 2.11.00.07, sysconfig 1.28.0, compiler 5.1.1.LTS, openocd 0.12.0+dev-02228.
- Before the first write: LED2 needs its jumpers J2, J3, J12 and J13 on (as shipped), and J101's nine debugger jumpers on.
- Backup: `tools/benchmark/backup.py` has a recipe; the route backs up before writing.
- Recovery: 4 method(s) with a run behind them (`mspm0l1306/AGENTS.md`, When the board stops answering).
- First command: `./start.sh mspm0l1306 blink --dry-run`, then without `--dry-run`.

### LP-MSPM0L2228 (`mspm0l2228`)

- Probe: XDS110 on the board; transport `openocd`.
- `blink` tools, versions the route was validated with: sdk 2.11.00.07, sysconfig 1.28.0, compiler 5.1.1.LTS, openocd 0.12.0+dev-02228.
- Before the first write: The red LED needs its jumpers on (as shipped) and the XDS110's debugger jumpers on.
- Backup: `tools/benchmark/backup.py` has a recipe; the route backs up before writing.
- Recovery: no recovery method with a run behind it yet; a restore is recorded (`mspm0l2228/AGENTS.md`, When the board stops answering).
- First command: `./start.sh mspm0l2228 blink --dry-run`, then without `--dry-run`.

### LP-MSPM33C321A (`mspm33c321a`)

- Probe: XDS110 on the board; transport `openocd`.
- `blink` tools, versions the route was validated with: sdk 1.04.00.00, sysconfig 1.28.0, compiler 5.1.1.LTS, openocd any.
- `print-a-message` tools, versions the route was validated with: sdk 1.04.00.00, sysconfig 1.28.0, compiler 5.1.1.LTS, openocd any.
- Before the first write: LED2 jumpers on as shipped, and the XDS110's debugger jumpers on.
- Backup: `tools/benchmark/backup.py` has a recipe; the route backs up before writing.
- Recovery: 2 method(s) with a run behind them; a restore is recorded (`mspm33c321a/AGENTS.md`, When the board stops answering).
- First command: `./start.sh mspm33c321a blink --dry-run`, then without `--dry-run`.

## Computers the guided route has run on

Only these cells have a run record (`hosts` in `data/routes.json`); every other combination is untested.

| Computer | Result | Date |
|---|---|---|
| Intel Mac, macOS 15.7.9 (Darwin 24.6.0), x86_64, bash, Python 3.9.6 | tested: blink-pattern backed up, built, flashed, verified and restored on hardware, no assistant, Claude Code 2.1.299 and Codex CLI 0.160.0 (ChatGPT app 26.930.31730) | 2026-10-14 |
| Apple Silicon Mac mini, macOS 26.3, arm64, Python 3.9.6 | built: image built there passed frl verify on the board (flashed from the Intel Mac); doctor ready with SysConfig unvalidated. Flashing from this host untested (no board on it) | 2026-10-14 |
| windows-native | untested |  |
| windows-wsl | untested |  |
| Ubuntu 24.04 x86_64 container (linux/amd64 under OrbStack), Python 3.12.3, GNU Make 4.3 | built: image built there passed frl verify on the board (flashed from the Intel Mac). USB flashing from Linux untested | 2026-10-14 |

## Boards set up by hand

No guided route. Start with `bash mac-setup/bootstrap.sh --check` for the tools, then the board's own README.

| Board | Id | Start here | Backup | Recovery methods with a run |
|---|---|---|---|---|
| LP-AM243 | `am243x` | [`am243x/README.md`](../am243x/README.md) | none | none |
| LP-AM263P | `am263p` | [`am263p/README.md`](../am263p/README.md) | none | 1 |
| TMDS62LEVM (AM62L EVM) | `am62l-evm` | [`am62l-evm/README.md`](../am62l-evm/README.md) | none | 4 |
| BeagleY-AI | `beagley-ai` | | none | 2 |
| CC3200-LAUNCHXL | `cc3200` | [`cc3200-launchxl/README.md`](../cc3200-launchxl/README.md) | none | none |
| ESP32-C6-DevKitC-1-N8 | `esp32c6` | [`esp32c6/README.md`](../esp32c6/README.md) | none | 2 |
| LAUNCHXL-F28P65X | `f28p65x` | [`c2000-launchpad/README.md`](../c2000-launchpad/README.md) | none | none |
| FRDM-MCXA153 | `frdm-mcxa153` | [`frdm-mcxa153/README.md`](../frdm-mcxa153/README.md) | none | 1 |
| IWRL6432BOOST | `iwrl6432boost` | [`iwrl6432boost/README.md`](../iwrl6432boost/README.md) | none | 1 |
| NUCLEO-G071RB | `nucleo-g071rb` | | none | none |
| NUCLEO-U083RC | `nucleo-u083rc` | [`nucleo-u083rc/README.md`](../nucleo-u083rc/README.md) | none | 1 |
| Raspberry Pi Pico 2 H | `pico2` | [`pico2/README.md`](../pico2/README.md) | none | 1 |
| SK-TDA4VM | `tda4` | [`tda4/README.md`](../tda4/README.md) | none | 1 |
