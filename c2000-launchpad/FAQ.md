# What goes wrong with the TI LAUNCHXL-F2800137 LaunchPad development kit on a Mac, and how do you fix it?

Every problem hit bringing up a used TI LAUNCHXL-F2800137 (chip TMS320F2800137: C28x core, 120 MHz, 256 kB flash, 36 kB RAM; onboard XDS110 probe) from a Mac terminal on 2026-10-06. Each answer starts with the fix, names what caused it, and quotes the exact error text so a search finds it. Versions: Intel Mac, macOS 15.7.9, CCStudio 21.0.1, C2000 compiler 25.11.1.LTS, SysConfig 1.28.1, UniFlash 9.6.0, C2000Ware 26.02.00.00 LTS, probe firmware 3.0.0.43. 1 kB = 1,024 bytes.

**What caused it**, one tag per answer. The tags used on this page:

| Tag | Meaning |
|---|---|
| Setup or handling | How the hardware was plugged in or set up |
| TI example default | A TI example's default setting is why it fails or misleads |
| Tool or docs | A TI tool bug, a missing tool, or a missing or wrong instruction |
| Chip limit | The hardware cannot do it |
| Third-party behavior | Software from outside TI, such as a benchmark's own check |
| Unknown | Not found; a lead is written as "suspected" |

Contents: [Connecting](#connecting), [Getting the software](#getting-the-software), [Benchmarks](#benchmarks), [Running examples by script](#running-examples-by-script), [The DCC clock examples](#the-dcc-clock-examples), [Neural nets](#neural-nets), [The ADC](#the-adc), [Libraries written for 8-bit bytes](#libraries-written-for-8-bit-bytes).

Words used below (more in [docs/vocab.md](../docs/vocab.md)): the **probe** is the XDS110 debugger built into the LaunchPad. **cJTAG** is the 2-wire debug link this board uses. A **connection file** (`.ccxml`) tells TI's tools which probe and chip to use. The **TMU** is the chip's trig and divide hardware. Bring-up records quoted below:.

## Connecting

### Why does TI's software refuse to connect to my LAUNCHXL-F2800137, and how do I update the probe?

Update the probe's firmware with TI's command-line tool `xdsdfu`. The test board arrived on 3.0.0.19 and TI's software would not connect until it was on 3.0.0.43. UniFlash's command line printed `IcePick_C_0: Warning: A firmware update is required for the XDS110 probe. The current firmware is version 3.0.0.19. The probe must be upgraded to firmware version 3.0.0.43 to be compatible with this software. Click the "Update" button to update the firmware.` and then `Failed: Timed out after 45 seconds`; there is no Update button on the command line. The commands that worked, both in `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/`:

```bash
./xdsdfu -I CL371081 -m
```

```bash
./xdsdfu -I 00000000 -f firmware_3.0.0.43.bin -s CL371081 -r
```

The first switches the probe to update mode; the second writes the firmware, puts the serial number back and restarts it. Use your probe's serial number in place of <probe serial>. While the update runs the serial number shows as `00000000`; it came back as <probe serial> afterward. The update itself was observed 2026-10-06 and its output not saved.

### My LAUNCHXL-F2800137 is new. Do I still need the probe update, and how do I check first?

Check first: run `./xdsdfu -e` in the same folder; it lists each XDS110's firmware version and serial number (TI's `XDS110SupportReadMe.pdf` in that folder). If it is older than what your CCStudio or UniFlash asks for (3.0.0.43 for CCStudio 21.0.1 and UniFlash 9.6.0), update it as above. Only one board, a used one, was tested, so the firmware new boards ship with is unknown.

### Why does UniFlash's command line fail with error -2131 "Unable to access device register" on the LAUNCHXL-F2800137?

Put the property `SWD Mode Settings` = 4 (cJTAG 2-pin advanced) in the connection section of the connection file. The device file defaults to cJTAG, but hand-written files failed without the line, from UniFlash 9.6.0's command line, with `fatal: IcePick_C_0: Error connecting to the target: (Error -2131 @ 0x0) Unable to access device register. Reset the device, and retry the operation.` followed by `Failed: Operation was aborted on 'C28xx_CPU1'`. CCStudio's window with a file it generated was not run. TI's own `TMS320F2800137_LaunchPad.ccxml` in C2000Ware has the line and connected first time; `tools/F2800137_CL371081.ccxml` is that file plus the probe's serial number The console text is saved in.

### Can I use 4-pin JTAG on the LAUNCHXL-F2800137?

No, not as shipped; use cJTAG. It is how the board is built, not the chip: the board wires the XDS110 for 2-pin cJTAG only, and the TDI and TDO lines on GPIO35 and GPIO37 need resistors R44 and R45 fitted, which the board ships without (user guide SPRUJ32, section 3.2.1).

### TI's tool says `Configuring Debugger (may take a few minutes on first launch)...`. How long does it really take on the LAUNCHXL-F2800137?

On this bench, with TI's connection file, that step took 1.8 s. The "few minutes" is TI's own warning for a first launch; the first launch was not timed, so leave the first connection a long timeout.

### How do I read which chip and silicon revision my LAUNCHXL-F2800137 has?

Read two registers with UniFlash's command line: PARTIDH at 0x5D00A and REVID at 0x5D00C, two 16-bit words each. The chip's marking alone does not settle it: the test board's says `X F2800137PM`. It read PARTIDH `0x06FF0500`, which data sheet SPRSP63B lists as the TMS320F2800137, and REVID `0x00000003`, silicon revision B. The command, with your connection file:

```bash
/Applications/ti/uniflash_9.6.0/deskdb/content/TICloudAgent/osx/ccs_base/DebugServer/bin/DSLite memory -c tools/F2800137_CL371081.ccxml -r 0x5D008,8 -s 16 -o id.bin
```

It reads 8 words from 0x5D008 (PARTIDL, PARTIDH, REVID) into `id.bin`. The read is saved as `data/chip-id-0x5D008.bin`, with its hex dump in. The X prefix marks a pre-production (TMX) part; the data sheet's "Revision Identification" table lists revision B as available as production TMS parts too.

### How do I run a program and watch the LED from a terminal?

Use the scripting runner. Build with `tools/c2kbuild.sh <folder> RAM`, then run `/Applications/ti/ccs2101/ccs/scripting/run.sh tools/c2krun.js <abs path to .out> blink`. Separate UniFlash command-line calls each reconnect, so they cannot watch a running program. TI's `led_ex1_blinky` toggled GPIO20 every 500 ms in 8 samples 250 ms apart. This loads the program into RAM, so it is gone at power-off; build with `FLASH` to keep it. `README.md`.

### How do I print my own message from the LAUNCHXL-F2800137 over the USB cable?

Run `python3 tools/frl/frl.py run --board f2800137 --recipe print-message --probe <serial> --message "Hello from my LaunchPad"`. It backs up main flash, copies TI's `launchxl_f2800137` demo, swaps its ASCII logo and ADC readout for `> <message> <count>` once a second, builds it with the C2000 compiler, writes main flash through CCStudio scripting and reads the probe's first serial port (`/dev/cu.usbmodem<serial>1`) at 115200 baud. Three runs took 67 to 73 s each, with the flash write at 11.2 to 14.0 s, and the message counted 3 to 4 times in the 4 s listen. The demo's SCIA pins (GPIO28 receive, GPIO29 transmit) are the ones the board wires to the probe, so nothing needs moving. The tools leave the program running from flash; `frl restore <backup folder>` puts TI's original demo back and read back identical. `data/results/f2800137.csv`, `message_flash_repeat`.

### frl verify says `FRL-E042 UNVERIFIED: only 18 reads a second through the probe`. Is the board wrong?

No: the check could not read the LED pin fast enough to time it. CCStudio reads this pin about 42 times a second when the computer is quiet; with five other boards writing at once it read 18, below the 20 the check needs, so frl refused to guess. Stop other debug sessions and board runs, then run `frl verify` again. Seen 2026-10-25; `docs/frl.md`.

### How long does it take from box to a blinking LED on the LAUNCHXL-F2800137?

From a board that is already powered and identified, with the tools installed: 58 s for the whole terminal path in one command (`python3 tools/frl/frl.py run --board f2800137 --probe <serial>`: backup read twice, build, write to main flash, LED4 on GPIO20 timed to 150, 150, 150, 900 ms), and the restore read back identical. One run. The first-power steps (probe firmware update, the slow first connection, installing the tools) are not in that number and were not timed. On first bring-up TI's `led_ex1_blinky` built in 4.6 s and loaded into RAM in 3.9 s (observed 2026-10-06; not saved). That load is not a flash write, so the program is gone at power-off. Before it came the probe firmware update and the first, slow connection, neither timed. A flash write of the blink example alone took 9.2 to 9.9 s over three runs; writing the 256 kB demo image back to flash took 18.6 s and 20.9 s. `README.md`.

### Does it work from CCStudio's window, or on an Apple Silicon Mac?

Not tested. Everything here ran from a terminal on an Intel Mac: CCStudio's scripting runner, UniFlash's command line and this repo's build script. CCStudio's window was not used and no Apple Silicon Mac was available.

## Getting the software

### The C2000Ware download on ti.com asks for a login and export approval. Is there another way?

Yes: TI's public GitHub repo has C2000Ware. The ti.com page does not mention it. A sparse checkout of 26.02.00.00 LTS needed no login and takes 427 MB on disk, including its 87 MB git folder. `README.md`.

### Can TI's Neural Network Compiler (NNC) run on my Mac?

Not on an Intel Mac: NNC 2.1.2 has no Intel Mac build. It ran on GitHub's Linux machines with the C2000 compiler 25.11.1.LTS for Linux, from a GitHub Actions job. TI's NNC guide lists Linux and Windows. An Apple Silicon Mac was not run.

## Benchmarks

### Why does CoreMark fail its checks on the TMS320F2800137?

Unmodified, it builds a different-sized workload, because the C28x's smallest memory unit is 16 bits; a three-change patch fixes that, but the score is then unofficial. That applies to the unmodified sources: `char`, `short` and `int` are all 16 bits and `sizeof` counts 16-bit words. Unmodified, three of four result checks fail (`[0]ERROR! list crc 0x0814 - should be 0xe714`, and the same for the matrix and state CRCs) and CoreMark's own type check prints `ERROR: ee_u16 is not a 16b datatype!`; the printed 117.8 iterations per second is not a score. This repo's patch counts the list in 8-bit bytes and masks characters to 8 bits; then all four checks match the standard run. crcfinal depends on the iteration count: 0x4983 at 2k iterations, as on the CC2340R5, and 0xcc42 at 3,000, as on the MSPM0G3507. From flash it scores 141.0 per second (1.175 per MHz), from RAM 155.3, at `--opt_level=4 --opt_for_speed=5`..

### Why does the C2000 compiler say `identifier "int8_t" is undefined`, and why does portable C give different answers on the C28x?

The C28x has no 8-bit type, so TI's `stdint.h` leaves out `int8_t` and `uint8_t`; C2000 compiler 25.11.1 prints `"i8.c", line 2: error: identifier "int8_t" is undefined` for a file that uses it. Add your own `typedef int16_t int8_t;` and check that the code never relies on 8-bit wraparound. The room-occupancy models clamp every value, so they gave 20 of 20 right answers. Every int8 value takes 2 bytes, so int8 weights need twice the flash.

### Why does a C++ library fail to compile for the C28x with `invalid storage class for a template declaration` or `may not have a parameter of type`?

The C2000 compiler (cl2000 25.11.1.LTS) accepts C++03 only. `--c++11` and `--c++14` print `invalid compiler option (ignored)`, and rvalue references (`String(String &&)`), trailing return types and `auto` are errors. ArduinoCore-API fails this way on `String.h` line 75 and `Common.h` line 145, in all five API files; plain C needs `--c11` for declarations inside `for` loops (MicroPython `py/`). uLisp, which is C-style C++, does compile (`ports/ulisp/boards/f2800137/PORT.md`). Measured 2026-11-09; logs in `c2000-launchpad/bench/arduino/` and `c2000-launchpad/bench/micropython/`.

### Why do Embench-IoT's `edn`, `matmult-int` and `picojpeg` fail their own check on the TMS320F2800137?

Not traced; use their cycle counts with care. All three do byte or 16-bit integer arithmetic, and they probably assume an 8-bit `char` or a 32-bit `int`, which the C28x does not have. With `st` (next answer) that makes 4 of 19 that fail; 15 of 19 pass. On the MSPM0L1306, 13 of 19 pass, but only 14 fit in its 4 kB of RAM. The build used `CPU_MHZ=1`, which sizes each benchmark's work for a 1 MHz chip, the same setting as on the MSPM0 boards.

### Why does Embench-IoT's `st` fail on the C28x and on the MSPM0 boards?

It fails on all three chips, so it is not the 16-bit byte. On the MSPM0G3507 it was traced to Embench's 1e-13 tolerance being finer than one step of a double; not traced on the C28x.

### How do I compare a C2000 chip with an Arm chip fairly?

Time the work you care about in cycles, and use the patched CoreMark only with its label. The [README](README.md) lists CoreMark, Dhrystone, the occupancy models, FFT, Embench and ADC rate, each with its conditions.

### Is the C28x faster or slower than an Arm Cortex-M0+ for my code?

Slower per MHz on general integer code, faster per MHz on float work and on the int8 multiply-adds of small neural nets. Which chip wins depends on the code you run, so time your own. Chip and compiler are measured together here. Integer, from flash on both: patched CoreMark 1.175 per MHz against 2.30 on the MSPM0G3507, Dhrystone 0.362 against 0.731 on the MSPM0L1306. Multiply-adds: the occupancy networks, which are int8, take 2 to 24% fewer cycles than on the M0+. Float: the networks' quantize step takes about 10 times fewer cycles, and Embench's `minver` 2.4 times fewer, because the C28x has a floating-point unit and the M0+ does not. In seconds, the 120 MHz C28x finishes the models and FFTs well ahead, but the 80 MHz MSPM0G3507 still beats it on CoreMark (183.6 per second against 141.0 from flash) and on several integer Embench benchmarks.

### Why does the TMS320F2800137 need more cycles per byte for AES and per pixel for JPEG than the Arm Cortex-M chips?

Not looked into; if AES or JPEG speed matters, time your own build, and run it from RAM at `--opt_level=4 --opt_for_speed=5`. The suspicion: the C28x's smallest data type is 16 bits, and tiny-AES-c and picojpeg are written for 8-bit bytes, so byte work costs extra masking; the generated code was not studied. At -O2 from flash, tiny-AES-c CBC encrypt of 1 kB takes 754 cycles per byte here against 235 on the CC1354P10's Cortex-M33 and 405 to 425 on the Cortex-M0+ chips, and picojpeg 491 cycles per pixel against 178 to 344 on the Arm cores. From RAM at -O4 with speed level 5, AES drops to 395 cycles per byte, level with the M0+ chips, but the Arm builds did not use RAM or a speed option. TI C2000 compiler 25.11.1.LTS; rows in each board's file in `data/results/`; page.

### Are the AES speed numbers for the F2800137 and the Arm boards like for like?

Nearly: every board runs tiny-AES-c CBC encrypt of 1 kB with the key expanded before the timing starts, and every result matches the standard test vectors. Two differences remain: the CC2340R5 and CC1354P10 rows take the 1 kB copy into the buffer out of the count (2 to 4 percent of the software time there), while the F2800137 and MSPM0 rows keep it in; and the hardware AES rows on the Arm chips load the key on every call, which is most of the cost for one 16-byte block. The F2800137 has no AES hardware. Rows in `data/results/`.

### How much slower is code in flash than in RAM on the F2800137?

For plain-C programs, 10 to 22% more cycles at 120 MHz. Flash needs 2 wait states at 120 MHz; TI's `Device_init()` sets them and turns on prefetch and the cache. From flash the motor-control (FOC) step took 14% more, CoreMark 10 to 18%, the neural nets 14 to 19%, Dhrystone 22%; single TMU calls 1.5 to 4 cycles more. The whole PWM-triggered loop took only 8% more, because the ADC's sample and conversion take the same time either way. Not always slower: TI's NNC build of one model and the 256-point complex FFT ran faster from flash. Copying the motor-control step and its interrupt to RAM with `#pragma CODE_SECTION(..., ".TI.ramfunc")` brought them back to the RAM time.

### Do I need assembly to use the TMU?

No: it is a compiler option. Add `--tmu_support=tmu0 --fp_mode=relaxed` to the C2000 compiler's flags, and the compiler turns `sinf`, `cosf`, `atan2f`, `sqrtf` and float divide into TMU instructions. With both flags and code in RAM, sine takes 8 cycles per loop iteration against 82 without either, each including about 7 cycles of loop overhead (9 to 12 from flash). `--fp_mode=relaxed` was not tested on its own, so part of the gain may be that flag.

### How fast can a control loop run on the F2800137?

At 50 kHz a PWM-triggered loop with one field-oriented control (FOC) step uses 12 to 13% of the period with the TMU, 2.3 to 2.6 µs from PWM trigger to new PWM value. About 66 cycles of that are the ADC's sample and conversion and the interrupt entry; the interrupt code itself takes about 220 cycles from RAM and 243 from flash. Measured with a rate sweep (1,000 periods, none missed), the TMU loop holds 301.5 kHz from flash and 336.1 kHz from RAM, and at 100 kHz it takes 29.7 and 26.4 percent of the CPU; without the TMU 209.4 and 231.7 kHz. A real drive needs time for other work on top.

### How fast can the F2800137 run a digital-power PID loop?

With no missed period in 1,000, the PID loop holds 439.6 kHz in float and 367.0 kHz in Q15 from flash (500.0 and 431.7 kHz from RAM), and at 100 kHz it takes 19.7 and 24.2 percent of the CPU (TI C2000 compiler 25.11.1.LTS -O2, 120 MHz). Float is faster than Q15 here. The generated code was not compared.

### Why is the F2800137's highest loop rate lower than its CPU time allows?

From flash the PID interrupt takes 236.5 cycles of CPU (float) and 290.7 (Q15), which alone would allow about 507 and 413 kHz; the sweep found 439.6 and 367.0 kHz. The FOC loop's flash C-library build, at 209.4 kHz, also came in 5 percent under the 220 kHz estimate, which probably has the same cause: the estimate left out part of the interrupt's return.

### Why is the F2800137 power loop's jitter not quoted?

The first interrupt of each pass in that capture was a stale one, entered with the PWM counter still at 0, so its best case is not real. The program enabled the ADC interrupt without clearing the flag left pending from setup. The worst case and the mean are valid; the rate sweep clears the flag before every trial.

### Why is the ePWM count on the F2800137 not exactly 1,000 per 1k CPU cycles?

Unknown. The ePWM clock is set to the CPU clock, and the two flash runs that saved the check counted 991 and 1,005 ePWM counts per 1k CPU cycles; this repo treats ePWM counts as CPU cycles.

### Why does my ADC interrupt read the previous sample when I use the early interrupt?

The early interrupt (`ADC_PULSE_END_OF_ACQ_WIN`) runs your code at the end of the sample window, before the conversion finishes; wait for the conversion before reading the result register, or use the end-of-conversion interrupt. The test read the result straight away, so in early mode it read the previous sample; it starts the code about 20 cycles sooner (44 to 49 cycles after the trigger against 64 to 70, from RAM). TI's published 49-cycle figure for the F28004x (SPRACW5A) uses early mode.

### Why do the Dhrystone runs on the F2800137 stop at 30,000?

Dhrystone keeps its run count in an `int`, which is 16 bits on the C28x, so it cannot count past 32,767. 30,000 runs take 0.3 to 0.4 s; the timer counts every CPU cycle, so the short run loses no precision. Results: 0.362 DMIPS/MHz from flash, 0.442 from RAM, rules followed.

### Why did the fastest CoreMark build not fit in the F2800137's RAM with a 10 kB stack?

A RAM-only build puts the code, data and stack in the 36 kB of RAM, and the `--opt_for_speed=5` code is 19 kB; cut the stack to 6 kB (CoreMark needs about 4 kB of it) or build for flash. The 6 kB build used 30.3 kB of RAM and scored 155.3 per second; the flash build of the same setting needs 12.6 kB of RAM..

### Why does my F2800137 RAM build fail with `program will not fit into available memory, or the section contains a call site that requires a trampoline`?

The program is bigger than the 36 kB of RAM: a RAM-only build puts code, constants and stack there. Build for flash, where code and constants sit in the 256 kB and RAM holds only data and stack. The occ_cnn_30k model's weights alone are 65.7 kB, so it cannot run from RAM; from flash it runs in 65 ms. The linker names the section that did not fit: for that model `fails for section ".const" size 0x97de page 0`, 39k words of constants. `fit-check.md`.

### Why does the occ_cnn_8k model not link as a RAM-only image when its weights fit?

The test harness is the cause, not the model: drop the test vectors or build for flash. The weights are 17.4 kB, but every test image also carries 20 test vectors (10.2 kB) and a 2 kB stack, which together with the code pass 36 kB. Without the vectors it would be about 27 kB, an estimate that was not built.

### Will a 2,048-point float FFT fit on the F2800137?

Probably not with TI's FPU DSP library at its stock settings; none was built. A 1,024-point complex FFT used two 8 kB buffers, one starting on a 4,096-word boundary as the library needs, plus a 4 kB twiddle table (the sine and cosine values it multiplies by): 20 kB of the 36 kB. Scaled to 2,048 points that is about 40 kB, and the library's stock tables stop at 1,024 points. A smaller layout, fixed point and another library were not run.

### Why are my FFT timings slower than the cycle counts in TI's DSP library headers?

Measured on this bench, 5 to 17% more cycles than `fpu_cfft.h` and `fpu_rfft.h` list. TI does not say where its code and buffers were placed; the measured runs had code and data in RAM, called from C at `-O2`. Results were right (checked against numpy).

### Is FreeRTOS available for the F2800137?

None found. C2000Ware's public GitHub copy has no `kernel/` folder, C2000Ware's docs list FreeRTOS examples for other C2000 chips but not the F280013x, and upstream FreeRTOS has no C28x port. The full C2000Ware installer, which needs a login, was not checked.

## Running examples by script

### How did you judge whether an example "ran"? Is running the same as working?

No. Running only means it did not crash. The sweep script loaded each example, let it run 3 seconds, and checked where the CPU was: 114 were still running in normal code, 21 had stopped at their own `ESTOP0` (C2000Ware's "inspect results here" stop), 2 low-power examples could not be halted (see the `Operation was aborted` answer below), and none was in a fault handler. Most drive a peripheral that needs a wire, a second board or a sensor to show they work. Where an example sets its own pass or fail variable, the script reads it.

### Which C2000Ware examples should I never load on a board I want to keep?

`dcsm/dcsm_security_tool`. Its image puts data at 0x78000 to 0x78220, the chip's one-time-programmable security area (zone 1 and 2 passwords and settings), and some of those words are not the erased value. Loading it through the debugger's flash tool would program them for good. The sweep's safety check found the sections in the link map and skipped it. Check any image's map for sections outside RAM and main flash (0x80000 to 0x9FFFF) before loading.

### Why does a terminal build of a SysConfig-only C2000Ware example fail with `unresolved symbols remain` for `code_start`?

Add C2000Ware's `device_support/f280013x/common/source/f280013x_codestartbranch.asm`. The newer projectspecs list only the example's `.c` and `.syscfg`; the entry branch is not in them and SysConfig does not generate it. CCStudio must add it on import; how was not checked. Also pass `--abi=eabi` (CCStudio adds it for ELF projects) and compile every `.c` SysConfig writes, including its subfolders.

### Why does `transfer_ex2_customlog_export_sci_buffer` fail with `TransferRingBuffer_init` unresolved?

Add C2000Ware's `utilities/transfer/source/transfer_ringbuffer.c` to the build. Neither the projectspec nor SysConfig's output lists it. Whether CCStudio's import adds it was not checked.

### How do I put the LAUNCHXL-F2800137 back on TI's out-of-box demo, and how do I know it worked?

Write the flash image you saved on arrival back with `tools/c2kflashback.js`, then reset with `tools/c2krestore.js`. The script programs main flash only, verifies it, and reads the whole bank back; the readback matched the saved SHA-256 exactly, both times it was done (18.6 s and 20.9 s to write). The demo prints `ADCINA6 Sample` and a reading once a second on `/dev/cu.usbmodemCL3710811` (the probe's first serial port; yours has your probe's serial number) at 115200 baud. If you did not save the flash, rebuild TI's `launchxl_ex1_f2800137_demo` for flash. `README.md`.

### After one example, every later example stops inside `SysCtl_setClock`. Why?

Use a System Reset before loading, not the debugger's default reset. After TI's `sysctl_ex3_intosc_to_xrosc_config` changed the clock source, the next 7 images all halted in `SysCtl_setClock`; a `System Reset` (`s.target.reset("System Reset")` in CCStudio scripting) cleared it and they ran normally.

### What does `Operation was aborted on 'C28xx_CPU1'` mean when I halt a low-power example?

Probably that the chip is asleep: press the wake-up button or reset. `lpm_ex3_standbywake_gpio` and `lpm_ex5_haltwake_gpio` put the chip in STANDBY or HALT and wait for a GPIO edge, as their source says, and only these two gave the error. The debugger probably cannot halt a sleeping chip; nobody pressed the button to confirm. The debugger reconnected for the next image.

### Is `(Error -261 @ 0x0) Invalid response was received from the XDS110.` fatal?

No. Retry the load. It happened once while the Mac's load average was 78; the same image loaded fine on the next try, and the probe stayed listed by `xdsdfu -e`. Once more the scripting runner exited with no message while loading `hrpwm_ex6_duty_sfo_qformat`; that image also ran fine on retry. `sweep/tools/run_list.py` restarts the runner after either.

### What does `There are not enough resources available` mean when I time code with the debugger's profile clock?

The profile clock and a hardware breakpoint compete for the same debug hardware. With code in flash every breakpoint is a hardware one. Time with a CPU timer instead: the script starts CPU Timer 1 from the debugger at the breakpoint and reads it after stepping out.

### Why do some C2000Ware examples stop in `verifyEEPROMRead` or set `errorFlag` with nothing else wrong?

They need wiring or a second device. The I2C EEPROM examples need an EEPROM on the bus, `can_ex5_transmit_receive` needs a CAN bus, and the eQEP, eCAP and external-interrupt examples need jumpers or signals on their pins; each example's comments say which. On a bare LaunchPad they run but have nothing to talk to.

## The DCC clock examples

The DCC (dual-clock comparator) is the chip's block that checks one clock against another. Four of C2000Ware's DCC examples report fail as shipped on this board; all four pass with the changes below.

### Why do TI's DCC examples report fail on the LAUNCHXL-F2800137?

Two causes. `dcc_ex1`, `dcc_ex2` and `dcc_ex4` need the board's crystal turned on, which TI's default clock setup leaves off; `dcc_ex3_continuous_monitoring_of_clock_syscfg` waits for a clock error its settings never cause. The fixes, one per example, are in the next three answers. Each crystal example's header says the crystal must be enabled; a script or a newcomer sees only `result` = 1 (fail).

### Why does `dcc_ex1` or `dcc_ex2` stop at `ESTOP0` under the comment `XTAL is not enabled!!`?

Build it with `--define=USE_PLL_SRC_XTAL`, which turns the board's crystal on and runs the clock from it; both then pass. C2000Ware's `device.h` runs the PLL from the internal oscillator (`#define USE_PLL_SRC_INTOSC`), so the crystal stays powered down (`XTALCR` = 1 after `Device_init()`). These examples check the crystal first and stop at their own `ESTOP0` with `result` still at its starting value, 1, which they define as fail. In CCStudio, add the define under the project's predefined symbols. From a terminal, with this repo's build script and a copy of the example folder (the third argument is the optimization level, `off` as in TI's project; everything after it goes to the compiler):

```bash
tools/c2kbuild.sh <your copy of dcc_ex1_single_shot_verification> RAM off --define=USE_PLL_SRC_XTAL
```

### Why does `dcc_ex4_clock_fail_detect` pass for the wrong reason?

Build it with `--define=USE_PLL_SRC_XTAL --define=CPU_FRQ_100MHZ`. It monitors the PLL as a 100 MHz clock, while TI's `device.h` sets 120 MHz. With only the crystal define, its clock monitor flagged an error before the example turned the crystal off, so its pass did not test anything. With both defines it counted no error until the crystal was turned off, then passed: `tools/c2kbuild.sh <your copy of dcc_ex4_clock_fail_detect> RAM off --define=USE_PLL_SRC_XTAL --define=CPU_FRQ_100MHZ`..

### Why does `dcc_ex3_continuous_monitoring_of_clock_syscfg` report fail when the clock is fine?

Add `dcc1.fclk1 = 100;` to its `.syscfg` file; it needs no crystal. Its pass condition is that the monitor fires an error. Its comment says it uses "an un-realistic tolerance" to force an error on a 100 MHz clock, but the SysConfig file leaves the clock at SysConfig's default of 120 MHz with a 3% tolerance, so the window it computes fits the real clock (`DCC_setCounterSeeds(myDCC0_BASE, 3264, 272, 40800)`). With `fclk1` at 100 the error fires and it reports pass. The non-SysConfig `dcc_ex3_continuous_monitoring_of_clock` passes as shipped (`result` = 0).

### Does the LAUNCHXL-F2800137 have a crystal, and what frequency is it?

Yes, Y2, wired to the chip's X1 and X2 pins by default (user guide SPRUJ32, section 3.3.6). Its frequency is assumed to be 20 MHz, not measured. The user guide does not give the frequency; TI's `device.h` assumes 20 MHz when the crystal is the clock source. With 20 MHz assumed, the DCC read the internal oscillator INTOSC2 at 9.991 MHz and the PLL at 120.13 MHz, close to their nominal 10 and 120 MHz. That shows the three clocks agree; it does not measure the crystal on its own, because the internal oscillator has its own tolerance.

## Neural nets

### What is the room-occupancy model, and why is it the benchmark?

Four small neural nets that count the people in a room (0 to 3) from light, sound, CO2 and motion readings, in sizes from 636 to 33k weights. They were trained on a public data set for the MSPM0G3507, and the same C source and 20 test vectors run on every board in this project, so their cycle counts compare chips directly. On this chip they take 1.8 to 65 ms from flash as plain C..

### Can TI's Neural Network Compiler (NNC) build my model for the F2800137?

Yes, on Linux or Windows: `tvmc compile --target="c, ti-npu type=soft" --target-c-mcpu=c28` with `--cross-compiler="cl2000"` and the options in TI's NNC user's guide, F28x section. This chip has no NPU, so use `type=soft`. NNC 2.1.2 has no Intel Mac build, so it ran on GitHub's Linux machines with the C2000 compiler 25.11.1.LTS for Linux. Place `.rodata.tvm` in flash and `.bss.noinit.tvm` in RAM in the linker file. On the four occupancy models from flash, NNC's code took 38 to 53% fewer cycles than the fastest plain-C build of each (`-O4 --opt_for_speed=5`), and 27 to 29% fewer from RAM on the two that fit there, with every test vector right.

### Why are NNC's memory sizes for the C28x half what the linker map shows in bytes?

NNC counts 16-bit words and calls them bytes: double NNC's figures on the C28x. For occ_cnn_500 NNC prints `Code: 1647 bytes`; the linker map lists 1,647 for the same library in its own unit, 16-bit words, which is 3.2 kB..

### Does NNC's code need more memory than plain C, and where should it run from?

More memory, and run it from flash. Against the plain C builds, NNC's code is 2.3 to 5.4 kB bigger and its buffers 0.5 to 8 kB bigger. For occ_cnn_2k it was slower from RAM (357k cycles) than from flash (312k): in the RAM build the weights, buffers and most of its code landed in the same RAM block, and the CPU probably stalls when it fetches code and reads data from one block in the same cycle. Another RAM layout was not run.

### Why was the washing-machine AI example's feature extraction not timed?

The debugger never stopped at its breakpoint on `FE_runFeatureExtract` within 120 s, although the example passes. Not traced; its linker file copies code to RAM, which may move the function after the breakpoint is set. The model call was timed (24.9 ms).

## The ADC

### Can I sample the F2800137's ADC with an 8 or 9-cycle window?

9 cycles yes, 8 no: the data sheet's minimum sample window is 75 ns, which is 9 cycles at 120 MHz. Data sheet SPRSP63B ("ADC Operating Conditions") gives the 75 ns minimum for a source resistance of 50 Ω or less, and a maximum rate of 4 Msps at 120 MHz. Measured on this bench: 3.98 Msps at 9 cycles and 4.12 Msps at 8, so the 8-cycle row is outside the specification. TI's continuous example uses 36 cycles (2.10 Msps). A sensor with a higher source resistance needs a longer window. Accuracy was not measured.

### Why does TI's demo read about 2,000 on pin A6 but your ADC-rate test about 115?

Unknown. Both read pin A6 with nothing connected, and a floating pin has no defined voltage. The two programs sample it differently: TI's demo converts once a second with a 10-cycle window and the ADC clock at SYSCLK/4 (its source, `launchxl_ex1_f2800137_demo.c`); the ADC-rate test converts back to back with 16 chained conversions and the ADC clock at SYSCLK/2. The leftover charge on the sampling capacitor probably sets the reading, but which setting causes the difference was not tested. Connect a known voltage before trusting any reading.

## Libraries written for 8-bit bytes

### Why does a library written for Arm fail on the C28x with `error #20: identifier "uint8_t" is undefined`?

Define `uint8_t` yourself before the library's headers: `typedef unsigned char uint8_t;` (16 bits on the C28x), then check the code never relies on a byte wrapping at 256. The C28x cannot address a single byte, so the C2000 compiler's `stdint.h` leaves out `uint8_t` and `int8_t`. u8g2 2.37.1 printed `"csrc/u8x8.h", line 215: error #20: identifier "uint8_t" is undefined` and 2,701 errors after it; TI's CC1407P host driver printed the same error 445 times from `TRX.h`. C2000Ware's `typedef uint16_t uint8_t;` also clears the error, but breaks u8g2's fonts (next answer). C2000 compiler 25.11.1.LTS, 2026-10-08: [u8g2 experiment](../docs/experiments/2026-10-08-f2800137-u8g2.md), [CC1407P host experiment](../docs/experiments/2026-10-08-f2800137-cc1407p-host-driver.md).

### Why does u8g2 fail on the C28x with `error #145: a value of type "char [772]" cannot be used to initialize an entity of type "const uint8_t [772]"`?

Define `uint8_t` as `unsigned char` (16 bits on the C28x) for u8g2's files instead of using C2000Ware's `typedef uint16_t uint8_t;`; with that, u8g2 2.37.1 compiles and links unchanged. u8g2 stores its fonts as string literals (all 2,174 in 2.37.1's two font files, counted; two were compiled), and C lets a string fill only an array of a character type. Without any `uint8_t`, u8g2 fails earlier with `identifier "uint8_t" is undefined`. Fonts and the frame buffer then take one 16-bit word per byte, twice the memory of a byte-addressed chip. Compile and link check only, C2000 compiler 25.11.1.LTS, 2026-10-08: [u8g2 experiment](../docs/experiments/2026-10-08-f2800137-u8g2.md).

### Can TI's BP-CC1407P host driver run on the F2800137? It fails with `error #1167: invalid attribute for "struct TRX_Phy_s"`

Not as it is: even with C2000Ware's `uint8_t` and `-D__packed__=` on the command line, which let all 7 portable files compile, 11 of its 12 message structs come out a different size than the bytes on the SPI wire. The driver lays its structs straight over SPI buffers, which works only where a byte is 8 bits; a C2000 host needs code that packs each message byte by byte, or a small byte-addressed host (an MSPM0) between the C2000 and the radio. Compile check only, TI Zephyr fork v3.7.0-ti-1.00.01_ga, 2026-10-08: [CC1407P host experiment](../docs/experiments/2026-10-08-f2800137-cc1407p-host-driver.md).

### Why does the C2000 compiler print `warning #64-D: shift count is too large` on code that builds clean on Arm?

`int` is 16 bits on the C28x, so `1 << 16` and above overflow; write `1UL << n` (or `1ULL`) for any flag at bit 16 or higher. The code assumes a 32-bit `int`: TI's CC1407P host driver defines 15 event flags as `(1 << 16)` to `(1 << 30)` and draws 29 of these warnings with the C2000 compiler 25.11.1.LTS; the same rule is why BitNetMCU needed `1L << shift` above. What the driver's build does with those flags at run time was not checked. [CC1407P host experiment](../docs/experiments/2026-10-08-f2800137-cc1407p-host-driver.md).

### Why does uLisp run on the C28x but print garbage numbers, return empty strings or get `(tak 12 8 4)` wrong?

The sketch assumes a 32-bit `int`, an 8-bit `char` and a 4-byte `sizeof` of a packed word, and the C28x has none of them, so a build that only fixes the compile errors boots, answers `(+ 1 2)` and fails the rest: `(/ 1.0 3)` printed `0.0-+/*//`, `"abc"` came back empty, integers wrapped at 16 bits, and `1-` collided with another name in the radix-40 symbol packer. Widen every `int` to `long`, cast before `<< 16` and `<< 24`, replace `sizeof` of the packed word by 4, and set `INT_MAX` to the 32-bit limit (`ports/ulisp/boards/f2800137/build.sh` and `compat.h`); 19 of 19 REPL checks then pass and `(tak 18 12 6)` takes 8.1 to 8.2 s (run 2026-11-09, `c2000-launchpad/bench/ulisp/`).

### Why does a flash image written from a linked `.out` print nothing on the C28x?

Two things are easy to miss: the codestart branch has to be at the flash entry, 0x80000 (a linker file that keeps `BEGIN` at 0x0 leaves the flash entry blank), and the `.TI.ramfunc` code (`Flash_initModule` and the other flash helpers) has to be copied to RAM before `Device_init` calls it. Put `BEGIN` at 0x80000 and start `main` with `memcpy(&RamfuncsRunStart, &RamfuncsLoadStart, (size_t)&RamfuncsLoadSize);`; the banner `uLisp 4.9a` then prints after a reset (run 2026-11-09, `ports/ulisp/boards/f2800137/PORT.md`). Write the image as a raw binary the way `c2kflashback.js` does: CCStudio's `loadProgram` of the same `.out` timed out twice.

## Power measurement and ported examples

### Can I measure the F2800137's current with the kit's own probe, as the MSPM0 LaunchPad does with EnergyTrace?

Not with its own probe; borrow one. The LAUNCHXL-F2800137's XDS110 is a plain debug probe without EnergyTrace hardware: the user guide SPRUJ32 never mentions EnergyTrace, CCStudio's XDS110 driver answers -260 when `tools/energytrace/etcap.py` asks to open probe <probe serial> in EnergyTrace mode, and opened plainly the probe reports hardware id 0x2F, `ET_HardwareInfo` -670, EnergyTrace type 0 (the LP-MSPM0L1306's probe answers 0, type 2, and streams). Leaving EnergyTrace off a motor-control board is a fair choice, but nothing says so. Two wires from another kit's EnergyTrace probe to the chip side of JP2 make that probe the meter; the first catch is the pin, below, and the second is that the probe is only the meter while it is wired. A bench meter in place of the JP2 +3V3 shunt is the cross-check.

### I power the F2800137 from another board to measure it, and now CCStudio and DSLite say "Error -2131 Unable to access device register". What now?

Flash first, then rewire. With the chip side of the board on outside power (the JP2 +3V3 shunt out, a wire on the chip-side pin) the chip boots and runs, but the board's own XDS110 cannot connect: the scripting runner and DSLite both stop at `IcePick_C_0: Error connecting to the target: (Error -2131 @ 0x0) Unable to access device register`, and a reset press does not help. Put the shunt back, take the wire off, flash, then undo both with the capture already running. One image can carry every phase you want to measure:.

### My STANDBY current on the F2800137 is the same as IDLE. Is STANDBY broken?

No. You re-entered STANDBY too soon after the watchdog wake. The technical reference manual SPRUIX1B section 3.9.4 says, for IDLE, that software must wait for the watchdog's WDINT line to go high again (512 WDCLK cycles) before re-entering; it does not say so for STANDBY, but the rule applies there too. Re-entering inside that window left the chip in IDLE at 29 mA; a 30 ms wait after the wake interrupt put it in STANDBY at 8.7 mA between wakes, 2.6 mA above HALT.

### Why does the F2800137 read 6.8 mA in HALT when the data sheet promises far less?

Because that reading is not the chip alone. The first profile image entered HALT with the PLL still powered, and the technical reference manual SPRUIX1B section 3.10.4 says to power the PLL down first; with the PLL and both internal oscillators off, HALT reads 6.1 mA, only 0.7 mA less. The rest is everything on the chip side of JP2 that draws with no program running: the CAN transceiver in normal mode, the LED1 power indicator, the encoder buffer. Until a meter on the transceiver splits it, the power rows for this board are quoted "above the HALT reading", not as absolute chip figures.

### Which JP2 pin is the chip side when I wire in outside power?

The lower pin of the +3V3 column, as drawn on the board silkscreen, and it worked on the first try. It was not confirmed with a meter; the check is that LED1 lights only when the wire is on the chip-side pin with the shunt out.

### Why does a ported F280015x example fail with `fatal error: cannot open source file "inc/stw_dataTypes.h"`?

The example uses CAN FD (MCAN), and the F2800137 has only the classic DCAN controller, so the F280013x driverlib has no MCAN driver or headers. Use the DCAN examples under `driverlib/f280013x/examples/can` instead. Eleven of the 129 F280015x examples stop here.

### Why does a ported example fail in SysConfig with `cannot set '$assign' to 'EPWM2': No option named EPWM2 defined, valid options are Any EPWM1`?

It asks for high-resolution PWM on a second ePWM module. SysConfig offers high resolution on ePWM1 only for this chip. The data sheet says two channels have high resolution; which was not checked. Deleting the fixed pin lines lets SysConfig place two of the nine examples; seven still need a second high-resolution module.

### Why does a ported example fail with `cannot set '$assign' to 'GPIO25'` or `'GPIO30'`?

The F2800137 does not have those pins: neither is in the F280013x `pin_map.h` or on the 64-pin package. Pick another pin in the `.syscfg` file; for the four ePWM digital-compare examples the pin also sits on an input crossbar line, which needs the same change by hand.

### Why does a ported LIN example fail with `identifier "DEVICE_GPIO_CFG_LINTXA" is undefined`?

The F2800137 has no LIN port. There is no port to make; the three LIN examples do not apply.

### Why does `ecap_ex4_sw_sync` fail with `cannot set 'ecapBase' to 'ECAP3_BASE'`?

The F2800137 has two eCAP modules, the F280015x three. Change the third capture to ECAP1 or ECAP2 if the example's logic allows it.

### BitNetMCU on the C28x answers `8888808888` for every digit. What is wrong?

The test digits are stored upstream as hex bytes in a signed array; on a machine where `char` is 16 bits, `0xEC` reads as 236 instead of -20, so every input is wrong and every answer is 8. Write the digits as signed decimal values (`ports/bitnetmcu/c2000/make_test_data.py` does it) and the answers come out `3209069277`, the same as on a Mac.

### BitNetMCU on the C28x: why `1L << shift` instead of `1 << shift`?

`int` is 16 bits on the C28x, so `1 << 15` and above overflow. The `L` makes the shift 32-bit. On the test digits the largest shift is 7, so this patch changes no answer; it stops a wrong one on other inputs.

### BitNetMCU on the C28x: why does one buffer carry `aligned(2)`?

The inference code reads the same buffer as 8-bit and as 32-bit words. On the C28x a 32-bit read needs an even word address. The compiler placed it on one anyway; the attribute makes that certain across builds.

### tiny-AES-c gives the wrong ciphertext on the TMS320F2800137, and FIPS-197 C.1 fails. Why?

Mask the result of tiny-AES-c's `xtime()` to 8 bits (`& 0xFF`); with that one line changed, all 9 checks in this repo's test pass on the chip. tiny-AES-c expects a `uint8_t` to drop its top bit when it shifts left, which is true on an 8-bit-byte chip. On the C28x the smallest type is 16 bits, so the bit stays, the next S-box lookup reads past the end of the table, and the cipher comes out wrong. A Mac build with a 16-bit `uint8_t` shows the same failure and the same fix. This repo's `build.sh` patches a copy and leaves the vendored source untouched. TI C2000 compiler 25.11.1, tiny-AES-c commit 2385675; results in.

### TI's CC1407P host driver fails on the C28x with `uint8_t` undefined and `invalid attribute for "struct TRX_Phy_s"`. How do I build it?

Pass `portability/cc1407p-host/c28x/c28x_compat.h` with `--preinclude`: it defines `uint8_t` as a 16-bit `unsigned char` and `__packed__` as empty. All files then compile with no change to TI's code. The build is not the end: run on the board, the driver's first SPI message is 9 units against the Arm build's 10 bytes, because structs laid over the SPI buffer take a 16-bit word per member. If you see `TRX_Host_open` never return, that is why; write a field-by-field packer (`portability/cc1407p-host/c28x/README.md`).

### Why does the CC1407P host driver's `LRF_*_byteCount` come out half as big on the C28x?

It is `sizeof` of a `uint32_t` array, and `sizeof` counts 16-bit words here. A 68-entry table reports 136, not 272. Use 4 octets per entry with `portability/cc1407p-host/wire/trx_wire.c` (`trx_w_words_to_octets`), which also packs every other message field by field; 17 of 17 cases equal TI's structs on a Mac and on this board (run 2026-10-25, `docs/experiments/2026-10-25-f2800137-cc1407p-wire-packer.md`).

### Does the portable three-ways app run here without editing it?

Yes, with one preinclude. `app.h` uses `uint8_t`, which the C28x `stdint.h` omits; `portability/three-ways/c2000/c28x_preinclude.h` supplies it for `app.c` only (C2000Ware's driverlib headers already define `uint8_t`, so the glue file must not get it). 35 glue lines, 29 config lines, 4.9 kB code, 1.0 kB RAM, serial check passes at 115200 (`portability/three-ways/c2000/README.md`).

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-24. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

**LAUNCHXL-F2800137**

No lockup is recorded for this board.

**LAUNCHXL-F28P65X**

No lockup is recorded for this board.

<!-- recovery:end -->
