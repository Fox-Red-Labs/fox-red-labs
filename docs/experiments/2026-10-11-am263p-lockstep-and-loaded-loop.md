<!-- experiment
id: E140
board: LP-AM263P
date: 2026-10-11
status: works
reached: verified
line: The LP-AM263P's R5F cores ran in lockstep from the debugger (the lockstep status bit set on both clusters, both checker cores refused a debug connection), at the same speed as split mode except 3 cycles on one build: TI's CoreMark 1,464.52 per second against 1,464.53, the repo's current loop 1.236 µs with the TMU in both. A current loop on core 0 kept the same timing to the cycle (mean 493.5 cycles, worst 844) while the other three cores ran CoreMark or copied memory in their own on-chip RAM. When they copied in the RAM right next to core 0's, its mean stayed the same but its worst pass, the first one with cold caches, rose from 2.11 µs to 2.51 to 2.70 µs. With the loop's handler, data and stacks in the core's own TCM, its worst pass was 1.26 µs alone and beside the same copies.
stop: none
-->
# Does lockstep, or a busy neighbour core, slow the LP-AM263P's control loop?

**Short answer.** Lockstep: no. A busy neighbour: not on average, but it can raise the worst case, and putting the loop in TCM, the core's own tightly coupled memory, removed that in this test. On the LP-AM263P (AM263P4 chip, four Cortex-R5F cores at 400 MHz), lockstep pairs two cores to run one program in step, so a hardware fault in either shows up as a mismatch. Split mode is the usual setup, where each core runs its own program. The TMU is the chip's built-in sine and cosine hardware.

| What | Split mode or alone | Changed setup | Difference |
|---|---|---|---|
| TI's CoreMark example, per second | 1,464.53 | 1,464.52 in lockstep | none |
| Current loop with the TMU, trigger to new PWM value (1,000 interrupts) | 1.235 µs | 1.236 µs in lockstep | none |
| Same, TI's software math library | 1.454 µs | 1.455 µs in lockstep | none |
| Same, C library sine and cosine | 2.060 µs | 2.068 µs in lockstep | 3 cycles (0.4%) |
| Current loop with the TMU on core 0 (200,000 interrupts) | 1.234 µs, worst 2.11 µs | 1.234 µs, worst 2.11 µs, with CoreMark on cores 1 to 3 | none, to the cycle |
| Current loop with the TMU on core 0, cores 1 to 3 copying memory in their own RAM | 1.234 µs, worst 2.11 µs | 1.234 µs, worst 2.11 µs, with the copies at 179 MB/s (library memcpy) or 779 MB/s (the repo's word loop) together | none, to the cycle |
| Current loop with the TMU on core 0, cores 1 to 3 copying in the RAM next to core 0's (899 MB/s together) | 1.234 µs, worst 2.11 µs | 1.234 µs, worst 2.51 to 2.70 µs | mean none; worst pass up to 28% longer |
| Current loop with the TMU on core 0, hot path in TCM, the same copies beside it | 1.194 µs, worst 1.26 µs | 1.194 µs, worst 1.26 µs | no change from the copies; 3.2% faster than the on-chip RAM build |

Lockstep costs half the cores, not speed. Busy neighbours left the loop's average alone in every test; only copies into the RAM beside core 0's block moved anything, and only the first pass, which runs with cold caches. In TCM the first pass is no longer slower than the rest: the worst case was 1.26 µs against 2.11 µs for the on-chip RAM build alone, and the spread (worst minus best) 0.09 to 0.10 µs against 0.905 µs, with or without the neighbours.

Core names, as TI writes them:

- Cluster 0: core 0 = R5FSS0-0, core 1 = R5FSS0-1. Cluster 1: core 2 = R5FSS1-0, core 3 = R5FSS1-1.
- In lockstep, core 1 checks core 0 and core 3 checks core 2.

Terms: [vocab](../vocab.md) (lockstep, split mode, TMU, TCM, GEL, DSLite, development boot). 1 kB = 1,024 bytes. Problems and fixes: [LP-AM263P FAQ](../../am263p/FAQ.md) ("Lockstep", "Using all four cores"). Code: `am263p/tools/lockstep.js`, `am263p/tools/lockstep_run.py`, `am263p/bench/ctrl/ctrl.c`. Logs: `am263p/data/logs/lockstep/` and `am263p/data/logs/e139/`. Every number is also a row in `data/results/am263p.csv`.

**Question.** The AM263P can pair its R5F cores in lockstep for functional safety. What does that cost a control loop? And in split mode, does a loop on one core slow down when the other three are busy?

**Vendor position.** TI's MCU+ SDK 26.01 documentation (read 2026-10-11):

- Bootloader page: "The SBL runs on R5 core in Lock Step Mode." and "The SBL will switch to Dual core or continue in lock step mode depending on whether App Image loaded by SBL contains the R5 Core 1 image or not."
- CCM page: "The R5F sub-system, when bootstrapped to lockstep mode, has a logic that compares the outputs of the two cores and asserts an interrupt whenever an error is detected."
- Smart placement page: "TCM-A 0x0 32KB (in split) or 64KB (in lockstep)", the same for TCM-B.

None of these gives a speed for lockstep, or says how to reach it from a debugger.

**What was tried.** LP-AM263P Rev A, MCU+ SDK 26.01.00.08, TI Arm Clang 5.1.1, CCStudio 21 (DSLite and its scripting), Intel Mac. Every program is loaded into RAM over the debug probe; no switch was moved and no flash was written.

1. **Lockstep.** TI's lockstep connection file printed "Lockstep Reset" but left both clusters in split mode (its own check said "Dual-Core"). Reading TI's GEL scripts showed why. The chip comes out of reset in lockstep: the lockstep field in MSS_CTRL `R5SS0_CONTROL` resets to 0x7. TI's usual connection file switches both clusters to split mode on every connect, and the lockstep file never writes that field back. So `am263p/tools/lockstep.js` connects without a GEL script, issues the debugger's System Reset, and runs TI's own GEL steps for unlocking, clocks and memory, leaving out the split-mode switch. Then it loads the program on core 0 and runs it. Each run reads the lockstep status bit (0x100 in `R5SS0_STATUS_REG` and `R5SS1_STATUS_REG`) before and after, and tries a debug connection to core 1.
2. **The same programs in lockstep and split mode.** TI's CoreMark example, and this repo's current loop from the [three-chip comparison](2026-10-11-current-loop-three-mcus.md): EPWM0 at 50 kHz starts an ADC conversion, the ADC interrupt runs one field-oriented-control step from the C2000's `foc.c` and writes the new PWM value. Three runs each, 1,000 interrupts per run, in the TMU, SDK and C-library variants.
3. **A busy neighbour.** The TMU loop for 200,000 interrupts (4 s) on core 0. Cores 1 to 3 ran one of four loads through the whole window: TI's CoreMark (60k iterations, about 41 s), each in its own 256 kB of on-chip RAM; a 64 kB block copied back and forth between two buffers in its own RAM for 40 s with the C library's `memcpy`; the same with the repo's own 8-byte word loop, about 4 times faster; and the word loop with one buffer moved into the 256 kB just below core 0's block (0x70000000 plus 64 kB per core, `-DFRL_MEMNEAR`; SysConfig's generated linker file names 0x70000000 to 0x7003FFFF `SBL`, the bootloader's area, and nothing in this repo's programs is placed there). 64 kB is meant to be larger than the R5F's data cache, so the copies keep going out to the on-chip RAM (the cache size was not checked; the SDK pages read here do not give it). Each copy run checks that the two buffers match at the end (`-DFRL_MEMSTRESS` in `am263p/bench/multicore/coremark4/cm4.c`). Core 0 is loaded first, because its connect clears on-chip RAM and would stop the others. Its program waits 20 s before starting, and the other three are loaded during the wait. Three runs loaded and three runs with core 0 alone, same image.

**How far it got.** Everything ran and was checked.

- Lockstep: bit 8 (0x100, lockstep) was set in both clusters' status registers after the reset, after setup and after every run: R5SS0 read 0x100, then 0x101 after setup (bit 0 is not explained in the pages read, and which setup step sets it was not found); R5SS1 read 0x100 throughout. Core 1 refused a debug connection in every run (DSLite error -1170, "Unable to access the DAP"). That was expected of a checker core; TI's pages do not say it. Cluster 1's checker, core 3, refused the same way in a later check. CoreMark printed its correct checksum for TI's 15k-iteration build (crcfinal 0x65c5) in all three runs: 1,464.508, 1,464.524 and 1,464.531 per second.
- Neighbour: cores 1 to 3 were loaded 4, 8 and 12 s after core 0, during its 20 s wait, so CoreMark had started before the loop began. When the loop finished, each CoreMark core's progress marker in shared memory read 3, "CoreMark running" (read over the probe). All three then finished with the correct checksum for the 60k-iteration build, crcfinal 0xbd59.

**Current loop, lockstep against split** (µs, median of 3 runs of 1,000 interrupts; split figures are the [three-chip comparison's](2026-10-11-current-loop-three-mcus.md)):

| Sine and cosine from | Trigger to handler, split | Lockstep | Trigger to new PWM value, split | Lockstep | Worst of the three runs, split | Lockstep |
|---|---|---|---|---|---|---|
| TMU | 0.632 | 0.632 | 1.235 | 1.236 | 2.14 | 2.14 |
| TI's software math library | 0.636 | 0.637 | 1.454 | 1.455 | 2.35 | 2.35 |
| C library | 0.654 | 0.662 | 2.060 | 2.068 | 2.74 to 2.80 | 2.69 to 2.81 |

With the TMU and the software math library, the worst pass in every run is the first interrupt after the load, with cold caches (the same cycle count every run). With the C library it is a later pass (the first takes 2.51 µs, the worst 2.69 to 2.81 µs), in both modes. The C-library variant is about 3 cycles slower in lockstep (median of the run means 827.3 cycles against 824.0, and every lockstep run above every split run); the other two match to within a cycle. The cause was not found.

**Current loop beside three CoreMark cores** (TMU, 200,000 interrupts, cycles at 400 MHz, every run):

| | Trigger to handler, mean | Trigger to new PWM value: best | mean | worst |
|---|---|---|---|---|
| Core 0 alone (cores 1 to 3 run nothing from this repo) | 255.0 to 255.2 | 482 | 493.5 | 844 |
| Cores 1 to 3 running CoreMark | 255.1 to 255.2 | 482 | 493.5 | 844 |

With either copy in the cores' own RAM the loop read exactly the same, in all three runs each: mean 493.5, best 482, worst 844 cycles.

**Copies next to core 0's RAM.** Three runs against the three alone:

| | Trigger to handler, mean | worst | Trigger to new PWM value: best | mean | worst (the first pass) |
|---|---|---|---|---|---|
| Core 0 alone | 255.0 to 255.2 | 318 | 482 | 493.5 | 844 |
| Cores 1 to 3 copying next to core 0's RAM | 255.0 to 255.2 | 344 to 376 | 482 | 493.4 to 493.5 | 1,004 to 1,080 |

The mean did not change, so the other passes did not either on average; the program records only the worst pass, which was the first one each time. The first pass after the load fetches its code and data from the on-chip RAM, with cold caches; it was slower here, with three cores writing beside it. Suspected cause: the neighbours' traffic shares a path to the RAM with core 0's block; no TI description of how the on-chip RAM is split into banks was found. With it, the loop's spread from best to worst (jitter) went from 0.905 µs to 1.405 to 1.495 µs, and its worst pass from 10.5% of the 20 µs period to 13.1 to 13.5%.

**The loop in TCM.** TCM is memory wired to one core. Suspected: other cores' traffic does not reach it; whether other cores or DMA can address it was not checked. The loop was rebuilt with `-DFRL_TCM`: the ADC handler and `foc_step` in TCM-A beside TI's interrupt dispatcher, and the variables, the main stack and the interrupt stacks in TCM-B. The linker file is SysConfig's own with those groups moved; the rest of the program and its heap stay in on-chip RAM. The only on-chip RAM call left in the handler, a 64-bit divide for the timing, runs after both timestamps are taken. Three runs each, cycles:

| | Trigger to handler, mean | worst | Trigger to new PWM value: best | mean | worst |
|---|---|---|---|---|---|
| TCM, core 0 alone | 236.6 to 236.7 | 246 | 466 | 477.7 to 477.8 | 502 to 504 |
| TCM, cores 1 to 3 copying next to core 0's RAM | 236.6 to 237.0 | 246 | 466 | 477.8 | 502 to 506 |

The first pass now takes 494 cycles, no longer the worst. The loop is also 3.2% faster on average (477.7 against 493.5 cycles), and the copies beside it ran at −0.08% to +0.72% of their rates beside the on-chip RAM build, a little faster in two of three runs.

**Memory copy on cores 1 to 3** (MB/s per core = bytes copied, not read plus written, per second, with 1 MB = 1,000,000 bytes; median of 3 runs, which differ by under 0.3%):

| Copy | Core 1 | Core 2 | Core 3 | Together |
|---|---|---|---|---|
| C library `memcpy` (in this build `TI_memcpy_small`, a byte loop) | 59.2 | 58.6 | 61.4 | 179 |
| The repo's 8-byte word loop | 247.5 | 235.3 | 296.5 | 779 |
| The repo's 8-byte word loop, one buffer next to core 0's RAM | 327.2 | 262.2 | 309.1 | 899 |

The copy rates were the same with and without the loop on core 0, to within 0.3%. Why the cores copy at different rates, and why the copy next to core 0's RAM is faster than within their own, was not found. The reworked build that moves the buffer was checked against the old one: built for the cores' own RAM it copied at 247.7, 235.4 and 296.5 MB/s, within 0.1% of the old build.

Back to the CoreMark neighbour: CoreMark on cores 1 to 3 did not slow down either: 1,463.0, 1,460.2 and 1,461.4 per second beside the loop. The control for that is CoreMark only, no loop: the same images with core 0 running a silent loop gave the same scores to 0.01 (3 runs each). Both CoreMark sets are a little under the four-core page's figures for one core alone (1,463.4 to 1,464.1) because this build runs 60k iterations with no common start; compare only within one build.

**The line.** Nothing stopped. The limits:

- The worst case moved only for the first pass after a load. A loop that misses its cache often (more code than the cache, or data in shared memory) would feel the neighbours on every pass; not tested. TCM-A holds 31.9 kB in split mode, and the program's `.text` section alone is 31.9 kB (0x7FA0 bytes in the on-chip RAM build's map) before the start-up and interrupt code, so only the hot path moved. Streaming from the flash or by DMA was not run. A faster copy than the word loop would load the memory harder; not run.
- Lockstep was reached from the debugger in development boot, not through TI's bootloader. Only TI's CoreMark example (code in on-chip RAM) was run in lockstep; the CoreMark build with its code in TCM was not. The bootloader route (lockstep when the image has no core 1 program) was not run.
- TI's doubled TCM in lockstep, checked for TCM-A only: in lockstep a word written at 0xC000 (past 32 kB) read back separately from one at 0x4000; in split mode the debugger could not access 0xC000 at all. The same test on TCM-B (0x8C000) read back separately in both modes, so it does not show the TCM-B size; what sits at that address in split mode was not found.

Three things cost time and are worth knowing:

- **Tool or docs:** TI's lockstep connection file did not reach lockstep after split-mode connects had run. Suspected, from reading TI's GEL scripts: it never writes the lockstep field back. It also left both clusters' FORCE_WFI register at 0x7, and after that every load with TI's usual connection file reran the previous program instead of the new one, until a System Reset from the debugger. A probe reset did not clear it. The register reads are in `board-state-2026-10-11.md` (copied from the terminal). Check after any reset change: load two programs with different banners in turn.
- **Tool or docs:** each connect to core 0 with TI's usual connection file clears on-chip RAM, so programs already running on cores 1 to 3 stop. Load core 0 first.
- **Our mistake:** the first neighbour runs loaded core 0 last, so the loop ran alone; reading the other cores' progress markers during the run caught it before anything was written.

- A loop whose code or data does not fit the cache, beside neighbours copying next to its RAM; neighbours running from flash or a DMA stream.
- Two loops on two clusters at once (core 0 and core 2), each with its own PWM and ADC.
- Lockstep through TI's bootloader from flash, and a fault injected to see the compare logic fire.

**To repeat this.** You need CCStudio 21, MCU+ SDK 26.01, Python with pyserial and your probe's serial. The steps:

1. Lockstep: build TI's CoreMark example with the SDK makefile. Connect to core 0 without a GEL script, issue the debugger's System Reset, run TI's GEL steps for unlocking, clocks and memory, and leave out the split-mode switch. Load the program and read both clusters' status registers. Expect SS0 0x100 and SS1 0x100 after the reset, core 1 refusing a debug connection, and CoreMark 1,464.5 per second.
2. Loop alone: build TI's `adc_soc_epwm` example with `-DUSE_TMU` and 1,000 interrupts. Expect a minimum of 484 cycles.
3. Busy neighbour: load core 0 first with `-DUSE_TMU -DN_IRQ=200000 -DSTART_DELAY_S=20`, then CoreMark with 60k iterations on cores 1 to 3. Expect a minimum of 482, a mean of 493.5 and a maximum of 844 cycles.
4. Copy neighbour: the same with a copy loop (`-DFRL_MEMSTRESS=40`, with `-DFRL_WORDCOPY` for the word loop). Next to core 0's RAM, add `-DFRL_MEMNEAR`; expect a maximum of 1,004 to 1,080 instead of 844.
5. Loop in TCM: build with `-DFRL_TCM` and a linker file that moves the handler to TCM-A and the data and stacks to TCM-B. Expect a minimum of 466, a mean of 477.7 and a maximum of 502 to 506, alone or beside the copies.

Loading one core at a time over the probe: load core 0 first, because each connect to core 0 clears on-chip RAM.

**Ask.** If you have run the AM263P's lockstep compare logic with an injected fault, or measured a control loop beside a memory-heavy neighbour, compare results in an issue.
