# How do you bring up a new motor, driver or power supply on the LP-AM13E230 and measure it?

Run this checklist each time the motor bench changes: a new supply, a new motor, a new driver board or a new controller. It comes from bringing the AM13 SDK's Hall FOC firmware up on a DRV8376EVM and a BLY171S motor, then running it with no Hall sensors (2026-10-15 to 2026-10-17). Each step names what it caught on that bench.

Terms: [vocab](../vocab.md). FOC is field-oriented control; a Hall sensor reports the rotor position.

## 1. Write down what you expect before anything moves

For every run, write the prediction first: current, speed, time to close the loop, what would count as a miss. Afterwards, sort every miss into one of three kinds:

- **Model error:** the simulation leaves something out. The speed loop model predicted an 11 ms rise; the board took 50 ms, because the model had no Hall timing and no speed ramp.
- **Parameter error:** the motor or board is not what its label or its preset says. The ESMO observer read the rotor angle 31 degrees off because its gains came from another motor.
- **Hardware or calibration deviation:** a part behaves differently from its documentation. The firmware's automatic Hall calibration left the Hall angle 49 degrees off the rotor, so Hall FOC drew about 1.5 times the current it needed.

## 2. Put the limits in the firmware, not the host

A page or a script cannot keep a motor safe; the board must. The guard (`am13e230/foc-drv8376/frl_guard.h`) stops every run on its own:

- a run time cap, and a stop 1 s after the host goes quiet;
- a phase-current trip checked every control interrupt (start at 1.0 A on a new motor, whatever its rating);
- a speed clamp;
- a bus voltage window that refuses a start and stops a run outside it;
- a stop if the background loop hangs.

Time the guard from the CPU's cycle counter: the solution's background tick is not a millisecond.

## 3. Change the supply on purpose

Set the guard's bus window for the supply before connecting it, and prove it refuses the wrong one:

| Supply | Window | Why |
|---|---|---|
| 12 V 0.5 A adapter | 10 to 14.5 V | the bench default |
| 20 V tool battery pack (five cells) | 15.5 to 21.5 V | 15.5 V keeps each cell above about 3.1 V; a pack fresh off the charger read 21.2 V on the board (2026-10-22) |

`python3 tools/motorlink/campaign.py supply pack20` sets it; with the window set for the pack and the adapter still connected, a start must be refused ("refused: bus out of range"). Check the driver board's rating (the DRV8376EVM takes 4.5 to 65 V, the BOOSTXL-DRV8323RS 6 to 54 V), and that the board's bus reading spans the new voltage (the DRV8376EVM reads up to 81 V). A battery needs a fuse in its lead every time: it can deliver far more current than the board.

Size the supply wire to the fuse, and the fuse to the board, before a battery goes on: a 5 A blade fuse and 20 AWG or heavier wire between the pack and the driver board. Small jumper wires carry about 1 A; on 2026-10-19 two of them between a 20 V tool pack and the DRV8376EVM melted behind a 30 A fuse, which lets the wire fail long before the fuse does.

Power the motor supply before the controller boots, or reset the controller after the supply is on. The AM13 FOC firmware measures the current-sense offsets once, at boot; if the driver's amplifiers were unpowered then, every phase reads a fixed false current. On the 20 V pack (2026-10-18) the board had booted with the pack off: all three phases read -1.53 A at standstill, and every start tripped within 1 ms at a "1.54 A peak" that was never there. Before the first run on a new supply, read the three phase currents at standstill; each must be within about 0.05 A of zero (`tools/motorlink/pack_campaign.py` checks this before every run).

## 4. Check the driver's own limit and fault pins before enabling it

If the DRV8376 reports a fault before the motor is enabled, its current-limit pin is floating: set it before enable. Read the driver's fault output and route it to the PWM trip. On a driver with SPI (DRV8323RS), read its fault registers too.

## 5. Find out what the motor really is

A new motor's label is a hypothesis. Measure:

- pole pairs, from Hall edges per mechanical turn;
- Hall order and type, by turning the shaft slowly by hand with the Hall lines logged;
- winding resistance, inductance and back-EMF, with the SDK's motor identification or a bounded test.

Feed the measured values to the model before predicting anything.

## 6. Check the angle, not just the spin

A motor can turn smoothly on a wrong angle and simply draw more current. Shift the FOC angle in steps at a steady speed and read the phase current: it is lowest at the true rotor angle (`campaign.py sweep`). Run a sensorless observer beside the Halls and compare (`frlGuard.obsMode`): two independent readings that agree are worth more than one calibration flag.

## 7. Measure with the vendor's own live link

The firmware streams variables over the LaunchPad kit's USB serial port (MotorVision). Use it before building any instrumentation of your own, and read the indices from the firmware's table, not the JSON beside it: in SDK 26.01.00.03 the two disagree.

## 8. Record it the same way every time

Write the expectation first. Keep the raw readings. Add a result row per number in `data/results/`, and a write-up with what it shows and its limits. Sort the misses into the three kinds above. Read the board's state through the debugger before any reset: a reset erases it.

## What changes from board to board

| Board change | What to redo |
|---|---|
| New supply | Steps 3 and 4 |
| New motor | Steps 1, 5 and 6; guard trip back to 1.0 A |
| New driver | Steps 2 to 4, the current-sense gain and offset, then 5 and 6 |
| New controller | Everything; the guard and the observer code are the parts that move with it |

Worked example with the numbers: [LP-AM13E230 FOC README](../../am13e230/foc-drv8376/README.md).
