<!-- experiment
id: E8
board: LP-MSPM0L1306
date: 2026-10-06
status: works
reached: verified
line: Runs with static allocation and smaller stacks: blink uses 1 kB of 4 kB of RAM and 11 small tasks fit; TI's own settings file for this board needs 5.4 kB and does not link.
stop: ram
-->
# Can FreeRTOS run on the TI LP-MSPM0L1306, with only 4 kB of RAM and no TI example?

**Short answer.** Yes. TI's FreeRTOS kernel (V11.2.0, in the MSPM0 SDK) runs on the board with static allocation and 192-byte task stacks: a one-task blink uses 1 kB of RAM, 11 small tasks fit, and a task switch takes 12 to 16 microseconds. TI's own FreeRTOS settings file for this board does not fit it.

Terms: [vocab](../vocab.md) (RTOS, heap, stack, linker, `.bss`). Errors and fixes: [LP-MSPM0L1306 FAQ](../../mspm0l1306/FAQ.md). 1 kB = 1,024 bytes.

**Question.** TI ships FreeRTOS examples for the MSPM0G3507, G3519, L1117 and L2228, but none for the L1306. Does FreeRTOS fit and run in 4 kB of RAM, and what does it cost?

**Vendor position.** MSPM0 SDK 2.11.00.07 (read 2026-10-06): `examples/rtos/` has no `LP_MSPM0L1306` folder, but `kernel/freertos/builds/LP_MSPM0L1306/release/FreeRTOSConfig.h` exists. That file sets a 3 kB heap (`configTOTAL_HEAP_SIZE`, 6 × 512) and 128-word (512-byte) stacks for the idle and timer tasks. No document found says FreeRTOS is or is not supported on the L1306.

**What was tried.** Intel Mac, macOS 15.7.9, TI Arm Clang 5.1.1.LTS, -O2, probe <probe serial>. TI's kernel and Cortex-M0+ port files compiled unchanged; DriverLib for the LEDs and a TIMG0 timer as the cycle counter (FreeRTOS takes SysTick for its tick). Five images: a blink, a timing benchmark with and without FreeRTOS's checks, and two "how many tasks fit" images (static, and heap_4). The settings: static allocation only, 48-word (192-byte) task stacks, no timer task. Everything is in `mspm0l1306/freertos/` with `build.sh` and `check.py`.

**How far it got.**

- Link with TI's settings file, unchanged: fails, the zero-filled data needs 5.4 kB on a 4 kB chip. The file's heap alone is three quarters of RAM.
- Link with this repo's settings: blink 3.7 kB of flash and 1 kB of RAM; timing benchmark 12.7 kB and 2.1 kB.
- Run: all five images passed every check `check.py` makes (no stack overflow, no failed assert, no fault; every task's stack kept bytes unused).
- Measured, 1k samples each, cycles at 32 MHz with checks on: task switch round trip 499 best, semaphore to a waiting task 615, interrupt to a waiting task 645, queue send and reply 2.4k. With the checks off: 397, 527, 559 and 2.2k.
- How many fit: 11 small tasks with static allocation, ran with all 11 alive; 10 with a heap, where the 11th create failed for lack of heap exactly as the arithmetic predicted. With TI's 128-word stacks, only 4 would fit (linked, not run).

**The line.** Not reached for small programs. **Unknown (suspected: Chip limit).** At about 257 bytes per minimal task, RAM caps the task count near 11; real tasks with bigger stacks will fit fewer. Only this repo's settings and TI's were tried.

- A real sensor task set (ADC, filter, UART) under FreeRTOS, with its stack use measured.
- FreeRTOS's tickless idle for low power, with a meter.
- The same benchmark on the MSPM0G3507 with TI's FreeRTOS example as the baseline.

**Ask.** Has anyone run FreeRTOS on a 4 kB MSPM0 with a heap and stacks smaller than TI's settings file for the L1306, and what task count did it reach?
