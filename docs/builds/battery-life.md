# Measure a board's running and sleep current

The LAUNCHXL-F2800137 has no current meter, so two wires from an LP-MSPM0L1306's EnergyTrace probe power the chip and read it: 35.4 mA running CoreMark at 120 MHz, 2.6 mA in STANDBY, above a 6.10 mA HALT reading. Coin-cell life is not measured.

The page: [site/project-battery-life.html](../../site/project-battery-life.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Current of a LAUNCHXL-F2800137 read through an LP-MSPM0L1306 probe

Two wires from the LP-MSPM0L1306's EnergyTrace probe to the C2000 board's JP2 jumper; one flash image runs every phase in a row.

Status: Works. Boards: f2800137.

Folders this version uses (in a sparse clone, fetch them first, then read `c2000-launchpad/AGENTS.md`):

```bash
git sparse-checkout add c2000-launchpad tools
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LAUNCHXL-F2800137 LaunchPad development kit | 1 | the board being measured |
| LP-MSPM0L1306 LaunchPad development kit | 1 | the meter: its EnergyTrace probe powers and reads the other board |
| ELEGOO jumper wires (120-piece pack) | 2 | 3V3 and GND between the two boards |

### Before you start

- A Mac with CCStudio 21.0.1 (its scripting runner and the TI C2000 compiler 25.11.1.LTS), C2000Ware 26.02.00.00 and Python 3. The capture tool calls CCStudio's XDS110 driver, so CCStudio must be installed; no board is touched until step 3.
- The code is in this repository: `c2000-launchpad/` and `tools/energytrace/`.
- The TI EnergyTrace probe feeds the board at up to about 75 mA (TI staff, E2E 2023), so measure the bare board with nothing plugged on it.

### Wiring

J101 is the fence between the MSPM0 board's probe and its chip; with 3V3 out, everything the probe measures comes down the wire. JP2 is the C2000 board's own fence: one pin is 3.3 V from its regulator, the other feeds the chip and LED1. Which JP2 pin is the chip side is not confirmed (the drawing assumes the lower one): with the wire off and only the C2000's USB in, LED1 should be dark; touch the wire to the pin and LED1 should light.

| Module pin | Board pin | What it is | Needed |
|---|---|---|---|
| LP-MSPM0L1306 J101, top-row 3V3 pin (probe side) | LAUNCHXL-F2800137 JP2 +3V3, chip-side pin | 3.3 V from the meter to the chip | required |
| LP-MSPM0L1306 J2, any GND pin | LAUNCHXL-F2800137 JP5, any GND pin | common ground | required |
| LP-MSPM0L1306 J101 3V3 jumper | (pull it out) | stops the probe powering its own chip | required |
| LAUNCHXL-F2800137 JP2 +3V3 shunt | (pull it out; leave the +5V0 shunt in) | stops the board powering its own chip | required |

### Steps

1. **Terminal (Mac).** Profile 2 runs every phase in one image: a 2 s busy spin at 120 MHz, CoreMark (2k iterations from flash), 12 watchdog wakes in IDLE, a 3 s spin at 60 MHz, 12 watchdog wakes in STANDBY, then HALT with the PLL and oscillators off. Build it from flash with the same flags as the recorded image. Do this with the board on its own USB power and the JP2 shunt in.

   ```text
   TAG=FLASH-O4s5 c2000-launchpad/bench/build.sh c2000-launchpad/bench/power/profile2 4 FLASH -DITERATIONS=2000 -DPERFORMANCE_RUN=1 --opt_for_speed=5
   ```

   You should see: The script prints the RAM and FLASH use and the path `c2000-launchpad/bench/power/profile2/build-FLASH-O4s5/bench.out`.

   If it cannot find the compiler or C2000Ware, the two paths are at the top of `build.sh`; change them to your install.

2. **Terminal (Mac).** Flash first: the board's own debugger cannot connect once the chip is powered from the wire (error -2131 on every attempt). Mode `go` writes the image to flash, starts it, disconnects and leaves it running.

   ```text
   /Applications/ti/ccs2101/ccs/scripting/run.sh c2000-launchpad/tools/c2krun.js c2000-launchpad/bench/power/profile2/build-FLASH-O4s5/bench.out go
   ```

   You should see: The scripting runner loads the image and disconnects; the program keeps running on the board.

   If the probe says a firmware update is required, the F2800137's probe needs firmware 3.0.0.43 before TI's tools connect (see c2000-launchpad/README.md).

3. **Bench.** Pull the 3V3 jumper from J101 on the LP-MSPM0L1306 and leave the other eight in. Pull the +3V3 shunt from JP2 on the LAUNCHXL-F2800137 and leave +5V0 in. Wire J101's probe-side 3V3 pin to the chip-side JP2 +3V3 pin, and a GND pin on J2 of the MSPM0 board to a GND pin on JP5 of the C2000 board. Do not plug the USB cables in yet.

   You should see: Two wires between the boards, two parts removed, both USB cables out.

   If the chip never starts after step 4, the GND wire is missing: the 3.3 V has no return path.

4. **Terminal (Mac).** Start the capture first, so the trace begins at power-up, using the serial number of the LP-MSPM0L1306's probe (`--list` shows it). Then plug in both USB cables. The C2000's own USB still runs its debugger; the MSPM0 board's probe is the meter.

   ```text
   python3 tools/energytrace/etcap.py --list
   python3 tools/energytrace/etcap.py --serial <MSPM0 probe serial> --seconds 150 --csv c2000-launchpad/bench/power/f28-run.csv --json c2000-launchpad/bench/power/f28-run.json
   ```

   You should see: The tool samples 2,000 times a second for 150 s and writes the CSV and a JSON file with read statistics.

   If a capture ends early with fewer samples than its length, run it again; the tool restarts the stream after 2 s of silence and records read errors in the JSON. A press of the C2000 board's reset button restarts the profile without a rewire.

5. **Terminal (Mac).** Open the CSV and find each phase by its edge. Subtract the HALT reading (the lowest, last phase) from the others: that reading holds the board's own CAN transceiver, power LED and encoder buffer, so every chip figure is 'above HALT'.

   You should see: Raw readings in the recorded run: busy spin 38.72 mA, CoreMark from flash 41.46 mA, IDLE 29.33 mA, 60 MHz spin 22.93 mA, STANDBY between wakes 8.68 mA, HALT 6.10 mA. Minus HALT: 35.4 mA running CoreMark, 23.2 mA IDLE, 2.6 mA STANDBY.

   If the STANDBY phase reads about 30 mA, the same as IDLE, the chip was re-entered too soon after each watchdog wake; the image waits 30 ms first, so check you built `profile2`, not `profile`.

6. **Bench.** Unplug both USB cables. Put the 3V3 jumper back in J101 and the +3V3 shunt back in JP2.

   You should see: Both boards as they came.

   If a board does not power on from USB afterward, one of the two parts is not seated.

### Expected result

A 150 s trace with clear edges between phases. Above the HALT reading: about 35.4 mA running CoreMark at 120 MHz, 23.2 mA in IDLE and 2.6 mA in STANDBY, 828.1 uJ per CoreMark iteration.

### What this shows

- A board with no meter of its own can be measured with another LaunchPad kit's EnergyTrace probe and two wires: 35.4 mA running CoreMark from flash at 120 MHz and 2.6 mA in STANDBY, above a 6.10 mA HALT reading.
- Energy per CoreMark iteration: 828.1 uJ ((41.46 - 6.10) mA x 3.3 V x 14.19 s / 2,000).
- About 263 uA per MHz between 60 and 120 MHz.

### Limits

- Coin-cell or battery life: not measured, and no battery was wired in.
- The chip's own HALT current: the 6.10 mA reference includes the board's CAN transceiver, power LED and encoder buffer.
- A second board of the same kind or a second run on another day: one board, one rewire (a second and third capture agree).
- The model-inference profile: it was flashed but the probe stopped streaming at full rate, so no row exists (energy_inference is waiting).
- Needs an LP-MSPM0L1306 (or another EnergyTrace-capable LaunchPad) as the meter.
- The probe supplies about 75 mA at most; a motor or a BoosterPack on the C2000 board would trip it.
- The board's own debugger cannot connect while the chip is on the wire; flash first.
- Checked on one Intel Mac, CCStudio 21.0.1.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Sampling | 2k samples a second through CCStudio's XDS110 driver, no IDE | `tools/energytrace/etcap.py` |
| HALT reference | 6.10 mA raw (PLL and oscillators off), subtracted from every chip figure | `data/results/f2800137.csv` |
| STANDBY | 8.68 mA raw between wakes (sd 0.16); 30 ms wait after each watchdog wake | `docs/experiments/2026-10-07-f2800137-borrowed-energytrace.md` |
