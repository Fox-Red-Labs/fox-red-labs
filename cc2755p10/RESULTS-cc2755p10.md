# LP-EM-CC2755P10 benchmark results: control loop, AES, SHA-256, ECC, post-quantum crypto (2026-10-17)

Terms: [docs/vocab.md](../docs/vocab.md). Errors and fixes: [FAQ.md](FAQ.md). Every number is a row in `data/results/cc2755p10.csv`. 1 kB = 1,024 bytes.

The Cortex-M33 at 96 MHz holds a 90.2 to 93.75 kHz control loop with no missed period, runs ML-KEM-768 decapsulation in 1.35M cycles (14.1 ms) and ML-DSA-44 signing in 4.77M cycles (49.7 ms), and its hardware security unit signs with ECDSA P-256 3 to 5 times faster than the best software. Setup: F3 SDK 9.21.00.36, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, code in flash. The board was backed up before the first flash (flash 0 to 0xE8000 and CCFG read twice, identical; SCFG read once and equal to the SDK default). At the end `cc2755p10/restore.sh` flashed the backup and read it back identical.

Every number is a cycle count taken on the chip, so host load does not change it. Flash loads need a quiet host (see "If you see this").

## Results

Each row gives what was expected before the run, where there was an expectation, and what came out.

| Test | Expected | Result |
|---|---|---|
| Control loop, highest rate with no missed period in 1,000 (three runs each) | -O2 90 kHz (3 percent either way), -O3 within 3 percent of -O2, -Os 5 to 10 percent below, from the 2026-10-09 -O2 runs | -O2 90.2 kHz, -O3 93.75 kHz, -Os 90.9 kHz. -O3 is 4 percent above -O2 and -Os is level with -O2, both better than expected |
| Control loop, CPU load | about 85 percent of a 100 kHz period | 76.5 to 76.6 percent, measured at about 45 kHz (half the highest rate, because the chip cannot hold 100 kHz); worst with checks 98 to 101 percent. The older figure did not correct for the handler's checks and the 16 cycles after the last stamp |
| Control loop, worst trigger-to-new-value and jitter | 129 percent of a 100 kHz period (1.24k cycles) | 129 percent and 3.8 µs, all three builds within 1 percent of each other |
| AES: software CBC, hardware ECB (derived from the 2026-10-09 captures, three runs, not reflashed) | none | software CBC 233k cycles at -O2 for 1 kB; hardware ECB 2.40 cycles per byte for 1 kB (driver, key set each call) and 40 cycles for one 16-byte block |
| ML-KEM-768 keygen, encaps, decaps (mlkem-native 2.0.0, -O3, 96 MHz) | 1.0 to 1.4 times the CC1354P10's cycles (997k, 1.15M, 1.40M at 48 MHz with flash wait states) | 961k, 1.11M, 1.35M cycles (10.0, 11.6, 14.1 ms), which is 0.96 to 0.97 times. The 96 MHz part did not pay for flash wait states. At -Os: 993k, 1.15M, 1.40M, so -Os costs only 3 percent |
| ML-DSA-44 keygen, sign (one fixed message), verify (mldsa-native 2.0.0, -O3) | 1.0 to 1.4 times the CC1354P10's (2.37M, 4.96M, 2.42M) | 2.29M, 4.77M, 2.33M cycles (23.9, 49.7, 24.3 ms). Signing over 16 messages: mean 7.12M, range 3.56M to 16.7M. At -Os: 2.25M, 4.54M, 2.27M |
| Post-quantum stack peak | same as the CC1354P10 within 1 kB | ML-KEM 14.7, 20.0, 18.7 kB; ML-DSA 32.0, 50.1, 30.2 kB. Matches |
| Post-quantum flash, -O3 and -Os | about 80 and 109 kB (-O3), 9.4 and 13.6 kB (-Os) | ML-KEM 80.5 kB and ML-DSA 106.7 kB at -O3; 9.4 kB and 13.3 kB at -Os |
| SHA-256 | none | 53.1 cycles per byte (B-Con C); 9.1 through TI's SHA2 driver (HSM, includes the call) |
| ECDSA P-256 sign, verify, ECDH shared | none | micro-ecc C 11.1M, 12.6M, 10.5M cycles; micro-ecc Thumb-2 assembly 7.79M, 9.04M, 7.31M; TI HSM drivers 2.33M, 4.01M, 2.18M (24, 42, 23 ms). The HSM signature has no known-answer vector; it is checked by the HSM's own verify and a one-bit-flip rejection |
| OpenSSL `speed` | none | Does not fit: no operating system |
| Power-loop family | none | Not run. The shared program has no CC27xx branch (96 MHz CPU, 48 MHz timer) |
| ADC and DAC error | none | Not applicable: the chip has no DAC |
| Zephyr (TI fork v3.7.0-ti-1.00.01_ga) | none | `hello_world` printed `Hello World! lp_em_cc2755p10/cc2755p10`; blinky toggled DIO12 (GPIO register read 8 times, both levels) |

Not run: radio pair tests, because the partner device was a house Zigbee light.

## The board's own image

The image on the board at the start was a Bluetooth image with the "Basic BLE Menu" strings, not the smoke-alarm Matter image. The Matter smoke alarm in Apple Home needs the Matter smoke image from [matter-smoke](matter-smoke/README.md); it was not on the board.

## If you see this

- **DSLite stops with `Target did not enter SACI after pin reset`.** It began during a period of Mac load 10 to 180 and kept failing at load 10. Powering the probe's USB hub port off and on cleared it. The cause is not confirmed.
