# TI LP-MSPM0G5187 third-party, loop and speed-run results

**Short answer.** The G5187 matches its sibling, the LP-MSPM0G3519, on every test below, because the core, clock, AES block and CRC block are the same. AES-128 hardware is 45.5 times faster than software on 1 kB, hardware CRC-32 is 8.1 times faster than a table, 18 of 19 Embench programs pass, and the UART loop is clean up to 10.67 Mbaud with a bad patch near 5.5 and 6.0 Mbaud.

Measured 2026-10-09 on an Intel Mac, MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, 80 MHz, code in flash, probe <probe serial>. The board's backchannel UART was off (the loop wire was fitted), so each program wrote its report into RAM and OpenOCD read it back. Every row is in `data/results/mspm0g5187.csv`. Terms: [vocab](../docs/vocab.md). 1 kB = 1,024 bytes.

| Test | Result | Compared with | Notes |
|---|--:|---|---|
| AES-128, 1 kB, CBC encrypt | 45.5 times faster (9.6k cycles against 439k) | G3519: 45.8 | ECB decrypt 78.5 times; outputs match the FIPS-197 and SP 800-38A vectors |
| CRC-32, 1 kB | 8.12 times faster (1.9k cycles against 15.4k) | G3519: 8.09 | check value 0xCBF43926 |
| Embench-IoT | 18 of 19 pass at -O2 and -Os | G3519: 18 of 19 | `st` fails its own check, as on the G3507 and G3519 |
| Dhrystone 2.1, -O3, no inlining | 0.651 DMIPS per MHz (91.5k per second, 874 cycles each) | L1306 0.731 at 32 MHz | lower at 80 MHz with two flash wait states; not tested further |
| 256-point FFT, 80 MHz | q15 0.918 ms, q31 3.273 ms, f32 10.953 ms | G3519: 0.918, 3.215, 11.055 | cycle counts 73.4k, 262k, 876k |
| UART loop, PA10 to PA11 | clean up to 10.67 Mbaud | G3507 and G3519 | 5.5 Mbaud: 72 percent of a 2 kB test right; 6.0: 94 percent; 12 Mbaud sends nothing |

Every cycle count was identical on three runs. Dhrystone used 208 bytes of its 1 kB stack.

## The sweep

174 of the 203 makefile examples build (85.7 percent). Of those, 88 were flashed and run for 4 s each and read over the debugger, with the UART loop wire fitted, so nothing printed and the verdict comes from the program counter and the pin registers. 86 were skipped on purpose with a reason on file.

| Verdict | Count |
|---|---|
| Running in its main loop or a delay | 63 |
| Stopped at its own end-of-example breakpoint | 16 |
| Stopped in sensor start-up (`fall_detection_ai`, `fall_detection_ai_npu`: no BMI270 on this board) | 2 |
| Hung at boot waiting for a clock | 7 |
| Hard fault | 0 |

The 7 that hang sit in `SYSCFG_DL_SYSCTL_CLK_init`, a loop that waits until the system PLL, high-speed clock and low-frequency oscillator are all good. Read live on `ecg_anomaly_detection_ai`: the clock status register (0x400B0204) was 0x22202650, so the PLL and high-speed clock are good and the low-frequency oscillator bit (0x800) is never set. They are `sysctl_hfxt_run`, `sysctl_mclk_syspll`, `ecg_anomaly_detection_ai` and its `_npu` twin, and the three `motor_bearing_fault` examples. The cause is not confirmed (the other NPU examples, whose generated clock code has no such wait, run fine). These 7 are counted as not clean.

Skipped, with the reason in each row: 61 by the tool's gates before the board was touched (outside main flash and SRAM, risky, low power, pin gate on PA10 and PA11 which carry the loop wire), and 25 that need a person or a wire (TI's Edge AI Studio capture and live-preview examples, LIN, PMBus, SMBus, and the I2C and SPI controller or target examples that need a partner board). Low-power examples were not run, because they can drop the debug link and need a replug.

**Second pass, 2026-10-10.** With the UART loop wire off and the backchannel serial port back, 32 of the 40 examples first skipped for using PA10 or PA11 (plus a few with no generated pin file) were flashed and run for 6 s each with the serial port captured on the Mac and the Sensors BoosterPack stacked. 29 ran in their main loop or a wait, 1 stopped at its own end-of-example breakpoint (SPI internal loopback), and 2 stopped in the BMI270 start-up of the live-preview examples that need TI's TIDA-010997 sensor board (the BoosterPack here has a BMI160 instead). No hard faults and no flash failures. Serial text appeared from four examples: the console test banner, a Manchester packet, three bytes from the UART controller and one byte from a data-capture example; the other UART examples wait for input from a partner. Totals with the first pass: 120 run, 109 clean (90.8 percent). Not run: 8 BSL, risky and out-of-range examples (by design), 4 EEPROM-emulation examples (they write data flash), 18 low-power examples (they can drop the debug link and need a replug), and 25 that need a partner board or a GUI. TI's factory image was restored afterwards and read back identical (main, data and BCR flash).

## Factory image

After all runs the main flash held the Dhrystone image. TI's 424 B factory image was written back with OpenOCD and read back byte for byte.

## Speed run (2026-10-17)

**Short answer.** The G5187 now has its core, security, control and sweep rows. ML-KEM-768 runs on it in 25, 30 and 36 ms at -Os and passes every NIST known-answer test; ML-DSA-44 does not fit its 32 kB of RAM. FreeRTOS switches tasks in 7.0 us, the AES engine is 45 times faster than software, and 65 of 65 GCC-built examples ran without a fault. Probe <probe serial>, MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, 80 MHz unless said. The board carried the alarm-listener image first; it was backed up (main flash SHA-256 57f24720...170ecc) and written back at the end, read back identical (main, data flash, BCR, BSL config).

| Test | Measured | Notes |
|---|---|---|
| rtos_task_switch (FreeRTOS, taskYIELD round trip) | 560 best, 564 mean, 718 worst cycles: 7.0 us best | identical on 3 flashes; TIMG0 counts at half the CPU clock (the image proves it against SysTick), so the resolution is 2 cycles |
| rtos_max_tasks, 128-word stacks, heap | 51 | same test as the M33's 397, CC2340R5's 41, U083's 149 |
| rtos_max_tasks, 48-word stacks, static | 122 links, 123 does not, all 122 ran | the L1306 fits 11 in 4 kB |
| aes_sw_cpb, aes_hw_cpb | 428.5 software (CBC), 9.40 hardware ECB, 9.42 hardware CBC | derived from the 2026-10-09 runs (identical on three flashes) |
| control_loop_worst_pct, jitter | float 2,331 percent worst, 157 us jitter; IQmath 264 percent, 0.19 us | derived from the 2026-10-09 logs; -O3 float 180.8 us trigger to PWM, 2,325 percent |
| ML-KEM-768 keygen, encaps, decaps (-Os) | 1.995, 2.383, 2.910 M cycles (24.9, 29.8, 36.4 ms) | NIST ACVP known answers pass first; stack peaks 14.1, 17.2, 18.2 kB |
| ML-DSA-44 | does not fit, short by 4.8, 25.6 and 6.6 kB | an image linked for this chip with a 51.8 kB stack fails to link |
| SHA-256 1 kB | 162k cycles (158 per byte) portable C; 212k (207) TI library | no SHA hardware on this chip |
| ECDSA P-256 sign, verify; ECDH | micro-ecc asm 16.65 M, 18.65 M, 15.68 M cycles; TI library 33.4 M, 60.0 M; ECDH in TI's library fails (0x3C) | the same as the G3519 to within 0.01 percent |
| toolchain_gcc | 65 of 65 ran, 0 faults, 15 with the same changing output pins as the ticlang run | 98 of 98 GCC makefiles build; GCC text 1.07 times TI Arm Clang's; 33 skipped before the board was touched (18 low-power, 12 on PA10 and PA11, 4 risky) |

Port sweep (build only, SysConfig retarget with way 3): G5187 to G3507 builds 89 of 207 (43.0 percent; 33 clean, 56 with a changed setting), G3507 to G5187 84 of 270 (31.1), G5187 to G3519 101 of 207 (48.8), G3519 to G5187 141 of 258 (54.7). Moving up to the bigger sibling works more often than moving down, as on the G3519.

**Power.** Not measured on this board. The chip-only capture needs the BoosterPack and radio off the 3V3 rail first.

**Security port.** The pack is the LP-MSPM0G3519's (`portability/boards/mspm0g3519/`); this board's changes are in `portability/boards/mspm0g5187/PORT.md`: one SRAM bank, ML-DSA not built, the join buffers dropped at link, a 3 kB RAM log, -Os for ML-KEM.

**GCC run.** GCC ran `sysctl_hfxt_run` and `sysctl_mclk_syspll`, two of the seven examples that hang in the clock-status wait under TI Arm Clang.
