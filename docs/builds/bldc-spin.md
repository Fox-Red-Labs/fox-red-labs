# Spin a brushless motor: six-step, FOC and sensorless FOC

An LP-AM13E230 and a plug-on driver board turn a Hall-sensor motor; drive it from the serial port, the browser console or a phone.

The page: [site/project-bldc-spin.html](../../site/project-bldc-spin.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Six-step on the Hall sensors

The simplest drive: the Halls pick which two phases are on. You type at a serial terminal.

Status: Works. Boards: am13e230.

Folders this version uses (in a sparse clone, fetch them first, then read `am13e230/AGENTS.md`):

```bash
git sparse-checkout add am13e230
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-AM13E230 LaunchPad development kit | 1 | the board |
| DRV8376EVM three-phase motor driver evaluation module | 1 | the motor driver |
| Anaheim Automation BLY171S-24V-4000 brushless motor (NEMA 17, Hall sensors) | 1 | the motor |
| 12 V DC power supply, 0.5 A or more (any wall adapter; model not recorded) | 1 | motor power, to the VBAT and PGND screw terminals |
| Small screwdriver, wire strippers and hookup wire | 1 | for the screw terminals: 3 motor phase wires, 5 Hall wires and the 2 supply wires |

### Before you start

- The code is in this repository. Clone it first: `git clone --depth 1 --filter=blob:none https://github.com/Fox-Red-Labs/fox-red-labs.git` (about 300 MB on disk; the sparse clone on the start page is smaller). This page calls the folder you clone it into `<repository folder>`. Every command below runs from inside it.
- Four free TI tools, each under /Applications/ti on a Mac or named by an environment variable the build script reads: the AM13E230x SDK 26.01.00.03 (https://www.ti.com/tool/LP-AM13E230, myTI login, about 0.4 GB; `FRL_AM13_SDK`), SysConfig 1.28.0 (https://www.ti.com/tool/SYSCONFIG, about 0.15 GB; `FRL_SYSCONFIG`), TI Arm Clang 5.1.1.LTS (https://www.ti.com/tool/ARM-CGT, about 0.3 GB, or the copy inside CCStudio; `FRL_TICLANG`) and, for the flash step, DSLite from UniFlash (https://www.ti.com/tool/UNIFLASH, about 0.5 GB, or the copy inside CCStudio; `FRL_DSLITE`). `python3 tools/frl/frl.py doctor --board am13e230` lists what is missing. The build script stops with the tool's name and download page when one is absent. Checked on one Intel Mac only; nothing is installed by the script.

### Wiring

The 40-pin edge is four 10-pin headers. Hold the board with the USB connector at the top: J1 (pins 1 to 10) and J3 (21 to 30) are the two left rows, J4 (31 to 40) and J2 (11 to 20) the two right rows. Pin 1 of J1 and pin 21 of J3 are the top pins of the left rows, nearest the USB connector; J4 and J2 count the other way, so pin 40 and pin 20 are at the top. Nothing needs a jumper wire between the boards: the EVM plugs onto J1 to J4 with its screw terminals pointing away from the USB-C connector. The loose wires are the motor's: phases yellow, red, black to OUT A, B, C (any order), and the Hall leads red, black, blue, green, white to HALL POWER, HALL GROUND, HALL A, B, C. Supply 4.5 to 65 V to VBAT and PGND; 12 V is plenty. The pin table is the one in am13e230/bldc-drv8376/README.md.

| Module pin | Board pin | What it is | Needed |
|---|---|---|---|
| EVM INHA, INLA | J4 pins 40, 39 (PB3, PA7) | gate drive, phase A: MCPWM4 1A, 1B | required |
| EVM INHB, INLB | J4 pins 38, 37 (PB1, PA2) | gate drive, phase B: MCPWM4 2A, 2B | required |
| EVM INHC, INLC | J4 pins 36, 35 (PB2, PA25) | gate drive, phase C: MCPWM4 3A, 3B | required |
| EVM HALL A, B, C | J4 pins 34, 33, 32 (PB7, PB6, PA12) | Hall inputs; the EVM has 10 k pull-ups to 3.3 V | required |
| EVM nSLEEP | J1 pin 9 (PA23) | high wakes the driver | required |
| EVM nFAULT | J1 pin 8 (PB4) | low means a driver fault | required |
| EVM DRVOFF | J2 pin 13 (PB18) | driver output enable | required |
| EVM VSENVM | J3 pin 23 (PA16) | bus voltage through the EVM's divider | required |
| EVM VSENA | J3 pin 26 (PA17) | phase A voltage | optional |
| LaunchPad S6 and S4 | both to BP | send PB6, PB7, PA11 and PA12 to the headers, not the CAN chip | required |
| EVM J13 | GVDD position (5 V) | powers the Hall sensors; the motor's Halls want 4.5 V or more | required |

### Steps

1. **Bench.** With USB unplugged: set EVM jumper J13 to GVDD, wire the motor and Hall leads as in the table, set LaunchPad switches S6 and S4 to BP, and press the EVM onto J1 to J4 with its screw terminals pointing away from the USB-C connector.

   You should see: The EVM sits flat on all four headers. Nothing is lit.

   If every Hall line later reads state 7, the J13 jumper is missing or the Hall ground lead is open.

2. **Terminal.** Build and flash the six-step program. With one LaunchPad on USB the script finds its probe and writes the connection file it needs; with more than one it lists their serials and asks for `./build.sh --flash <serial>`.

   ```text
   cd am13e230/bldc-drv8376 && ./build.sh --flash
   ```

   You should see: About 3 s to build and 5 s to flash over the LaunchPad kit's own probe; 32 kB of flash.

   If the build cannot find its tools, install the AM13E230x SDK 26.01.00.03, SysConfig 1.28.0 and TI Arm Clang 5.1.1.LTS where the script expects them (the paths are at the top of build.sh); `python3 tools/frl/frl.py doctor --board am13e230` lists what is missing with download links.

3. **Bench.** Plug in USB-C, then switch the motor supply on. Open the LaunchPad kit's serial port at 115200 baud and type `v`.

   You should see: The EVM's VM, AVDD and GVDD lights are on and NFAULT is off. `v` shows bus volts near your supply and a Hall state from 1 to 6. The current image boots with the driver off; Check the supply voltage and the driver wiring before the first start.

   If it says `no bus voltage on VM`, the supply is off or on the wrong terminals.

4. **Terminal.** Learn the Hall table once per power-up: type `l`. It takes about 25 s with no hands.

   You should see: The shaft clicks round six steps and a table prints with six different Hall states.

   If it says `learn failed`, a Hall wire is loose (state 0 or 7), or the rotor did not move: raise the supply voltage.

5. **Terminal.** Type `2` then `g`.

   ```text
   2g
   ```

   You should see: The motor turns. A line a second shows duty, direction, Hall state, rpm, bus volts and fault. At 20 percent duty it settles near 920 rpm.

   If it says `DRIVER FAULT`, lower the duty and check the supply can give the current, then type `c` to clear.

6. **Terminal.** Type `s`, then `r`, then `g` for the other direction. `b` brakes. `0` then `s` ends.

   You should see: Reverse runs 4 to 10 percent slower than forward at the same duty.

   If it runs rough or stalls at once, type `a` to trim the commutation advance, then `g` again.

### Expected result

At 10 to 60 percent duty the motor runs from about 400 to about 3,630 rpm forward and 380 to 3,440 rpm in reverse, unloaded, on 12.4 V.

### What this shows

- A plug-on driver board and about 350 lines of C turn a Hall-sensor motor, with the Hall table learned by the program in about 25 s.
- Unloaded, the motor started from standstill on every try at 10 and at 20 percent duty.
- Median speed at 20, 40 and 60 percent duty, forward: 924, 2,312 and 3,632 rpm; reverse: 891, 2,091 and 3,442.

### Limits

- Torque or efficiency: nothing was loaded and phase current was not measured in this version.
- That the speeds are exact: they come from Hall edge timing, and a tachometer would settle whether they read high.
- That the speed sweep repeats: it was run once. The same motor later turned under the FOC image.
- Open loop in duty: speed follows the duty you type, with no speed loop in the base program.
- One motor, one driver board, 12.4 V from a small adapter, 60 percent duty cap, unloaded.
- The Hall sensors sit about 7 electrical degrees off their ideal spacing; the program learns the table rather than assuming it.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Speed sweep | 10 to 60 percent duty, 3 runs each, 20 kHz PWM, 6x PWM mode, Hall-timed rpm | `am13e230/bldc-drv8376/README.md` |
| Motor | Anaheim BLY171S-24V-4000: 24 V, 4,000 rpm, 1.8 A, 4 pole pairs, 2.45 V per 1,000 rpm (label) | `am13e230/bldc-drv8376/README.md` |

## FOC on the Hall sensors

TI's three-shunt Hall FOC solution, moved onto this driver board, with a firmware guard and a live link.

Status: Works. Boards: am13e230.

Folders this version uses (in a sparse clone, fetch them first, then read `am13e230/AGENTS.md`):

```bash
git sparse-checkout add am13e230 tools/motorlink tools/frl
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-AM13E230 LaunchPad development kit | 1 | the board |
| DRV8376EVM three-phase motor driver evaluation module | 1 | the motor driver |
| Anaheim Automation BLY171S-24V-4000 brushless motor (NEMA 17, Hall sensors) | 1 | the motor |
| 12 V DC power supply, 0.5 A or more (any wall adapter; model not recorded) | 1 | motor power, to the VBAT and PGND screw terminals |
| Small screwdriver, wire strippers and hookup wire | 1 | for the screw terminals: 3 motor phase wires, 5 Hall wires and the 2 supply wires |

### Before you start

- The code is in this repository. Clone it first: `git clone --depth 1 --filter=blob:none https://github.com/Fox-Red-Labs/fox-red-labs.git` (about 300 MB on disk; the sparse clone on the start page is smaller). This page calls the folder you clone it into `<repository folder>`. Every command below runs from inside it.
- Four free TI tools, each under /Applications/ti on a Mac or named by an environment variable the build script reads: the AM13E230x SDK 26.01.00.03 (https://www.ti.com/tool/LP-AM13E230, myTI login, about 0.4 GB; `FRL_AM13_SDK`), SysConfig 1.28.0 (https://www.ti.com/tool/SYSCONFIG, about 0.15 GB; `FRL_SYSCONFIG`), TI Arm Clang 5.1.1.LTS (https://www.ti.com/tool/ARM-CGT, about 0.3 GB, or the copy inside CCStudio; `FRL_TICLANG`) and, for the flash step, DSLite from CCStudio (https://www.ti.com/tool/CCSTUDIO, 1.9 GB; `am13e230/tools/run_image.py` uses it). `python3 tools/frl/frl.py doctor --board am13e230` lists what is missing. The build script stops with the tool's name and download page when one is absent. Checked on one Intel Mac only; nothing is installed by the script.
- Chrome or Edge on a computer, if you use the motor console (it reads the board over Web Serial).

### Wiring

The 40-pin edge is four 10-pin headers. Hold the board with the USB connector at the top: J1 (pins 1 to 10) and J3 (21 to 30) are the two left rows, J4 (31 to 40) and J2 (11 to 20) the two right rows. Pin 1 of J1 and pin 21 of J3 are the top pins of the left rows, nearest the USB connector; J4 and J2 count the other way, so pin 40 and pin 20 are at the top. Same plug-on and motor wiring as the six-step version. The FOC build also reads the three phase currents (ISENA, ISENB, ISENC on PA27, PA30, PA29) and phase voltages B and C (PA28, PA18); those pin placements are listed in am13e230/foc-drv8376/README.md, and two of them have no recorded EVM header pin, so they are not repeated here. The current-sense reference jumper J2 must be fitted so the sense outputs rest near half scale.

| Module pin | Board pin | What it is | Needed |
|---|---|---|---|
| EVM INHA, INLA | J4 pins 40, 39 (PB3, PA7) | gate drive, phase A: MCPWM4 1A, 1B | required |
| EVM INHB, INLB | J4 pins 38, 37 (PB1, PA2) | gate drive, phase B: MCPWM4 2A, 2B | required |
| EVM INHC, INLC | J4 pins 36, 35 (PB2, PA25) | gate drive, phase C: MCPWM4 3A, 3B | required |
| EVM HALL A, B, C | J4 pins 34, 33, 32 (PB7, PB6, PA12) | Hall inputs; the EVM has 10 k pull-ups to 3.3 V | required |
| EVM nSLEEP | J1 pin 9 (PA23) | high wakes the driver | required |
| EVM nFAULT | J1 pin 8 (PB4) | low means a driver fault | required |
| EVM DRVOFF | J2 pin 13 (PB18) | driver output enable | required |
| EVM VSENVM | J3 pin 23 (PA16) | bus voltage through the EVM's divider | required |
| EVM VSENA | J3 pin 26 (PA17) | phase A voltage | optional |
| LaunchPad S6 and S4 | both to BP | send PB6, PB7, PA11 and PA12 to the headers, not the CAN chip | required |
| EVM J13 | GVDD position (5 V) | powers the Hall sensors; the motor's Halls want 4.5 V or more | required |

### Steps

1. **Bench.** Wire it as in the six-step version, with the 12 V supply, and make sure J2 (the current-sense reference) is fitted.

   You should see: Same layout. With power on and the motor stopped, the phase-current sense outputs rest near half scale.

   If the sense outputs read a few millivolts, J2 is missing.

2. **Terminal.** Build the normal image, the one with the bench guard: `cd am13e230/foc-drv8376 && OUT=$HOME/frl-build FRL_COMMISSIONING=0 ./build.sh`

   You should see: foc.out and foc.map in $HOME/frl-build/foc-drv8376/ (OUT sets the parent folder). The script builds and never flashes.

   Without `FRL_COMMISSIONING=0` the script builds the commissioning console instead, which does not run TI's state machine.

3. **Terminal.** Flash it over the LaunchPad kit's probe: `python3 am13e230/tools/run_image.py $HOME/frl-build/foc-drv8376/foc.out 3`

   You should see: The flash verifies and the program starts. The script flashes the bench's own probe by default; for yours pass `--serial <probe serial>` and add a matching AM13E23019_<probe serial>.ccxml beside the existing one in am13e230/.

   If the flash does not return in 90 s the script stops it and tries once more; a probe held by CCStudio or another program is reported as busy.

4. **Terminal.** Read the board: `python3 tools/motorlink/link.py poll`

   You should see: State reads stop idle, bus volts near your supply, phase currents under 3 mA.

   If the fault word reads 8192 (a gate driver fault), the board is not running the guarded image; flash it again.

5. **Terminal.** Learn the Hall angle once, under the probe lock: `python3 tools/frl/lock.py exec --serial <probe serial> --op motor-campaign -- python3 tools/motorlink/campaign.py cal` To find yours, run `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e` if you have CCStudio (it prints `Serial Num:`), else `system_profiler SPUSBDataType | grep -A3 XDS110`.

   You should see: State reports calibration, then idle, with the Hall table marked valid. It took 171 s on the bench; the guard allows up to 700 s.

   If calibration fails, TI's estimator fault is raised: check the five Hall wires and that the shaft can turn freely.

6. **Terminal.** Set the guard's bus window for the supply, then run at 10 Hz electrical: `campaign.py supply adapter12`, then `campaign.py fwd 10 OUTDIR TAG`, each under the same `lock.py exec` line as step 5 (supply takes `adapter12` or `pack20`).

   You should see: The motor aligns, opens loop, reaches closed loop at about 9 s and stops at the 12 s run cap. Speed in rpm is Hz x 15 for four pole pairs. Raw streams land in OUTDIR.

   If the start is refused with `bus out of range`, the supply is outside the window you set.

7. **Browser.** Or drive it by hand: Connect to the board and watch the motor live in Chrome or Edge, click Connect, set a speed under 40 Hz and click Run. STOP is always on screen.

   You should see: The state lights step Calibrate, Align, Open loop, Closed loop; speed, id, iq, peak current and bus volts update live.

   If Connect finds no board, it needs the normal image from step 3, not the commissioning build.

8. **Terminal.** Try the designed speed gains, under the lock: `python3 tools/motorlink/campaign.py gstep 0.0131 0.0157 200 OUTDIR TAG`, then `python3 tools/motorlink/speedloop_analyse.py` on the folder.

   You should see: Settle at 10 Hz, step to 15 Hz at 14 s. Median rise about 50 ms against 91 ms on TI's defaults.

   If a run ends before the step, the guard's 20 s cap or a fault stopped it: read frlGuard.reason from `link.py poll`.

9. **Phone, on the same private network.** Drive it from a phone. Phones have no Web Serial, so the computer that holds the board's USB port serves the same page with a stand-in serial port. Run the bridge under the probe lock with your own serial port and a private address of the computer; it prints one link with a random token. This is the home network, not Bluetooth. The remote's controls were checked against a simulated board; a run of the phone page on the real motor is not recorded.

   ```text
   python3 tools/frl/lock.py exec --serial <probe serial> --op phone-console -- node tools/motorlink/phone_console.mjs --port /dev/cu.usbmodem<probe serial>1 --host <private address of this computer>
   ```

   You should see: The page prints a link; opening it on the phone shows the Motor remote with Connect, Forward, Reverse, a speed slider, Start, Slow and stop, and a red STOP button. The board's guard stops the motor within 1 s if the phone goes quiet.

   If the phone cannot reach the link, check that it is on the same private network as the computer. Details: `tools/motorlink/BRIDGE.md`.

10. **Browser.** Optional: TI's own tool. TI publishes the MCU Motor Control GUI, a graphical tool for configuring and tuning motors, on [its public tool page](https://www.ti.com/tool/MCU-MOTOR-CONTROL-GUI) (download `MCU_Motor_Control_GUI.zip`, release notes on the same page). That page lists C2000 boards and reference designs as supported and does not list the LP-AM13E230. The AM13E2x SDK's Hall FOC example sends its variables over a UART at 9.2 Mbaud to MotorVision, the protocol this repository's console speaks. No recorded run connects TI's GUI to this board.

   You should see: Nothing to check here: this step only points to TI's tool.

   If you try it, note what the GUI says about the board; the console in step 7 is the tested way to see the same variables.

### Expected result

The motor holds a set electrical speed in closed loop on TI's FOC, stops on its own at the cap, and a +5 Hz step settles in about 80 ms with the designed gains.

### What this shows

- TI's Hall FOC solution runs on a driver board it was not written for: about 650 lines of change, no fault, peak current 0.57 A in a 12 s run at 10 Hz.
- The firmware guard stops every run on its own: a time cap, 1 s after the host goes quiet, a phase-current trip, a speed clamp and a bus voltage window. Host cut-off was tested: the motor stopped 1.0 s after the last packet.
- Speed gains designed for the 0.5 ms speed tick halve the step's rise: 50 ms median (41 to 129) against 91 ms (79 to 182) with TI's defaults, five runs against three.

### Limits

- Overshoot: one Hall-timed speed sample is only resolved to about 0.7 Hz, 14 percent of the 5 Hz step, so none is reported.
- That the designed gains are better everywhere: they hold 10 Hz tighter but hunt more at 15 Hz (13.84 to 15.92 Hz p5 to p95 against 14.47 to 15.92).
- That TI's default gains work for reversals: with them, 7 of 7 reversals faulted and 3 of 14 runs at 20 Hz reached a 20 s cap.
- The `campaign.py` script these steps call is not in this copy, so the Hall calibration, speed-loop and observer steps cannot be repeated from it; the results are the recorded runs.
- Unloaded, one motor, 12.4 V, 10 to 20 Hz electrical; a +5 Hz step only; the 30 Hz design row was not run.
- Current polarity and gain rest on the EVM's documented 1 V/A; the gain resistors were not read off the board.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Step response | rise 10 to 90 percent 50 ms median with raised gains against 91 ms with TI's defaults; settle to a 1.25 Hz band 82 ms against 151 ms |  |
| Live link | TI's MotorVision protocol at 9.2 Mbaud over the LaunchPad kit's USB serial; guard version 6 | `tools/motorlink/README.md` |
| Port | TI's 3sh_hall_sen_foc_drv8323rh solution, 20 kHz PWM, 2.0 Âµs minimum compare, 0.4 A software torque ceiling | `am13e230/foc-drv8376/README.md` |

## FOC with no sensor in the loop

TI's FAST observer, which TI's Hall solution builds but never runs, drives the motor after an align and open-loop start.

Status: Works. Boards: am13e230.

Folders this version uses (in a sparse clone, fetch them first, then read `am13e230/AGENTS.md`):

```bash
git sparse-checkout add am13e230 tools/frl
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-AM13E230 LaunchPad development kit | 1 | the board |
| DRV8376EVM three-phase motor driver evaluation module | 1 | the motor driver |
| Anaheim Automation BLY171S-24V-4000 brushless motor (NEMA 17, Hall sensors) | 1 | the motor |
| 12 V DC power supply, 0.5 A or more (any wall adapter; model not recorded) | 1 | motor power, to the VBAT and PGND screw terminals |
| Small screwdriver, wire strippers and hookup wire | 1 | for the screw terminals: 3 motor phase wires, 5 Hall wires and the 2 supply wires |

### Before you start

- The code is in this repository. Clone it first: `git clone --depth 1 --filter=blob:none https://github.com/Fox-Red-Labs/fox-red-labs.git` (about 300 MB on disk; the sparse clone on the start page is smaller). This page calls the folder you clone it into `<repository folder>`. Every command below runs from inside it.
- Four free TI tools, each under /Applications/ti on a Mac or named by an environment variable the build script reads: the AM13E230x SDK 26.01.00.03 (https://www.ti.com/tool/LP-AM13E230, myTI login, about 0.4 GB; `FRL_AM13_SDK`), SysConfig 1.28.0 (https://www.ti.com/tool/SYSCONFIG, about 0.15 GB; `FRL_SYSCONFIG`), TI Arm Clang 5.1.1.LTS (https://www.ti.com/tool/ARM-CGT, about 0.3 GB, or the copy inside CCStudio; `FRL_TICLANG`) and, for the flash step, DSLite from CCStudio (https://www.ti.com/tool/CCSTUDIO, 1.9 GB; `am13e230/tools/run_image.py` uses it). `python3 tools/frl/frl.py doctor --board am13e230` lists what is missing. The build script stops with the tool's name and download page when one is absent. Checked on one Intel Mac only; nothing is installed by the script.
- The FOC firmware built, flashed and Hall-calibrated as in the previous version (its steps 1 to 5). The Halls stay wired as a referee.

### Wiring

The 40-pin edge is four 10-pin headers. Hold the board with the USB connector at the top: J1 (pins 1 to 10) and J3 (21 to 30) are the two left rows, J4 (31 to 40) and J2 (11 to 20) the two right rows. Pin 1 of J1 and pin 21 of J3 are the top pins of the left rows, nearest the USB connector; J4 and J2 count the other way, so pin 40 and pin 20 are at the top. Same plug-on and motor wiring as the six-step version. The FOC build also reads the three phase currents (ISENA, ISENB, ISENC on PA27, PA30, PA29) and phase voltages B and C (PA28, PA18); those pin placements are listed in am13e230/foc-drv8376/README.md, and two of them have no recorded EVM header pin, so they are not repeated here. The current-sense reference jumper J2 must be fitted so the sense outputs rest near half scale.

| Module pin | Board pin | What it is | Needed |
|---|---|---|---|
| EVM INHA, INLA | J4 pins 40, 39 (PB3, PA7) | gate drive, phase A: MCPWM4 1A, 1B | required |
| EVM INHB, INLB | J4 pins 38, 37 (PB1, PA2) | gate drive, phase B: MCPWM4 2A, 2B | required |
| EVM INHC, INLC | J4 pins 36, 35 (PB2, PA25) | gate drive, phase C: MCPWM4 3A, 3B | required |
| EVM HALL A, B, C | J4 pins 34, 33, 32 (PB7, PB6, PA12) | Hall inputs; the EVM has 10 k pull-ups to 3.3 V | required |
| EVM nSLEEP | J1 pin 9 (PA23) | high wakes the driver | required |
| EVM nFAULT | J1 pin 8 (PB4) | low means a driver fault | required |
| EVM DRVOFF | J2 pin 13 (PB18) | driver output enable | required |
| EVM VSENVM | J3 pin 23 (PA16) | bus voltage through the EVM's divider | required |
| EVM VSENA | J3 pin 26 (PA17) | phase A voltage | optional |
| LaunchPad S6 and S4 | both to BP | send PB6, PB7, PA11 and PA12 to the headers, not the CAN chip | required |
| EVM J13 | GVDD position (5 V) | powers the Hall sensors; the motor's Halls want 4.5 V or more | required |

### Steps

1. **Terminal.** Have the guarded FOC image flashed and the Hall table calibrated: steps 2, 3 and 5 of the FOC version. The observer commands need guard version 6; `campaign.py` refuses to run on another version.

   You should see: `link.py poll` reads stop idle, with the Hall table valid.

   If the campaign stops with `board is not on guard version 6`, rebuild and flash the current am13e230/foc-drv8376.

2. **Terminal.** Find where the rotor really is: `campaign.py sweep 2 40 OUTDIR TAG`, under the same `tools/frl/lock.py exec` line as the FOC version's step 5. It steps the FOC angle by -45 to +45 degrees at 40 Hz with the Halls driving.

   You should see: Phase current falls from 0.129 A at 0 degrees to 0.089 A near +49 degrees, and a -45 degree step stalls the motor. The current minimum is the true rotor angle.

   A stall on the negative steps is expected and the guard stops it; clear faults with the console's Clear faults button before the next run.

3. **Terminal.** Shadow run: the Halls drive, FAST runs beside them: `campaign.py obs 2 0 40 40 15 OUTDIR TAG` (observer 2 = FAST, hand-over off, speed ceiling 40 Hz, reference 40 Hz, 15 s cap).

   You should see: FAST reads about 50 degrees from the Hall angle and its speed is within 0.4 Hz of the Halls'.

   If FAST's speed is far from the Halls' below about 20 Hz, run faster: both observers lock above about 20 Hz electrical here.

4. **Terminal.** Run with no Halls in the loop: `campaign.py hallfree 2 0 40 20 OUTDIR TAG` (observer FAST, no trim, 40 Hz reference, 20 s cap).

   You should see: TI's align (5 s at 0.36 A) and open loop to 30 Hz, then FAST takes over at about 8 s. The speed holds at 40.0 Hz and phase current reads about 0.063 A. The Halls are only logged against it.

   If it faults in start-up, check the Hall-free flag was applied: the campaign prints the config it read back before it starts.

### Expected result

After an align and open-loop start the motor runs on FAST alone at 40.0 Hz electrical, drawing about half the current the Hall version needs.

### What this shows

- TI's FAST observer identifies its own model and read the rotor angle to 2 or 3 degrees with no help from the Halls; the ESMO observer needed a 31 degree trim from the sweep because its gains came from another motor.
- TI's automatic Hall calibration left the Hall angle 49 degrees off the rotor, which is why the Hall version draws about 2 times the current.
- Hall-free runs held 40.0 Hz at 0.063 A (two FAST runs, one ESMO run), against 0.13 A on the Halls at the same speed.

### Limits

- That the Halls are not needed: they stay wired as a referee, and the firmware only logs them in this mode.
- Loaded starts, speeds below about 20 Hz electrical, reversals or speed steps on the observer: none were run.
- That the 49 degree answer is exact: it rests on a 1/cos current fit over six points, with a 45 to 52.5 degree band.
- The `campaign.py` script these steps call is not in this copy, so the Hall calibration, speed-loop and observer steps cannot be repeated from it; the results are the recorded runs.
- Unloaded, one motor, 12.4 V, one steady speed (40 Hz electrical, 600 rpm).
- ESMO's 31 degree trim came from this motor's sweep and would need its own sweep on another motor.
- The start is TI's align and open loop; a rotor that cannot align (a loaded shaft) is outside the tested cases.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Angle sweep | current = 0.087 A / cos(shift - 49), 4.4 mA rms residual, two runs each side |  |
| Hall-free runs | 3 runs of 20 s: FAST 52.1 degrees from the Hall table (referee), ESMO 50.9; no fault |  |
