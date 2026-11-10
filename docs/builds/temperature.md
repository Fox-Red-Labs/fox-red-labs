# Read the board's thermistor once a second

Build a small program that reads the thermistor on the LP-MSPM0L1306, flash it, and watch one line a second on your computer. No wiring: the thermistor is on the board. The line shows raw ADC counts, not degrees.

The page: [site/project-temperature.html](../../site/project-temperature.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Thermistor counts on the LP-MSPM0L1306

Build the three-ways app for the L1306, flash it with OpenOCD, and read t, adc and avg once a second from the board's serial port.

Status: Works. Boards: l1306.

Folders this version uses (in a sparse clone, fetch them first, then read `portability/three-ways/AGENTS.md`):

```bash
git sparse-checkout add portability/three-ways mspm0l1306/tools tools/frl
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM0L1306 LaunchPad development kit | 1 | the board and its thermistor |
| Micro-USB data cable | 1 | power, programming and the serial text |

### Before you start

- A Mac with CCStudio 21.0.1 (TI Arm Clang 5.1.1.LTS), the MSPM0 SDK 2.11.00.07, SysConfig 1.26.3 and xPack OpenOCD 0.12.0. The same tools as the Print a message page; install them there first. Checked on an Intel MacBook on macOS 15.7.9 only.
- The code is in this repository. This page calls the folder you clone it into `<repository folder>`. Run every command from that folder.

### Steps

1. **Bench.** Leave the jumpers as the board shipped: J16 and J17 on the XDS side. Plug the board into the Mac with a micro-USB data cable. Find the probe's serial number and the port name.

   ```text
   ls /dev/cu.usbmodem*
   python3 tools/frl/frl.py detect
   ```

   You should see: Two ports. The one ending in 1 is the chip's serial port at 115200 baud. `frl.py detect` lists the probe serial; use it as `<probe serial>` below.

   If no ports appear, try another cable. A charge-only cable looks the same.

2. **Terminal.** Build the app for the L1306. It reads ADC channel 9 (PA15, the thermistor) every second against the chip's 2.5 V reference.

   ```text
   portability/three-ways/driverlib/build.sh l1306
   ```

   You should see: A clean build in about 3 s that leaves `portability/three-ways/driverlib/build-l1306/three_ways.out` and prints its size (2.3 kB of flash).

   If SysConfig or the compiler is not found, the install paths at the top of `portability/three-ways/driverlib/build.sh` do not match your install.

3. **Terminal.** Flash it with OpenOCD. `ocd.sh` picks the probe by serial number and refuses to run without one. Back up whatever is on the board first; this overwrites main flash.

   ```text
   export PROBE=<probe serial>
   mspm0l1306/tools/ocd.sh "reset halt" "program portability/three-ways/driverlib/build-l1306/three_ways.out verify" "reset run"
   ```

   You should see: OpenOCD ends with `** Verified OK **`. The red LED1 changes state once a second.

   OpenOCD prints `Error: checksum mismatch - attempting binary compare` first; that is normal and the line after it is the real result.

4. **Terminal.** Record 12 seconds of the board's serial port and check the lines. Replace the port with yours (the one ending in 1).

   ```text
   python3 portability/three-ways/serial_check.py /dev/cu.usbmodem<probe serial>1 12 /tmp/run-driverlib-l1306.txt
   ```

   You should see: About 12 lines like `t=5 adc=2645 avg=2645`, one a second, then `PASS`. Expect roughly 2,600 to 2,700 counts at room temperature.

   If no lines appear, check the port ends in 1 and that J16 and J17 are still on the XDS side. If the checker prints FAIL, it names the first check that failed (t not counting up, avg wrong, gaps off one second).

### Expected result

One line a second on your Mac with the thermistor's raw reading near 2,645 counts at room temperature.

### What this shows

- The L1306's onboard thermistor can be read by a small DriverLib program and printed once a second over the board's own USB cable.
- In TI's out-of-box demo the same thermistor read 22.8 °C.

### Limits

- A temperature in degrees: this app prints raw ADC counts. The 22.8 °C figure came from TI's demo, not from this app.
- That the chip's own temperature sensor works: it read -165 °C on this chip with the factory trim (suspected wrong trim).
- That warming the thermistor changes the counts: no recorded run does this.
- The counts are against the 2.5 V internal reference with a 3.3 V supply assumed; the supply was not measured in this run.
- One board was run. No reference thermometer was used, so the counts are not an accuracy test.
- The ADC sample window in this app is 125 µs. TI's faster 8-cycle window reads the thermistor 1.7% low.

### Numbers and sources

| | Value | Source |
|---|---|---|
| App | portability/three-ways common/app.c with glue_driverlib.c: 4-sample moving average, one line a second at 115200, ADC0 channel 9 on PA15 | `portability/three-ways/README.md` |
| Result | thermistor PA15 about 2645 counts, avg 2644 (2684 on 2026-10-07); serial_check.py PASS, 12 s | `data/results/mspm0l1306.csv` |
