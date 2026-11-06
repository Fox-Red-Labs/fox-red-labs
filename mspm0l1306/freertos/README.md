# FreeRTOS on the TI MSPM0L1306: what fits in 4 kB of RAM and what it costs

**Short answer.** Yes. The FreeRTOS kernel that ships in TI's MSPM0 SDK (V11.2.0) runs on the MSPM0L1306 with static allocation: a one-task blink uses 3,784 bytes (3.7 kB) of flash and 1,008 bytes (1.0 kB) of RAM, 11 minimal tasks fit (192-byte stacks), and on the board a task switch round trip takes 397 to 499 cycles (12 to 16 microseconds at 32 MHz), an interrupt wakes a waiting task in 511 to 645 cycles. TI ships no FreeRTOS example for this chip, and TI's own FreeRTOS settings file for this board asks for 5,540 bytes (5.4 kB) of RAM, more than the chip has.

Terms: [docs/vocab.md](../../docs/vocab.md) (RTOS, stack, heap, SysTick, linker). Errors and fixes for this board: [FAQ](../FAQ.md). 1 kB = 1,024 bytes.

Built 2026-10-06 on an Intel Mac, macOS 15.7.9: MSPM0 SDK 2.11.00.07 (FreeRTOS Kernel V11.2.0 inside it, `kernel/freertos/Source`, read only), TI Arm Clang 5.1.1.LTS from CCStudio 21.0.1, -O2. Clock setup is the SysConfig output already in `../bench/coremark/syscfg32/` (TI's `gpio_toggle_output` example: SYSOSC 32 MHz).

Contents: [Words used](#words-used-below), [Sizes](#sizes), [Where the RAM goes](#where-the-ram-goes), [How many tasks fit](#how-many-tasks-fit), [Timing](#timing), [What was changed](#what-was-changed-from-tis-setup), [Build](#build), [Check on the board](#check-on-the-board), [Notes](#notes).

## Words used below

- **Task:** one function the RTOS runs as if it had the CPU to itself. Each task has its own stack and a 64-byte task block (FreeRTOS calls it the TCB).
- **Idle task:** the task FreeRTOS creates itself; it runs when nothing else can.
- **Tick:** the RTOS's regular timer interrupt, here SysTick at 1,000 per second. `vTaskDelay(500)` sleeps 500 ticks.
- **Context switch:** saving one task's registers and loading another's.
- **Semaphore, queue, task notification:** ways for one task (or an interrupt) to wake another. A queue also carries data. A notification is the lightest: no separate object.
- **Static allocation:** every task block, stack and queue is a fixed array the linker places. The alternative is a heap: one big array the RTOS hands out at run time.
- **High-water mark:** how much of a task's stack was never touched. FreeRTOS fills each stack with a pattern and counts what is left.
- **Interrupt stack:** the stack in the linker's `.stack` section. Before the scheduler starts, `main()` runs on it; after, interrupts do.

## Sizes

From the linker map of each image.

| Image | What it runs | Flash, bytes | RAM used, bytes | RAM left, bytes |
|---|---|--:|--:|--:|
| `blink` | 1 task + idle | 3,784 | 1,008 | 3,088 |
| `bench` | 3 tasks + idle, a semaphore, 2 queues, 1 interrupt | 13,016 | 2,178 | 1,918 |
| `bench-lean` | the same, no `configASSERT`, no stack overflow check | 12,088 | 2,178 | 1,918 |
| `edge-static` | 11 minimal tasks + checker + idle, all static | 4,504 | 3,956 | 140 |
| `edge-heap` | the largest heap that links, 3,424 bytes (3.3 kB); tasks made at run time | 4,712 | 4,092 | 4 |
| `ti-config` | `blink` with TI's own L1306 FreeRTOS settings file, unchanged | does not link | 5,540 (`.bss`) + 256 (stack) | none |

The kernel and port alone: 2,718 bytes (2.7 kB) of flash in `blink` (tasks and delays), 5,726 in `bench` (adds queues, semaphores, notifications and the interrupt calls), 4,872 in `bench-lean`. The asserts and the overflow check cost 928 bytes of flash in the whole `bench` image and no RAM. Flash is never the limit: the largest image uses 20% of it.

For scale: TI's bare-metal blink example on this chip is 440 bytes of flash and 256 bytes of RAM, all of it stack ([`../README.md`](../README.md)). FreeRTOS adds 3,344 bytes (3.3 kB) of flash and 752 bytes of RAM to a blink: 456 for the kernel and its idle task, 256 for the blink task, 40 for this page's result and fault records.

## Where the RAM goes

From the linker maps of `blink` and `bench` (every RAM symbol with its size).

| Item | Bytes | Notes |
|---|--:|---|
| Kernel lists and counters | 200 | 3 priority levels at 20 bytes each, 4 more lists, 15 pointers and counters |
| Idle task | 256 | 64-byte task block + 48-word (192-byte) stack |
| Each task you add | 64 + its stack | a minimal task here: 64 + 192 = 256 |
| Each task from the heap instead | 64 + stack + 16 | heap_4 puts an 8-byte header on the block and on the stack |
| Each semaphore or queue | 72 | plus the queue's item storage |
| Interrupt stack (`.stack`) | 256 | TI's value in its FreeRTOS examples; bare-metal examples use the same |

Part of the interrupt stack is never given back. The Cortex-M0+ port in the SDK does not reset the main stack pointer when the scheduler starts (its comment: the vector table offset register is optional on the M0+), so the frames `main()` used to start the scheduler stay on it: 100 to 132 bytes in these images, from the stack analysis (`stackdepth.py`). It is FreeRTOS's port design, documented in `portasm.c`.

## How many tasks fit

A minimal task here writes a flag and sleeps one tick, forever (`edge.c`). Its stack is 48 words (192 bytes). The compiler's stack-usage files and the call graph (`stackdepth.py`) put its worst case at 44 words: an 88-byte call chain (`vTaskDelay` > `xTaskResumeAll` > `xTaskIncrementTick`), 72 bytes for one saved context, and the 16 bytes the overflow check needs untouched. The idle task needs 22 words but gets the same 48, because FreeRTOS sizes it from the same setting.

| Memory | Minimal tasks | RAM per task | How it was found | Status |
|---|--:|--:|---|---|
| Static, 48-word stacks | **11** (13 tasks with checker and idle) | 257 bytes (block, stack, 1 flag byte) | 11 links; 12 gives `program will not fit into available memory` | ran on the board: 11 made, 11 alive, no fault |
| Heap (heap_4 at 3,424 bytes (3.3 kB)), 48-word stacks | **10 expected** | 272 bytes from the heap | arithmetic from heap_4's rules: 3,416 usable, idle 272, checker 368, then 10 × 272 leaves 56 | ran on the board: 10 made, the 11th failed for lack of heap, 56 bytes free, as predicted |
| Static, TI's 128-word stacks (`MIN_STACK=128`) | **4** (6 tasks with checker and idle) | 577 bytes | 4 links; 5 does not | linked, not run |

Static allocation fits one more task than the heap here, mostly because heap_4 adds 16 bytes per task (two 8-byte headers). The heap image also keeps a 96-byte table of task handles, which static tasks do not need: a static task's handle is its block's address. Static also moves the failure to build time: the linker refuses a design that does not fit, where `xTaskCreate` would fail on the board.

## Timing

FreeRTOS uses SysTick for its tick, so the cycle counter is TIMG0, a 16-bit timer clocked from the 32 MHz bus clock, divide 1: one count is one CPU cycle (31.25 ns), and a reading wraps after 2.048 ms, far above any test here. Before the scheduler starts, `bench` times one delay with both TIMG0 and SysTick to prove TIMG0 runs at the CPU clock. Each test: 8 warm-up passes, then 1,000 measured; best, worst, worst with no tick interrupt inside, and average. Values are raw and each includes one TIMG0 read, measured as `timer_read`.

| Test (cycles at 32 MHz) | `bench` best / worst / average | `bench-lean` best / worst / average |
|---|---|---|
| Task switch round trip: `taskYIELD` to an equal-priority task that yields straight back (2 switches) | 499 / 640 / 502.9 | 397 / 537 / 400.6 |
| `xSemaphoreGive` to the first line of the higher-priority task waiting on it | 615 / 755 / 618.8 | 527 / 652 / 529.5 |
| `xTaskNotifyGive` to the first line of the higher-priority task waiting on it | 556 / 697 / 557.8 | 469 / 609 / 471.1 |
| Queue round trip: send to a higher-priority task, it sends back, receive returns | 2,380 / 2,650 / 2,396.9 | 2,155 / 2,424 / 2,171.4 |
| Interrupt entry: pend I2C1 (line 25) to the handler's first line | 38 / 39 / 39.0 | 39 / 39 / 39.0 |
| Interrupt to task: pend to the first line of the task woken by `xSemaphoreGiveFromISR` | 645 / 789 / 651.2 | 559 / 685 / 564.2 |
| Interrupt to task: pend to the first line of the task woken by `vTaskNotifyGiveFromISR` | 583 / 724 / 585.7 | 511 / 646 / 513.3 |

Measured 2026-10-06 on the board (probe <probe serial>), 1k samples per test, every pass checked: zero errors, no fault, every stack kept bytes unused (`check.py` prints ALL OK for all five images). The worst cases include a tick interrupt landing inside the measurement; the "worst, no tick" column in `check.py`'s output leaves those out and is within 6 cycles of the best. For comparison: interrupt entry here (38 to 39 cycles, with a 3-cycle timer read) matches bare metal (25 to 28 with a SysTick read pair); a Zephyr thread yield round trip on this chip is 431 to 491 cycles, against FreeRTOS's 397 to 499 ([`../zephyr/README.md`](../zephyr/README.md)). The two use different timers and builds, so this is not a one-change comparison.

## What was changed from TI's setup

Nothing in the SDK was changed, and the FreeRTOS kernel and port are TI's files compiled as they are: `tasks.c`, `list.c`, `queue.c`, `portable/TI_ARM_CLANG/ARM_CM0/port.c` and `portasm.c`, plus `portable/MemMang/heap_4.c` for the heap image. TI ships a FreeRTOS settings file for this board (`kernel/freertos/builds/LP_MSPM0L1306/release/FreeRTOSConfig.h`) but no example that uses it; the closest examples are for the L1117 and L2228, whose files are the same apart from writing the heap as 3 × 1,024 instead of 6 × 512 (3,072 bytes (3.0 kB) either way). This folder's `FreeRTOSConfig.h` starts from TI's file and changes:

| Setting | TI's file | Here | Why |
|---|---|---|---|
| Allocation | static and heap, 3,072-byte heap | static only, no heap (`edge-heap`: heap only) | the heap is 75% of RAM before any task; static lets the linker count every byte |
| `configMINIMAL_STACK_SIZE` | 128 words (512 bytes) | 48 words (192 bytes) | stack analysis: 44 words worst case for a task that sleeps |
| `configMAX_PRIORITIES` | 10 | 3 | 20 bytes of RAM per level |
| `configMAX_TASK_NAME_LEN` | 16 | 4 | 12 bytes per task block |
| Mutexes, trace facility, task tags, thread-local pointers | on | off | none is needed here; with the name length, the task block drops from 104 to 64 bytes |
| Software timers, event groups, stream buffers | on | off | timers add a timer task and its queue |
| Tickless idle | on | off | keeps the tick regular for timing; costs sleep power, not measured |
| High-water mark and idle handle calls | off | on | needed for the stack reports |
| Idle task memory | TI's `dpl/StaticAllocs_freertos.c` supplies it | `configKERNEL_PROVIDED_STATIC_MEMORY` | the kernel supplies it; no TI DPL code needed |
| `configASSERT`, stack overflow check method 2 | on | on (`bench-lean`: off) | a failure leaves a code in `fr_fault` instead of a silent loop |

Kept from TI's file: 32 MHz, 1,000 Hz tick, preemption on, time slicing off, kernel interrupts at the lowest priority. Task block and queue object sizes: 104 and 80 bytes with TI's file, 64 and 72 here.

TI's FreeRTOS example makefiles link a kernel library from a CCStudio workspace and point at a kernel makefile that does not exist ([`../../portability/README.md`](../../portability/README.md)). This folder does not use them: `build.sh` compiles the five kernel files straight into each image.

**TI's own settings file for this board does not fit this board.** `./build.sh ti-config` builds the same blink with TI's `LP_MSPM0L1306/release/FreeRTOSConfig.h` unchanged, plus the files TI's kernel project compiles for it (`timers.c`, `heap_4.c`, TI's `dpl/StaticAllocs_freertos.c`). The blink skips its stack report there, because TI's file turns those calls off. The link stops:

```text
"./device_linker.cmd", line 62: error: program will not fit into available
   memory, or the section contains a call site that requires a trampoline that
   can't be generated for this section, or the section contains padded
   functions. run placement with alignment fails for section ".bss" size
   0x15a4.  Available memory ranges:
   SRAM         size: 0x1000       unused: 0xeab        max hole: 0xeab
```

0x15a4 is 5,540 bytes (5.4 kB) of zero-filled RAM on a chip with 4,096, before the 256-byte interrupt stack. The largest items, from the object files: the 3,072-byte heap, the idle and timer tasks' 512-byte stacks (static arrays in TI's `StaticAllocs_freertos.c`), the blink task's 512-byte stack, three 104-byte task blocks and 200 bytes of ready lists for 10 priorities. Without the blink task it would still need about 4,900 bytes (4.8 kB) (arithmetic from the same list). No L1306 example uses the file, so nothing in the SDK exercises it. This folder's `FreeRTOSConfig.h` is one way to make it fit.

## Build

From this folder (`mspm0l1306/freertos`), one command per image:

```bash
./build.sh blink
```

```bash
./build.sh bench
```

```bash
./build.sh bench-lean
```

```bash
./build.sh edge-static
```

```bash
./build.sh edge-heap
```

`./build.sh all` builds the five. `./build.sh ti-config` repeats the link with TI's settings file (it is meant to fail; the log is written to `build-ti-config/link.log`). Each writes `build-<name>/<name>.out` and `.map`, `sizes.txt`, `ram-symbols.txt` and `stack.txt`, and stops if any part of the image falls outside main flash (0x0 to 0x10000) or SRAM (0x20000000 to 0x20001000); nothing is written to the boot configuration area. `MIN_STACK=<words>` changes the minimal stack and builds into a folder with `-stack<words>` on its name. The kernel is compiled once per setting into `build-kernel/`. On a lightly loaded Mac each image takes under a minute.

## Check on the board

Each command flashes the image, fills RAM with 0xA5 before any code runs, lets it run, then reads the results and dumps all of RAM to `build-<name>/sram.bin` ([`../tools/paintrun.sh`](../tools/paintrun.sh)). `check.py` then decodes the dump and checks every value. Run from this folder:

```bash
../tools/paintrun.sh build-blink/blink.out build-blink/blink.map fr_blink 6 5000
```

```bash
python3 check.py build-blink
```

| Image | `paintrun.sh` arguments after the map | Run | Result symbol (words) | Fault record | Must see |
|---|---|--:|---|---|---|
| `blink` | `fr_blink 6 5000` | 5 s | `fr_blink` 0x2000023c (6) | `fr_fault` 0x200002a4 | LED1 (red) on 0.5 s, off 0.5 s. `magic` 0xB1E00001, `ticks` = 500 × (`toggles` − 1) |
| `bench` | `fr_bench 43 3000` | 3 s | `fr_bench` 0x20000400 (43) | `fr_fault` 0x20000710 | `magic` 0xBE4C0001, `samples` 1,000, `timg_counts` within 4 of `systick_cycles`, `errors` 0 |
| `bench-lean` | `fr_bench 43 3000` | 3 s | `fr_bench` 0x20000400 (43) | `fr_fault` 0x20000710 | as `bench` |
| `edge-static` | `fr_edge 12 5000` | 5 s | `fr_edge` 0x20000d9c (12) | `fr_fault` 0x20000e1c | `magic` 0xED6E0001, `made` 11, `alive` 11 |
| `edge-heap` | `fr_edge 12 5000` | 5 s | `fr_edge` 0x20000dfc (12) | `fr_fault` 0x20000e94 | `magic` 0xED6E0001, `alive` = `made` (10 expected), `ran_out` 1, `heap_free` 56 expected |

In every image: `fr_fault.code` 0 (0xDEAD0001 is a stack overflow with the task name, 0xDEAD0002 a failed assert with its kernel line, 0xDEAD0003 a hard fault with its address), and every high-water mark above 4 words. `paintrun.sh` also prints how deep the interrupt stack went; the stack analysis says at most 184 bytes (`blink`), 224 (`bench`: 116 held by `main()`, 36 for the interrupt frame, 72 for the I2C1 handler; 300 if an interrupt could nest on the tick, which this program never causes), 208 (`bench-lean`), 208 (`edge-static`) and 176 (`edge-heap`), all under the 256 reserved. The bench finishes its tests in well under a second (an estimate; `ticks_end` gives the real time in milliseconds). Addresses come from `build-*/ram-symbols.txt`; they change if an image is rebuilt with other settings, and `paintrun.sh` and `check.py` look them up by name.

## Notes

- **No FreeRTOS example for this chip.** TI ships FreeRTOS examples for the G3507, G3519, L1117 and L2228, and a kernel settings file for the L1306, but no L1306 example.
- **Stack sizes come from analysis, then the board.** `stackdepth.py` assumes an interrupt can land at the deepest point of every call chain, which overstates: the deepest kernel paths run with interrupts off. The high-water marks from the board are the measured answer.
- **The bench interrupt is I2C1, line 25.** This chip's interrupt controller has only the 13 lines that have a peripheral; pending any other line does nothing ([`../README.md`](../README.md)).
- **No UART.** Results stay in RAM, as in the rest of the bench, so printing costs nothing in RAM or timing.

## Files

`FreeRTOSConfig.h` (the settings), `fr_common.c` and `.h` (LED1 setup, fault records), `blink.c`, `bench.c`, `edge.c`, `build.sh`, `mapsize.py` (flash and RAM from a map), `stackdepth.py` (worst-case stack per task), `check.py` (decode and check the board results), `build-*/` (images, maps, reports).
