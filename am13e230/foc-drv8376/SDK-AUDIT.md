# TI AM13 SDK motor examples for the LP-AM13E230 and DRV8376EVM: which one fits

The installed AM13 SDK has the motor algorithms needed for this project. Its supplied examples target DRV8323RH or DRV8329A power boards, so this DRV8376EVM needs a hardware adaptation. This repo's Hall FOC port of the nearest example ran closed loop on the bench, unloaded; current sign, gain and the over-current trip are still unverified ([port README](README.md)). Common questions: [motor bench FAQ](../FAQ.md#drv8376evm-motor-bench).

## Available software, checked 2026-10-16

| Path | What exists | Fit for this bench |
|---|---|---|
| AM13 SDK `ufoc_lp_bxl/3sh_hall_sen_foc_drv8323rh` | Hall-sensored FOC, SVPWM, motor state machine and tuning interface | Closest existing AM13 example; adapted in this directory. Current sign, scale, sampling and fault interface require validation. |
| AM13 SDK `ufoc_lp_bxl/3sh_sen_less_foc_drv8323rh` | Sensorless FOC, FAST/ESMO selection and motor identification support | Available locally; not ported here. Reuses the same power-stage interface work. |
| InstaSPIN/FAST and TI Tune | Identification and tuning routines in the SDK | These exercise the power stage. They depend on valid voltage/current feedback and board configuration. |
| DRV8376EVM reference GUI/firmware | TI's supplied motor demo for LAUNCHXL-F280049C | Correct driver EVM, different controller. The binary cannot execute on the AM13 Arm core. |
| Repo `bldc-drv8376` | Custom six-step controller with previous spin records | Spun on 2026-10-15; the later repair restored rotation with a measured current-limit reference. See its README. |

## What the source audit found before commissioning

 The FOC port still seeded zero current at 2048 ADC counts and described AVDD-only reference selection. That assumes the CSA and ADC references are equal. For the nominal separate 3.0 V CSA reference, the seed is `4096 * (3.0 / 2) / 3.3`, approximately 1861.8 counts, calculated rather than measured. The header now derives the seed from the two reference constants. Startup calibration continues to measure the actual offset. This port has never been flashed, so this mismatch cannot be the cause of the current six-step failure.

The FOC startup calls `HAL_enableGateDriver()` during control-parameter initialization, before the main run loop. It also performs comparator and ADC offset calibration. Consequently, flashing this image with its run flag false is not equivalent to the six-step diagnostic firmware's outputs-disabled boot. A drive-disabled commissioning entry point is needed before using this port for passive remote inspection.

The six-step high/low PWM primitives and nSLEEP/DRVOFF polarity match the previous working commit `eee67735`. The fault input has intentionally changed from PA22 to PB4 because PB4 follows the observed driver fault. Reverting that fault input would conceal a fault, not diagnose it. This comparison does not establish the voltage at each physical driver pin.

The installed MSPM0 SDK also contains a DRV8316 Hall-trapezoidal driver. Its enable path resets nSLEEP, lowers DRVOFF and uses SPI to clear/configure the driver. That SPI path does not apply directly to the hardware-interface DRV8376 on this bench. TI's support advice permits DRV8316 examples as a starting point with GPIO adaptation; it is not a claim that their binaries run unchanged here.

The same reference change reduces the nominal linear current range to +/-1.25 A at 1 V/A. The previous 1.3 A comparator setting fell outside that range. The setting is now 1.0 A, above the 0.4 A initial command limit. A regression check failed on the old threshold and passes on the new one. This is a calculated configuration check, not a measured protection test.

## What happened after the audit

A drive-disabled commissioning entry point was written, flashed and exercised on 2026-10-16: calibration completed, then an arming attempt asserted nFAULT before enable. After the current-limit reference was measured and set, trapezoidal rotation came back, and TI's Hall FOC later held closed-loop speed unloaded ([port README](README.md)). Sensorless FAST and motor identification are not ported.

## Sources

- Installed AM13E230X SDK 26.01.00.03: the two example READMEs, `system_config/mtr1_init.c`, shared gate-driver interfaces and local port source. [TI example list](https://software-dl.ti.com/mcu-sdk/esd/AM13E2X-SDK/26_01_00_03/exports/docs/docs/am13e230x/user_guide/chapters/solutions/am13e230x/motor_control/solutions_motor_control.html).
- [TI motor identification guide](https://software-dl.ti.com/mcu-sdk/esd/AM13E2X-SDK/26_01_00_03/exports/docs/docs/am13e230x/user_guide/chapters/solutions/am13e230x/motor_control/control_motor_tuning.html).
- [DRV8376EVM product page](https://www.ti.com/tool/DRV8376EVM), supported controller and trapezoidal demo.
- [TI support guidance on DRV8376 example source](https://e2e.ti.com/support/motor-drivers-group/motor-drivers/f/motor-drivers-forum/1609444/drv8376evm-source-code-for-drv8376evm-sensored-trapezoidal). The linked legacy ZIP download returned HTTP 403 in this audit; it was not inspected. The locally installed MSPM0 DRV8316 driver was inspected instead.
