# TI's three-shunt Hall FOC on the DRV8376EVM and the LP-AM13E230: port, pins and what ran

Terms: [LP-AM13E230 FAQ](../FAQ.md). FOC (field-oriented control): the motor's three currents are turned into two numbers, one that makes torque and one that does not, and each is held by its own loop. The six-step program that proved the board and pins: [`../bldc-drv8376`](../bldc-drv8376/README.md). TI's own solution is `solutions/motor_control/ufoc_lp_bxl/3sh_hall_sen_foc_drv8323rh/` in the AM13E2x SDK.

**What ran:** trapezoidal rotation was restored on 2026-10-16 with a measured current-limit reference and a new Hall table (revision 17: two unloaded runs of 133 and 279 consecutive forward Hall sectors, no fault). On 2026-10-17 TI's Hall FOC held closed-loop speed on this EVM, unloaded, at a low reference. Current sign, gain and the over-current trip are not verified: see "What is unproven". The board is stopped.

## Build

```bash
./build.sh
```

The script writes `foc.out`, `foc.map` and `build.log` to a build folder outside the repository and never flashes. It reads TI's real-time library sources and prebuilt libraries in place from the SDK at `/Applications/ti/am13e230x_sdk_26_01_00_03`.

## What was copied, and what changed

Copied from `solutions/motor_control/ufoc_lp_bxl/3sh_hall_sen_foc_drv8323rh/` and `solutions/motor_control/common_files/`: `main.c`, `main.h`, `isr.c`, `system_config/`, `motorvision_config/`, the SysConfig file, and the parts of `common_files/` this build uses (about 14,400 lines of C, headers and SysConfig in all). The 1-shunt and sensorless files, DShot, and the other gate-driver interfaces were left out. The initial port changes, with lines added and removed against TI's file:

| File | Lines | Change |
|---|---|---|
| `common_files/hal/gate_driver_interface/drv8376/drv8376_interface.h` and `.c` | new, 207 and 84 | The DRV8376 interface. Enable order from the DRV8329A (nSLEEP high, 6 ms, DRVOFF low); data in the 3-shunt layout. Scale constants for 1 V/A about nominal 1.5 V, divider 143 k over 6.04 k, over-voltage 60 V, under-voltage 6 V. |
| `foc_drv8376.syscfg` | +62 -67 | The EVM's pins and ADC placement, CMPSS on PA27, PA30, PA29, interrupt on the last ADC2 conversion, debug DAC and `FAULT_OUTPUT` dropped, `ISR_TIMING` to PA5. |
| `system_config/system_config.h` and `.c` | +24 -11, +13 -13 | Driver objects renamed; minimum compare clamp 0.75 to 2.0 us so the current sample lands while the low side conducts. |
| `system_config/user_motor.h` and `mtr1_init.c` | | PWM 50 to 20 kHz; identification currents and the bench speed-loop torque ceiling set to 0.4 A rather than the motor's 1.8 A rating. |
| `common_files/app_lib/include/motor_params.h` and `source/motor_params.c` | +80 -2, +79 -1 | The BLY171S-24V-4000 entry and `motorParams_M1` pointed at it. |
| `main.h` | +5 -2 | Low-priority pass interval rescaled for 20 kHz; debug DAC channel off. |
| `isr.c` | +4 -3 | One port macro. |
| `build.sh` | new, 86 | The command-line build. |

The initial port changed about 650 lines; the plan had estimated 400. The subsequent bench-current limit changed `mtr1_init.c`; `main.c` now explicitly disables the external PA4 DAC output; `mtr1_init.h`, `mtr_config.h` and TI's real-time library remain untouched.

## Pins

All from [`../bldc-drv8376/README.md`](../bldc-drv8376/README.md) (proven on the bench for six-step). The ADC pins give the same ADC and channel numbers as the bench build.

| EVM signal | EVM header pin | LaunchPad pin | In this port |
|---|---|---|---|
| INHA, INLA | J4 40, 39 | PB3, PA7 | MCPWM4 1A, 1B (same as TI) |
| INHB, INLB | J4 38, 37 | PB1, PA2 | MCPWM4 2A, 2B (same) |
| INHC, INLC | J4 36, 35 | PB2, PA25 | MCPWM4 3A, 3B (same) |
| HALL A, B, C | J4 34, 33, 32 | PB7, PB6, PA12 | GPIO inputs; InputXBAR 4, 3, 5 to ECAP1 for speed |
| nSLEEP | J1 9 | PA23 | Output, starts low; high = awake |
| nFAULT | J1 8 | PB4 | Input; InputXBAR2 to the PWM trip zone (OST2); EVM J9 pin 6 |
| DRVOFF | J2 13 | PB18 | Output, starts high; low = outputs on |
| ISENA | J3, 27 to 29 | PA27 | ADC1 SOC0; CMPSS1 over-current comparator |
| ISENB | J3, 27 to 29 | PA30 | ADC2 SOC0; CMPSS2 |
| ISENC | J3, 27 to 29 | PA29 | ADC2 SOC1; CMPSS0 |
| VSENA | J3 26 | PA17 | ADC0 SOC0 |
| VSENVM | J3 23 | PA16 | ADC0 SOC1 (the bus) |
| VSENB | not recorded | PA28 | ADC2 SOC2 (last, raises the interrupt) |
| VSENC | not recorded | PA18 | ADC1 SOC1 |
| (MotorVision link) | | PA0, PA1 | UART UC1 at 9.2 Mbaud, the LaunchPad backchannel pins |
| (ISR timing) | none | PA5 | LED4, active low, high during the control ISR |
| (IPD capture) | none | PB0 | LED5, active low; needed by SysConfig, moves only on a comparator trip |

What TI used that the EVM cannot take: GATE_EN PA3, GATE_MODE PB7, GATE_GAIN PB12, GATE_CAL PB11 (the DRV8323RH's mode, gain and calibration pins; the EVM sets them with resistors), `FAULT_OUTPUT` on PB6 (Hall B), `ISR_TIMING` on PA23 (nSLEEP), IPD output on PA11, and the unconstrained debug DAC on PA4. PA4 is left undriven with its DAC disabled; the present setup uses the hardware divider. Phase current and voltage placement is TI's names with a new meaning: `MTR1_IU_SEN` is ISENA, `IV` is ISENB, `IW` is ISENC, `VU` is VSENA, `VV` is VSENB, `VW` is VSENC.

## SDK audit, 2026-10-16

The current port uses the separate nominal 3.0 V CSA reference and calculates its initial zero-current ADC offset from that reference and the MCU's 3.3 V ADC reference. The previous fixed 2048-count seed assumed both references were equal. Startup still calibrates the measured offset. The corrected startup calibration has now run; it does not establish winding-current gain or polarity. See [SDK audit](SDK-AUDIT.md).

## Before closed-loop operation

Commissioning startup ran. These physical and feedback checks remain.

1. **Retain the reported external-reference setup.** The user reports J2 center connects to the separate J3 onboard reference pin; J4 selects DIV and J6 remains open. The six-step idle readings are near half of the nominal 3 V reference. ILIMIT voltage is still unmeasured. Do not substitute GVDD for the reference feeding these 3.3 V ADC inputs.
2. **Read the gain and slew resistors on the board** (lab camera or loupe). The constants assume 1 V/A (one count 0.806 mA, linear to +/-1.25 A at nominal VREF=3.0 V, full scale 3.3 A); the DRV8376EVM guide lists the other settings. Gain is `DRV8376_CSA_GAIN_V_PER_A` in `drv8376_interface.h`. If the slew is the 50 V/us setting, `SYS_M1_ADC_NOISE_WIN_uS` in `system_config.h` goes from 1.3 to 5.8.
3. **Establish the current sign independently before alignment.** `DRV8376_SIGN_CURRENT_SF` remains an unverified +1.0. Use a separately validated bounded stimulus with known physical current direction, then compare the sense output with its idle baseline. Closed-loop Hall alignment assumes the sign is correct and cannot establish its own feedback polarity. That independent test is still outstanding; do not use alignment to discover the sign.
4. **Resolve the driver fault before FOC.** PB4 now follows the fault/reset transitions and feeds the GPIO and InputXBAR trip path; the earlier PA22 route is disconnected at R25. R33 also appears absent, so the older PA4 DAC setting was not evidence of an ILIMIT voltage. Current firmware disables that DAC. The FOC software torque ceiling remains 0.4 A; that is not a measured hardware limit.
5. **Wiring as for the six-step program:** EVM J13 on GVDD, the five Hall wires, motor on OUT A, B, C, LaunchPad S6 and S4 on BP, EVM on J1 to J4 with the terminals away from USB-C, 12 V on VBAT and PGND, USB-C last ([`../bldc-drv8376/README.md`](../bldc-drv8376/README.md), "Hardware").

## How to run it

The default build installs a commissioning console if explicitly flashed. It does not enter TI's normal motor state machine.

### Reference for future closed-loop operation

TI's normal control path has run: on 2026-10-17 TI's Hall FOC held closed-loop speed on this EVM (commissioning revision 39, key U, with the DRV8376 cycle-by-cycle limit off; then under the bench guard in the normal image, driven over MotorVision by `tools/motorlink` and the motor console). The table below is TI's variable map for that path; the bench guard (`frl_guard.h`) and its limits are in `tools/motorlink/README.md`.

| Do | Variable | Expect |
|---|---|---|
| Wait for start-up | `motorVars_M1.statusVars.flagOffsetCalibDone` | True after the comparator calibration (about 3 x 12 steps of 2 ms) and a 2 s current-offset average. Then `halFOCDataObj_M1.adcData.offset_I_ad.value[0..2]` near the measured VREF/2 baseline (nominal 1,862 counts for a 3.0 V CSA reference and 3.3 V ADC reference) and `halFOCDataObj_M1.adcData.VdcBus_V` near the supply. |
| Calibrate the Hall table once | `motorVars_M1.inputCtrl.flagStartHallCal` = true | State `MOTOR_HALL_CAL`, align, a forward and a reverse sweep, then back to idle. It settles the Hall angle offset after current polarity has been independently validated. A failed calibration raises `faultMtrNow.bit.estimError`. |
| Start | `motorVars_M1.inputCtrl.flagEnableRunAndIdentify` = true, `speedRef_Hz`, `direction` (1.0 or -1.0) | `speedRef_Hz` is electrical: 4 pole pairs, so rpm = Hz x 15; the default is 40 Hz (600 rpm). TI initializes `flagEnableFlyingStart` to true, and then the sequence in `outVars.motorState` is `MOTOR_SEEK_POS`, `MOTOR_ALIGNMENT` (`AlignDelay_sec`, 5 s at 0.4 A), `MOTOR_OL_START`, `MOTOR_OL_END`, `MOTOR_CTRL_RUN`. Set it to false first to take the Hall torque-start instead: `MOTOR_HALL_STARTUP` (a fixed 0.4 A of torque current while Hall edges are counted), then `MOTOR_CTRL_RUN`. Which one TI intends for this solution is not stated in the files; both are TI's code, unchanged. |
| Watch speed | `motorVars_M1.outVars.speed_Hz`, `statusVars.speedHall_Hz`, `statusVars.angleHall_rad` | Speed follows the reference. |
| Watch current | `outVars.Is_A`, `outVars.Idq_in_A`, `outVars.IdqRef_A`, `statusVars.Irms_A[3]` | Software reference ceiling is 0.4 A; phase-current gain/polarity and actual hardware limiting remain unverified. The adapter rating is not a phase-current limit. |
| Faults | `outVars.faultMtrNow` (live), `statusVars.faultMtrUse` (masked), `statusVars.faultMtrPrev` (history) | Zero. Clear with a pulse on `inputCtrl.flagClearFaults`. The EVM's NFAULT LED shows a driver fault. |
| Stop | `flagEnableRunAndIdentify` = false | Back to `MOTOR_STOP_IDLE`. |

Speed ceiling on this bench: the voltage limit is 0.576 x the bus (about 7.1 V peak phase at 12.4 V), which is about 190 Hz electrical (2,900 rpm) before field weakening, if the flux convention below is right. Keep `speedRef_Hz` under that.

Read the variables without stopping the core. A debug probe that halts the core leaves MCPWM4 running in free-run at its last duty, a fixed voltage on the windings; a DSLite memory read halted the core on 2026-10-08 (`../tools/run_image.py` flashes, waits, then reads once). Whether a CCStudio live watch halts it on this board is unproven. TI's MotorVision link on PA0 and PA1 (9.2 Mbaud) may not pass through the XDS110 backchannel; also unproven.

## What is unproven

| Item | State |
|---|---|
| FOC on hardware | Closed loop at a 10 Hz reference to a 12 s cap, no fault (2026-10-17). TI's default speed gains hunt after a step at 10 to 15 Hz. Control ISR at most 5 us at 200 MHz. |
| Current sign | Unknown; a named constant (above). |
| Gain, slew, VREF | Gain 1 V/A is from the bench notes, not read off the board; slew unread; the six-step image now measures VREF/2 at rest with J2 fitted. |
| ILIMIT jumper and PWM mode | Reported as J4 DIV and J6 open; not independently verified. The six-step image gives weak winding current and rocks after the phase wires were fixed; 3x versus 6x mode is not conclusively settled. Do not enter closed-loop FOC until these are resolved. |
| Flux 0.03675 V/Hz | Reads the label's 2.45 V per 1,000 rpm as phase-to-neutral peak. The datasheet torque constant (4.96 oz-in/A) is 1.497 times the back-EMF constant, which fits that reading (the other readings give 1.22, 0.87 and 2.12 for a ratio that should be 1.5). If wrong: 0.0300 (line-to-line rms) or 0.0212 (line-to-line peak). The six-step bench run read 2.04 V per 1,000 rpm of average bus voltage, not a clean test of this. |
| Pole pairs 4 | From the label (eight poles); Hall edges per turn (should be 24) not counted. |
| Hall order against phase order | TI's default Hall table assumes a wiring order; the calibration step is how this port is meant to settle it. |
| Sample timing | 2.0 us minimum compare assumes the 250 V/us slew or faster. The window and the ADC sample points were worked out on paper, not on a scope. |
| Dead time | The MCU's 100 ns rising and 50 ns falling band is TI's; the chip adds 65 to 250 ns. `SYS_M1_PWM_DEADTIME_uS` stays 0.25 until measured. |
| Fault path | PB4 nFAULT asserted the hardware PWM trip during commissioning. Sleep/wake cleared the driver indication while MCU trip latches remained set. Automatic retry has not been validated. The 6 ms wake wait is from the data sheet (tWAKE 5.5 ms maximum). |
| Over-current comparator | One-sided (trips below the rest level by the limit); nominal limit 1.0 A, inside the calculated +/-1.25 A linear range at 3.0 V reference and 1 V/A. Gain, polarity and actual trip operation remain unverified. |
| PB0 | Carries the IPD capture signal. Whether PB0 is on a header net the EVM uses was not checked. LED5 is the only claim. |
| Gains at 20 kHz | Current-loop gains come from L, R, a 200 Hz bandwidth and the ISR period at start-up, so they follow the new rate; behavior was not simulated or measured. ISR run time (CPU load) not measured; the budget is 50 us. |
| Inertia 2.4e-6 kg.m2 | From Anaheim's 0.00034 oz-in-sec^2, converted; speed-loop gains use it. Untuned. |
| Parameter identification | Bypassed (`flag_bypassMotorId` true, as TI ships it). The 0.4 A identification currents were never exercised. |

## Where it stops

The pulse attempts in commissioning stopped during PWM arming on nFAULT until the current-limit reference was measured and set (revision 17). Closed-loop FOC ran later, unloaded and briefly. No loaded, sustained or current-loop result is claimed.

PA4 update, 2026-10-16: this source now leaves the external DAC output disabled, matching the six-step meter image and the reported J4 DIV configuration. Internal comparator DACs remain configured for protection. Older binaries may still drive PA4; remove the temporary measurement lead before using those binaries. This change does not resolve the driver fault or establish current gain and polarity.
