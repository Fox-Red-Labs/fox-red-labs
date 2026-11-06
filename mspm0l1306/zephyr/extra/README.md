# More Zephyr 4.4.2 on the TI LP-MSPM0L1306 LaunchPad: what else fits in 4 kB of RAM

Terms: [docs/vocab.md](../../../docs/vocab.md). 1 kB = 1,024 bytes. Built 2026-10-06 with Zephyr 4.4.2, GCC 12.2.0 from Zephyr SDK 0.17.4, on the out-of-tree board in `../boards/` (see [../README.md](../README.md)). Built and, unless a row says otherwise, run on the board 2026-10-06. The "What you should see" section is from Zephyr's source; where a table above it shows something different, the table wins.

## Short answer

Run on the board 2026-10-06 (probe <probe serial>), not just linked:

- **A Zephyr shell runs in 4 kB.** With TI's 32-pin chip and the board file, `shell_module` in its minimal setting answers `demo ping` with `pong`, with 48 bytes of RAM left.
- **All eight Thread-Metric tests run**, with stacks and data cut to fit (so their counts are not comparable with other chips' published Thread-Metric scores). Three 30-second periods each agreed within 0.05%.
- **Logging works in both modes**, and the synchronization sample runs. Deferred logging first crashed 4 ms after boot (`ZEPHYR FATAL ERROR 0: CPU exception`, in `log_output_flush`); a 768-byte log thread stack fixed it. A log thread stack of 512 bytes is too small; use 768.
- **latency_measure crashes after its first result.** It reports a `k_yield` context switch at 307 cycles (9.6 µs), then faults in `z_setup_new_thread` and reboots in a loop. Its test stacks were cut to 256 bytes to link; not traced further.
- Zephyr's minimal C library saves 24 bytes of RAM against picolibc and costs 140 bytes of flash.

## What was built

Every image needs `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=0` (without it the C library heap takes all leftover RAM and the image hard-faults at boot, see [../README.md](../README.md), point 8). Every image also sets `CONFIG_ISR_STACK_SIZE=512`, because Zephyr's default interrupt stack is 2,048 bytes (2.0 kB), half the chip. Those two are not repeated in the Changes column.

| Build | Fits | Flash (bytes) | RAM (bytes) | RAM left (bytes) | Changes |
|---|---|--:|--:|--:|---|
| Thread-Metric basic | yes, ran | 13,520 | 3,712 | 384 | Main stack 768. Reads a 256-entry array instead of 1,024 (the 1,024 array alone is all 4,096 bytes (4.0 kB)), so the count is not comparable with other chips |
| Thread-Metric cooperative | yes, ran | 14,124 | 3,984 | 112 | Five test stacks of 256 instead of 1,024; main stack 768 |
| Thread-Metric preemptive | yes, ran | 13,972 | 3,984 | 112 | Same as cooperative |
| Thread-Metric interrupt | yes, ran | 22,748 | 2,880 | 1,216 | Test stack 512. `CONFIG_TEST=y` (needed for `irq_offload`) |
| Thread-Metric interrupt preemption | yes, ran | 22,796 | 3,536 | 560 | Same as interrupt |
| Thread-Metric memory allocation | yes, ran | 14,080 | 2,976 | 1,120 | Test stack 512; one slab of two 128-byte blocks instead of four slabs of eight |
| Thread-Metric message | yes, ran | 14,340 | 2,888 | 1,208 | Test stack 512; one queue instead of four |
| Thread-Metric synchronization | yes, ran | 14,032 | 2,704 | 1,392 | Test stack 512 |
| latency_measure | links; crashes after the first result | 32,212 | 3,884 | 212 | Test stacks 256 (`CONFIG_TEST_EXTRA_STACK_SIZE=-768`), main stack 512, kernel heap 256 (was 2,048) |
| logger, deferred mode | links; crashed at boot (log thread stack 512) | 22,780 | 3,656 | 440 | Log buffer 256 (was 2,048), log thread stack 512, demo thread stack 512, main stack 512 |
| logger, immediate mode | yes, ran | 17,684 | 3,640 | 456 | Demo thread and main stack keep the sample's 1,024 bytes (1.0 kB); no log buffer, no log thread |
| logger, deferred mode, log thread stack 768 | yes, ran | | 3,912 | 184 | As the deferred row, log thread stack 768 (`-DCONFIG_LOG_PROCESS_THREAD_STACK_SIZE=768`) |
| logger, minimal mode | no | | | | The sample's own code uses `CONFIG_LOG_BUFFER_SIZE`, which does not exist in minimal mode: compile error |
| shell_module | yes, ran: `demo ping` gives `pong` | 22,048 | 4,048 | **48** | `prj_minimal.conf` (`CONFIG_SHELL_MINIMAL`), shell stack 640 (was 2,048), system work queue stack 512 (was 1,024), main stack 512, no history, 32-byte command line, no tab completion, no dynamic or foreground demo commands (a 660-byte table), no logging |
| synchronization | yes, ran | 10,852 | 3,136 | 960 | Both thread stacks 512 (the sample hard-codes 1,024, so a copy of its `main.c` is built with that number changed), main stack 512 |
| hello_world, Zephyr minimal libc | yes, ran | 9,564 | 3,808 | 288 | `CONFIG_MINIMAL_LIBC=y` |
| hello_world, picolibc (baseline from [../README.md](../README.md)) | yes | 9,424 | 3,832 | 264 | none |

"Left" is 4,096 minus RAM used. Thread-Metric tests have to share one build folder to be quick (the tests differ only in app code), which is why `extra/thread_metric/build_all.sh` exists.

Two places needed more than a config line, and both are in this folder, not in Zephyr: the Thread-Metric porting layer (`thread_metric/src/tm_porting_layer_l1306.c`) replaces Zephyr's, which reserves ten 1,024-byte stacks plus buffers (14 kB); and the synchronization and logger folders each build a copy of the sample's `main.c` with one hard-coded stack number changed. Zephyr's own test sources are read in place and are not edited.

 Zephyr's defaults assume more RAM; everything that fits does so by giving up Zephyr's default stack sizes. Stack overflow is silent on this chip (no memory protection unit): it corrupts the next stack down and the run still looks fine.

## Thread-Metric results

Counts per 30-second period, as Thread-Metric prints them (`Time Period Total`), three periods each, Zephyr 4.4.2, GCC, 32 MHz. Higher is better. Because stacks, the basic test's array, the slab and the queue were cut to fit 4 kB (table above), compare these only with each other or with the same builds on another board, not with published Thread-Metric tables.

| Test | Period 1 | Period 2 | Period 3 |
|---|--:|--:|--:|
| Basic processing (256-entry array) | 208,142 | 208,103 | 208,103 |
| Cooperative context switching | 3,326,961 | 3,326,410 | 3,326,362 |
| Preemptive context switching | 2,287,188 | 2,287,411 | 2,287,411 |
| Interrupt processing | 883,305 | 883,158 | 883,159 |
| Interrupt preemption | 627,140 | 627,164 | 627,164 |
| Memory allocation | 6,531,728 | 6,530,683 | 6,530,684 |
| Message processing | 1,799,745 | 1,799,452 | 1,799,453 |
| Synchronization | 7,935,244 | 7,933,778 | 7,933,778 |

## Images within 64 bytes of full RAM

shell_module: 48 bytes left. It has no way to print its own stack use. `CONFIG_INIT_STACKS=y` is on (the minimal shell has no `kernel stacks` command), so the stacks are painted with 0xAA and can be read with a debugger, which this project has not done. Treat a shell that hangs or prints garbage after typing a long line as a possible stack overflow, not as a driver fault.

Three more are close (112 bytes): Thread-Metric cooperative and preemptive. They print their own stack use every period (see below), so check those lines.

## Commands

Run from `mspm0l1306/zephyr/`. Set `OUT` to choose where builds go. Nothing here touches hardware.

```bash
export OUT=/some/folder
E=$PWD/extra

# Thread-Metric, all eight tests, one build folder, hex files at $OUT/tm-<test>.hex
extra/thread_metric/build_all.sh
extra/thread_metric/build_all.sh cooperative      # one test
TM_DURATION=10 extra/thread_metric/build_all.sh   # 10-second period instead of 30

# latency_measure
./build.sh custom latency tests/benchmarks/latency_measure -DEXTRA_CONF_FILE=$E/latency_measure/l1306.conf

# logger
./build.sh custom logger-deferred  $E/logger -DCONF_FILE="$E/logger/common.conf;$E/logger/deferred.conf"
./build.sh custom logger-immediate $E/logger -DLOG_STACK=1024 -DCONF_FILE="$E/logger/common.conf;$E/logger/immediate.conf"

# shell
./build.sh custom shell samples/subsys/shell/shell_module -DCONF_FILE=prj_minimal.conf -DEXTRA_CONF_FILE=$E/shell_module/l1306.conf

# synchronization
./build.sh custom sync $E/synchronization -DCONF_FILE=$E/synchronization/prj.conf

# hello_world with the minimal C library
./build.sh custom hello-minlibc samples/hello_world -DEXTRA_CONF_FILE=$E/hello_minimal/minimal_libc.conf
```

Hex files: `$OUT/<name>/zephyr/zephyr.hex` for `build.sh` builds, `$OUT/tm-<test>.hex` for Thread-Metric. `TM_DURATION` is a plain compile option (`-DTM_DURATION`), not Kconfig. Changing it rebuilds only the app.

## What you should see on UART0 (115200 8N1)

Every image starts with `*** Booting Zephyr OS build v4.4.2 ***` (shell_module turns the banner off).

**Thread-Metric (all eight).** Nothing prints for the first 30 seconds. Then each period prints, in this order: `S,` stack lines (`S,test_thread_N_unused_of_<size>,<bytes>`, `S,main_unused_of_768,...`, `S,isr_unused_of_512,...`, `S,idle_unused_of_...`), then `**** Thread-Metric <name> Test **** Relative Time: 30`, then `Time Period Total:  <count>`. Next period is at 60, then 90. Let it run three periods (90 seconds); the first period can include start-up. Cooperative also prints five per-thread counters; they should differ by at most 1. A line starting `ERROR:` means the test failed. Any `S,` value of 0 means a stack ran out and that run is not valid. The count is events per period: the larger, the faster. The basic test count is only for this chip (smaller array). Compare these counts only with the same builds on another board.

**latency_measure.** It prints the first result (`Context switch via k_yield : 307 cycles , 9620 ns`) and then faults in `z_setup_new_thread` and reboots in a loop.

**logger (deferred and immediate).** The sample prints these headings with `printk`, in order: `Module logging showcase.`, `Instance level logging showcase.`, `Severity levels showcase.`, `String logging showcase.`, `Logging performance showcase.` (ends with `Estimated logging capabilities: N messages/second`), `Logs from external logging system showcase.`. Between them are log lines like `[00:00:00.123,000] <inf> sample_module: ...`. In deferred mode the 256-byte buffer holds only a few messages, so expect `--- N messages dropped ---` lines; immediate mode drops nothing. If the output stops or the board resets partway, suspect a stack.

**shell_module.** The prompt `uart:~$`. Type `help`, `version` (prints `Zephyr version 4.4.2`), `demo ping` (`pong`), `demo board` (`lp_mspm0l1306`), `demo params 1 2 3`. The shell ran with interrupt-driven UART: `demo ping` returned `pong`.

**synchronization.** Two lines alternating about every 0.6 seconds: `thread_a_entry_point: Hello World from cpu 0 on lp_mspm0l1306!` then `thread_b_entry_point: ...` (thread names are off in this build, so the function name prints).

**hello_world (minimal libc).** `Hello World! lp_mspm0l1306/mspm0l1306`, same as the picolibc build.

## Where the change lives

`extra/thread_metric/` (CMakeLists, `prj.conf`, `irq.conf`, porting layer, `build_all.sh`), `extra/latency_measure/l1306.conf`, `extra/logger/` (CMakeLists, Kconfig, `common.conf`, `deferred.conf`, `immediate.conf`, `minimal.conf` which does not build), `extra/shell_module/l1306.conf`, `extra/synchronization/`, `extra/hello_minimal/minimal_libc.conf`.
