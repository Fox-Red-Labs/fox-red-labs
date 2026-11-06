# FreeRTOS on the LP-MSPM0C1104 with 1 kB of RAM

**Short answer.** Barely. TI's FreeRTOS kernel (V11.2.0, from MSPM0 SDK 2.11.00.07) runs a one-task blink in 4.1 kB of flash and 0.98 kB of the 1 kB of RAM, the same 0.98 kB as on the LP-MSPM0L1306. A second task does not fit with the safety checks on. With them off, two tasks plus idle ran (992 B of RAM). A task switch round trip takes 478 cycles (19.9 us at 24 MHz) with checks on and 392 (16.3 us) with them off.

Terms: [docs/vocab.md](../../docs/vocab.md) (RTOS, tick, stack, SysTick, context switch, TIMG14). 1 kB = 1,024 bytes. Measured 2026-10-09, TI Arm Clang 5.1.1.LTS, -O2, board LP-MSPM0C1104 on probe MC110001, 24 MHz.

## What had to change from the LP-MSPM0L1306 port

The chip has no working SysTick, and FreeRTOS's tick is normally SysTick. `fr_common.c` replaces the port's weak `vPortSetupTimerInterrupt` with TIMG14 on the CPU clock counting 24,000 per tick, and its interrupt handler calls the port's tick function. The blink image counted 3,000 ticks in 3.0 s, so the tick runs at 1,000 per second. Cycles are counted with TIMG8, 16 bits on the CPU clock (checked: 20,010 counts across `delay_cycles(20000)`).

## Sizes

| Image | What it runs | Flash | RAM, B | Result |
|---|---|--:|--:|---|
| `blink-48-256` | 1 task + idle, 48-word stacks, 256 B interrupt stack, checks on | 4.1 kB | 1,008 | runs, 6 toggles in 3 s |
| `blink` | same, 32-word stacks, 128 B interrupt stack | 4.1 kB | 752 | runs |
| `bench` | 1 task + idle, timing test, checks on | 6.2 kB | 918 | runs |
| `bench-lean` | same, 40-word stacks, checks off | 5.7 kB | 854 | runs |
| `two-36-128-lean` | 2 tasks + idle, 36-word stacks, checks off | 3.8 kB | 992 | runs, 7 toggles in 3.3 s |
| `two-38-128` | 2 tasks + idle, 38-word stacks, checks on | 4.2 kB | 1,016 | links; stops after 2 toggles, no fault recorded, cause not found |
| `two-48-128`, `two-44-128`, `two-40-128` | 2 tasks + idle, checks on | | | do not link |
| `edge` (1 task + a checker task) | the L1306's task-count test | | | does not link at 48 or 40 words |

RAM is data plus bss from the linker map; the interrupt stack counts. TI's own FreeRTOS settings file for the L1306 needs 5.4 kB of RAM and cannot fit here either.

## Timing

Best, worst and average of 1,000 passes after 8 warm-up passes, three flashes of each image with identical best figures. Values include one TIMG8 read (3 cycles).

| Test (cycles at 24 MHz) | `bench` best / worst / average | `bench-lean` best / worst / average |
|---|---|---|
| Task yield round trip: the bench task (priority 0) yields to the idle task, which does one pass of its loop and yields back | 478 / 592 to 607 / 480.8 to 481.1 | 392 / 521 / 394.3 |
| Interrupt to task: a one-shot TIMA0 interrupt notifies the waiting task; from the handler's first line to the task's first line | 491 / 674 / 549.9 | 460 / 585 / 463.0 |

The L1306 measured a yield to an equal-priority task (397 to 499 cycles at 32 MHz). Only one application task fits here, so this test yields to the idle task and includes one idle-loop pass. Do not compare the two one for one. Stack left over: the bench task had 17 of 48 words unused, the idle task 29 of 48 (checks on).

## Build and run

```bash
./build.sh <name> <app.c> [min stack words] [interrupt stack bytes] [-D flags]
./build.sh bench bench.c 48 128
./build.sh bench-lean bench.c 40 128 -DFR_SAFETY=0
```

`build.sh` has two folder variables near the top, `GEN` (the SysConfig output of TI's `gpio_toggle_output`: `ti_msp_dl_config.c` and the device linker files) and the output folder `O`. Point both at folders on your machine before you run it. The images were flashed with OpenOCD and the results struct (`fr_bench`, `fr_blink`) read from RAM after 3 to 4 s. Files: `FreeRTOSConfig.h`, `fr_common.c` (LED PA22, tick timer, fault recorder), `blink.c`, `bench.c`, `edge.c`, `mapsize.py`, `stackdepth.py`.
