<!-- experiment
id: E267
board: LP-EM-CC2340R5
date: 2026-10-24
status: works
reached: verified
line: The 11 TI Drivers examples that the Wi-Fi SDK (LP-EM-CC35X1) and the F3 SDK (LP-EM-CC2340R5) both ship run on the LP-EM-CC2340R5 with the Wi-Fi SDK app source and no edits: 7 show the behavior the source defines, 3 only their start-up, and the pwmled1 fade was not observed.
stop: none
-->
# Do the TI Drivers examples built from the CC35X1 (Wi-Fi SDK) source run on the LP-EM-CC2340R5?

**Short answer.** Yes. All 11 examples built from the Wi-Fi SDK `.c` files inside the F3 SDK project for the LP-EM-CC2340R5, flashed and ran with no fault. The image is byte-identical to the one built from TI's own F3 example for each of the 11, so TI's own behavior and the twin's are the same code. 7 showed the behavior the source defines, 3 (gpiointerrupt, spicontroller, spiperipheral) only started up, and the pwmled1 fade was not observed.

Terms: [vocab](../vocab.md). FAQ: [LP-EM-CC2340R5](../../cc2340r5/FAQ.md). The build side: 22 of 22 builds ok. Checked 2026-10-24 on one LP-EM-CC2340R5 with an LP-XDS110 probe, F3 SDK 9.21.00.36, Wi-Fi SDK 10.20.00.39, TI Arm Clang 5.1.1.LTS, SysConfig 1.28.0.

**Question.** An earlier check built the 11 shared examples and stopped there. Do they behave like TI's own on this chip?

**Setup.** The board held the house Zigbee light. Main flash (512 kB) and CCFG (2 kB) were read twice and compared equal before any write. Each hex was flashed with DSLite (no verify, 11 to 17 s), the chip was reset through the probe, and the back-channel serial port was held open at 115200 baud. Text was typed where the example asks for it. For LED-only examples the GPIO output register was read through the debugger. Afterwards the backup was flashed back and main flash and CCFG read back equal to the backup (0 differing bytes).

**Expectation.** Because baseline and swapped images are identical, any difference from TI's own example would point at the chip or the board, not the code. TI's source defines: ADC values printed, LED toggling, SHA-256 digest of typed text, echo, a wait for a missing I2C target, wiring notices for SPI.

**Result.**

| Example | Seen |
|---|---|
| adcsinglechannel | banner and raw and microvolt values for two ADC configs |
| empty | LED pin read high and low across 6 reads |
| exception | LED pin high; no serial, as in the source |
| gpiointerrupt | LED0 high at start; buttons not pressed |
| i2ccontroller | "Sending GET_STATUS until target responds"; no target fitted |
| pwmled1 | runs, no serial; fade not watched |
| sha2hash | "abc" gives BA7816BF...15AD, equal to sha256 on the Mac |
| spicontroller, spiperipheral | wiring notice and init line; no partner wired |
| uart2echo | echoes the 5 typed bytes |
| watchdog | LED pin both states across 6 reads over about a minute, no reset |

**The line.** The pin-read checks show an output changing, not its rate. Button, SPI, I2C and PWM behavior needs a hand, a partner board or a camera.

**Gotcha found.** `portability/everywhere/crossbuild_f3_wifi.sh` names SysConfig 1.26.3, which is no longer installed; with 1.28.0 the F3 9.21 projects build (22 builds, 11 to 40 s each).

**Ask.** A second LP-EM-CC2340R5 or a logic analyzer would close the SPI and PWM lines.
