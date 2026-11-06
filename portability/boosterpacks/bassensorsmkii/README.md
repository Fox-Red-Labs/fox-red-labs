# BP-BASSENSORSMKII sensors on an MSPM0 LaunchPad, in plain C

**Short answer.** Six small drivers (TMP117, HDC2080, OPT3001, DRV5055, BMI160, BMM150) sit on a five-function port interface and need no vendor library beyond the board's own I2C, GPIO and ADC calls. On the LP-MSPM0G5187 the hello example read humidity and temperature (22.0 C, 44.7 percent RH), light (about 4000 lux) and the magnet sensor (1.638 V, no magnet) once a second on 2026-10-20, with a second BoosterPack (the sub-1 GHz radio) stacked as well. The TMP117 and the BMI160 motion sensor did not answer, cause not found, so the BMI160's 100 Hz stream could only be proven with a stand-in: the host loop held 100.000 Hz for 62.4 s with no sample lost. The same code builds for the LP-MSPM0G3519 and LP-MSPM33C321A (not run there).

Terms: [vocab](../../../docs/vocab.md). Which sensor works with which pins: [COMPAT-G5187.md](COMPAT-G5187.md).

## What it covers

| Sensor | Address | Driver call | Tested on the G5187 |
|---|---|---|---|
| HDC2080 humidity and temperature | 0x40 | `bs_hdc2080_read` | Yes: 22.0 C, 44.7 percent RH |
| OPT3001 light | 0x44 | `bs_opt3001_read_clux` (single shot, 100 ms) | Yes: 3978 to 4098 lux, changes every read |
| TMP117 temperature | 0x48 | `bs_tmp117_read_mc` | No: no answer, ribbon cable in every fit |
| DRV5055 magnet, analog | pin 2 | `bs_drv5055_read_mv`, `bs_drv5055_dmt` | Yes: 1.637 to 1.639 V at rest (sheet: 1.59 to 1.71 V) |
| BMI160 acceleration and rotation | 0x69 | `bs_bmi160_init`, `bs_bmi160_read` | No: no answer at 0x68 or 0x69 |
| BMM150 compass behind the BMI160 | 0x13, on the BMI160's own bus | `bs_bmm150_init`, `bs_bmm150_read_raw` (raw counts, no factory trim) | No: needs the BMI160 |

Integer maths only. The sensors that need a header pin for power (pins 6, 24, 27, 28) are switched by `bs_power_all`.

## Layout

| Path | What |
|---|---|
| `include/bassens.h` | The driver API |
| `include/bs_port.h` | What a board provides: I2C write and read, power-pin write, ADC millivolts, millisecond delay, microsecond clock |
| `src/` | One file per sensor, plus `bs_core.c` for power, register helpers and the scan |
| `examples/hello` | One line per sensor per second on the backchannel UART |
| `examples/motion_stream` | BMI160 at 100 Hz as CSV; `-DBS_STREAM_STANDIN` reads an HDC2080 register burst instead |
| `examples/diag_imu` | Probes for why the BMI160 is silent |
| `boards/` | `gen_syscfg.py` writes each board's SysConfig file from one pin table; `mspm0_common/bs_port_mspm0.c` is the port for all three; `build.sh` builds |
| `tests/run.sh` | Conversion checks against a fake I2C bus, no hardware |

## Builds

TI Arm Clang 5.1.1, SysConfig 1.28.0, MSPM0 SDK 2.11.00.07 and MSPM33 SDK 1.04.00.00, `-Os`. Code and constants in bytes; RAM 1.0 kB everywhere, of which 1 kB is the stack the build script sets.

| Board | hello | motion_stream | Run |
|---|---|---|---|
| LP-MSPM0G5187 | 7408 | 3984 | Yes (hello, stand-in stream) |
| LP-MSPM0G3519, top site | 7296 | 3872 | No |
| LP-MSPM33C321A, site 1 | 7408 | 4080 | No |

On the M33 the DRV5055 read returns -1 (the ADC is a different peripheral, not ported) and header pins 24, 27 and 28 have no GPIO group: TI's board file names them PA16, PC5 and PC4 and SysConfig for that package offers none of them. Powering those three sensors there needs wires on the pack's top pins (OPT3001 and TMP117 to 3V3, DRV5055 power to ground); not run.

## What was seen on 2026-10-20

Expectation first (written before the run, [COMPAT-G5187.md](COMPAT-G5187.md)): HDC2080, OPT3001 and DRV5055 read; TMP117 and BMI160 stay silent. Result: matches.

| Check | Expected | Seen |
|---|---|---|
| I2C scan | 0x40, 0x44 (0x48 and 0x69 if they wake) | `40,44` in every line of two runs |
| Temperature | Room temperature | 22.0 C (earlier runs on 2026-10-10: 24.4 C) |
| Humidity | 30 to 60 percent in a lab | 44.7 percent |
| Light | A few hundred lux at the desk | About 4000 lux, steady within 3 percent over 90 s (3978 to 4098), different on every read. The room was brighter than on 2026-10-10 (300 lux). The covered-sensor check was not done |
| Magnet, no magnet | 1.65 V, within 0.06 V | 1.637 to 1.639 V |
| TMP117 against HDC2080 | Within 0.5 C | Not tested: no TMP117 reading |
| BMI160 flat | 1 g on one axis | Not tested: no reading |

The BMI160 probe: a soft-reset write and a chip-ID read at 0x68 and 0x69 both fail, and a read-only scan finds only 0x40 and 0x44, with all sensors powered. The probe with the other three sensors powered off did not finish, so it shows nothing; the earlier 2026-10-10 runs with the bus recovered and the lines driven by software found nothing either.

## The 100 Hz stream

Expectation: the host reads a 15-byte burst every 10 ms and prints one CSV line (about 36 bytes), 31 percent of a 115200 baud line, so no loss. Seen with the stand-in: 6.2k samples (6241) in 62.4 s, 100.000 Hz on the chip's microsecond clock, spacing 10000 to 10001 us, no missing sequence number, no gap over 15 ms. The columns are `seq,host_us,sensor_ticks,ax,ay,az,gx,gy,gz`; with a BMI160, `sensor_ticks` is its own 24-bit clock (39.0625 us a tick), so spacing is known even if the host reads late. The stand-in values are not motion data.

## Run it yourself

```bash
bash portability/boosterpacks/bassensorsmkii/tests/run.sh                                  # conversion checks, no board
python3 portability/boosterpacks/bassensorsmkii/boards/gen_syscfg.py                       # rewrite the SysConfig files
portability/boosterpacks/bassensorsmkii/boards/build.sh mspm0g5187 hello                   # build only
portability/boosterpacks/bassensorsmkii/boards/mspm0g5187/run.sh hello 20 /tmp/hello.txt   # flash, read 20 s (needs the probe free)
```

When the BMI160 answers:

```bash
portability/boosterpacks/bassensorsmkii/boards/mspm0g5187/run.sh motion_stream 62 /tmp/raw.txt
python3 portability/boosterpacks/bassensorsmkii/tools/analyze_motion.py /tmp/raw.txt /tmp/motion.csv
```

The analyzer prints the sample count, the rate on both clocks and the gaps.

## Limitations

- **Tool or docs.** The BMM150 setup follows the BMI160 data sheet's secondary-interface sequence and has never run; the compass values are raw counts without the BMM150's trim correction.
- **Unknown (suspected: setup or handling).** The TMP117 and BMI160 stay silent on this unit; the ribbon cable was tried in every fit.
- **Setup or handling.** On the G5187 only pins 1 to 10 and 21 to 30 reach the pack, so the two LEDs (39, 40) and the motion sensor's INT2 (13) are unreachable. The drivers poll and use no interrupt pin.
- **Setup or handling.** I2C runs at 100 kHz with a stop between the register write and the read. A repeated start is not used.
- **Tool or docs.** The OPT3001 conversion arithmetic and the BMI160 scaling at plus or minus 4 g and 500 degrees a second are checked by the host test against hand-computed values, not against a reference light meter or a known tilt.
