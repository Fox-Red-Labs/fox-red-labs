# Board table: clock, timer, LED, UART and ADC pin for the shared bench tests

Terms: [docs/vocab.md](../../docs/vocab.md). 1 kB = 1,024 bytes.

One row per board in `boards.csv`: CPU clock, the clock the trigger and PWM timers count at (they can be slower than the CPU: on the CC2755P10 the CPU runs at 96 MHz and the LGPT timers at 48 MHz), the cycle counter, LED, UART and ADC pin. An empty cell means nobody has checked it for that board; a test that needs it fails to compile instead of guessing.

`gen_board_h.py <board> <out.h>` writes `bench_board.h` with `BB_CPU_HZ`, `BB_TIMER_HZ`, `BB_TIMER_DIV` (CPU clock over timer clock) and the pins as strings. The build script of the SimpleLink boards runs it for every image, so `cc2340r5`, `cc1354p10` and `cc2755p10` read their clock from here. The shared tests include `bench_board.h` and carry no clock of their own. The check compares `cpu_mhz` with `data/boards.csv`:

    python3 portability/boards/gen_board_h.py --check

To add a board: add the row, fill the cells you have checked, and point its build at `gen_board_h.py`.

## Test rows this table unlocks

The table supplies the clock, timer clock, cycle counter and pins. A test row still needs a build recipe for the board's toolchain, so the table alone finishes few rows. Checked 2026-10-09 by building only, nothing flashed: the FOC image builds from the table on the `cc2755p10` (CPU 96 MHz, timers 48 MHz), `cc2340r5` and `cc1354p10`.

| Board | Test rows the table now covers, with what is still missing |
|---|---|
| cc2755p10 | `control_loop`, `isr_entry`: the fixed FOC image builds; the rows stay `failed` until a board run flashes it. `rtos_task_switch`, `rtos_max_tasks`: clock and counter are in the table; the FreeRTOS port path and board syscfg for `tests/rtos` are not |
| cc2340r5, cc1354p10 | no test row depends on the clock; theirs are radio examples |
| mspm0g5187 | `dhrystone`, `aes_speedup`, `crc_speedup`, `embench_fit`, `fft256`, `control_loop`, `foc_step`, `isr_entry`: the clock (80 MHz) and counter are known; each needs the G5187 SDK build recipe and the MSPM0L1306 bench code retargeted |
| mspm0g3519 | `aes_speedup`, `crc_speedup`, `embench_fit`, `fft256`: same, 80 MHz |
| mspm0g3507, mspm0l1306 | `fft256` (G3507), power images (L1306): code exists on the sibling board |
| nucleo-u083rc | `control_loop_*` figures of merit: `fom_tb.h` now takes `FOM_TIMER_DIV`; the STM32 timer clock needs a `timer_mhz` cell and a checked value |
| am13e230, mspm33c321a, cc35x1 | clock known; the PWM-triggered ADC harness and Embench ports are unwritten code, not table entries |

To add a board to any test row above, add the board's `timer_mhz`, `led`, `uart` and `adc` cells once checked, then include `bench_board.h` from the test.
