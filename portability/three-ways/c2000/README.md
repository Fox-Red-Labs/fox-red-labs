# The portable app on C2000Ware DriverLib (LAUNCHXL-F2800137)

The same 81-line `common/app.c` and `app.h` run unchanged on the LAUNCHXL-F2800137 with 35 glue lines, 29 configuration lines and 4.9 kB of code. The F2800137 is a different driver family from the MSPM0 DriverLib and TI Drivers rows: C2000Ware DriverLib, a 16-bit-char C28x core, no SysConfig in this build. Run 2026-10-25, C2000 compiler 25.11.1.LTS, C2000Ware 26.02.00.00 LTS, RAM image. Terms: [docs/vocab.md](../../../docs/vocab.md).

| Measure | Value |
|---|---|
| Shared lines | 81 (62 + 19 code lines), not edited |
| Glue lines | 35 (five calls and `main`) |
| Config lines | 29 (24 of board setup in `board_init`, 5 in `c28x_preinclude.h`) |
| Code and constants | 4.9 kB (2,474 + 32 + 26 words); RAM image, flash not written |
| RAM | 1.0 kB (8 + 10 + 512 stack words) |
| Clean build | 2.7 to 4.0 s |
| Hands-on | about 12 minutes |
| Serial check | `serial_check.py` PASS: 14 lines, 115200, gaps 0.999 to 1.000 s, adc 2030 to 2156 (pin A6 floating) |

The one thing app.c needed: `app.h` uses `uint8_t`, which the C28x `stdint.h` omits (a chip limit: the core has 16-bit chars). `c28x_preinclude.h` supplies it with `--preinclude` for `app.c` only, so the file is not edited. C2000Ware's own `hw_types.h` defines `uint8_t` too, so the glue file must not get the preinclude (a second `typedef` is an error). Smallest change to `app.c` if the preinclude is unwanted: include `<stdint.h>` plus a board `types.h`, or declare `i` and `n` as `uint16_t`. Not made.

`char` is 16 bits here: `app_format` fills `char line[40]` as 40 words and `uart_write` sends the low byte of each. The 4-sample average and the number printer work as written.

Run: `build.sh`, then `portability/three-ways/c2000/run_board.sh /dev/cu.usbmodemCL3710811 <log folder>`, which loads the RAM image through CCStudio scripting and records the serial line. The board's flash was read before and after: SHA-256 identical to the as-received image, and TI's demo is running again. The LED toggle (GPIO from `DEVICE_GPIO_PIN_LED1`) is in the code and was not seen by eye.
