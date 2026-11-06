<!-- experiment
id: E268
board: LP-MSPM0G5187
date: 2026-10-24
status: works
reached: verified
line: G3519 DriverLib examples retargeted to the LP-MSPM0G5187 run on it: 15 of the 55 that ran show the behavior TI describes, the three DMA examples hang because SysConfig moves them to DMA channel 1, and the port tool's 41 "name" failures were a stale-file trap.
stop: none
-->
# Do the G3519 DriverLib examples retargeted to the LP-MSPM0G5187 run, and do they do what TI's example does?

**Short answer.** Mostly yes. Of the examples that build, the ones that can be judged without an outside signal behave as TI's README says (timers on time, CRC, ADC buffers, flash controller, SPI internal loopback, UART transmit). The three DMA examples do not: the data moves but the example never sees the done flag. An earlier build pass reported 117 failed builds; 41 of them were names such as `UART0` and `SPI1` not declared; those errors come from stale generated files copied with each example, and with them deleted 79 of 122 `driverlib` examples build instead of 64.

Terms: [vocab](../vocab.md). Rows are in `data/results/mspm0g5187.csv` (variant `g3519-port-<example>`). Checked 2026-10-24 on one LP-MSPM0G5187, TI Arm Clang 5.1.1.LTS, MSPM0 SDK 2.11.00.07, SysConfig 1.28.0 (1.26.3 was no longer installed, so the run is unvalidated).

**Question.** An earlier pass built 141 retargeted G3519 examples for the G5187 and never ran one. Do they run, and does each show what TI's example shows?

**Setup.** `port_mspm0.py` retargets the SysConfig device lines and unlocks pin and channel assignments; `run_sweep.py` flashes and runs with its safety gates; result variables were read through the debugger; pin toggles were timed with a millisecond clock. The board was backed up (main 128 kB and data 8 kB, read twice equal) and restored equal at the end. No external wiring.

**Expectation.** Set before the run: togglers toggle, breakpoint examples stop with the result variables TI names, input examples run idle, `sysctl_hfxt_run` hangs as the AGENTS file already says.

**Result.**

| Finding | Evidence |
|---|---|
| 15 examples match TI's description | systick half-period 497 ms (TI 500), a 2 s timer at 2,000 and 2,064 ms, CRC 0x2C81, flash controller `gErrorType` 0 in four examples, SPI loopback received equals sent, UART transmit `MSP!` on the backchannel |
| 3 DMA examples fail | destination words correct; `gChannel0InterruptTaken` 0 because SysConfig assigned channel 1 and the handler tests channel 0; the native G5187 build passes |
| Stale generated files | the SDK ships `ti_msp_dl_config.c` and `.h` beside each example; the source includes them before SysConfig's output, so a port compiles G3519 names (`UART0`, `SPI1`, `GPIOC`). 64 build with them, 79 of 122 without |
| TI Drivers examples keep the G3519 linker map | `drivers/uart_echo` builds, then locks the debugger out; recovered with the wait-for-debug hold, backup read back equal |
| 37 ran, cannot be judged | need a button, signal or second device; no fault |

**The line.** A build is not a port. After SysConfig, check that `DMA_CHx_CHAN_ID` and the linker file match the chip, and delete the stale `ti_msp_dl_config.*` beside the source. UART receive, I2C, comparators and PWM shape are not confirmed here.

**Gotcha found.** The sweep scripts name SysConfig 1.26.3; with 1.28.0 installed they stop at the first call until the path is changed. The safety gates read load addresses only, so the stack top of a TI Drivers image is not checked.

**Limits.** The 22 low-power and key examples were skipped. `sram_parity` was not retried on the corrected build.
