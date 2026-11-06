# AGENTS.md: BP-BASSENSORSMKII drivers (TMP117, HDC2080, OPT3001, DRV5055, BMI160, BMM150)

Plain C drivers for the six sensors on the BP-BASSENSORSMKII. Each line says what shows it. Terms: [docs/vocab.md](../../../docs/vocab.md). Which sensor works on which pin of the LP-MSPM0G5187: [COMPAT-G5187.md](COMPAT-G5187.md).

| Fact | Evidence |
|---|---|
| Build: `portability/boosterpacks/bassensorsmkii/boards/build.sh <mspm0g5187\|mspm0g3519\|mspm33c321a> <hello\|motion_stream\|diag_imu>`; the output folder is set in `boards/build.sh` | built on all three, README table |
| Run on the LP-MSPM0G5187: `boards/mspm0g5187/run.sh hello 20 <out.txt>`; reads the LaunchPad kit's USB serial port at 115200 | hello ran on the G5187, README table |
| A board provides five functions in `include/bs_port.h`; the MSPM0 and MSPM33 port is `boards/mspm0_common/bs_port_mspm0.c` | built on three boards |
| Pins come from one table in `boards/gen_syscfg.py`; edit it and rerun, never edit the generated `bassens.syscfg` | built |
| Power: pins 6 and 24 low = on, pins 27 and 28 high = on; `bs_power_all(1)` does it and waits 50 ms | HDC2080, OPT3001 and DRV5055 read on the G5187 |
| The TMP117 needs the ribbon cable J5 to J6 and pin 28 high; cable fitted end for end shorts SCL to ground | pack user guide SLAU829, Figure 9 (J5 and J6 are numbered in opposite order) |
| Light: single shot, config 0xC200, wait for bit 7, result = mantissa << exponent in 0.01 lux | hello ran on the G5187 (about 4000 lux) |
| HDC2080: write 0x01 to 0x0F, poll bit 7 of 0x04, read 0x00 to 0x03; ID 0x5449 at 0xFC | hello ran on the G5187 |
| BMI160 at 0x69 and BMM150 behind it did not answer on the G5187, read or write, at 0x68 or 0x69 | the IMU probe on the G5187, README "What was seen" |
| A 100 Hz loop with a 15-byte burst and 36-byte CSV lines at 115200 baud loses nothing | the 62 s stand-in run, README "The 100 Hz stream" |
| On the M33 the DRV5055 read returns -1 and pins 24, 27, 28 have no GPIO group | `boards/gen_syscfg.py`, SysConfig error for PA16, PC5, PC4 |
| Check with no board: `tests/run.sh` | passes |
