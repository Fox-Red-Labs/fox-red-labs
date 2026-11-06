# TI LP-AM13E230 LaunchPad development kit: errors, causes and fixes

Errors and fixes from the AM13E2x SDK 26.01.00.03.STS on an Intel Mac, checked on the board, with TI's public user guide SLVUDH9. Each answer starts with the fix. 1 kB = 1,024 bytes.

**What caused it**, one tag per answer: Our mistake, Setup or handling, TI example default, Tool or docs, Chip limit, Third-party behavior, Unknown.

## Building and connecting

### The SDK's make command cannot find CCStudio or SysConfig on a Mac. What do you pass?

Pass the real paths, or build the way `am13e230/prebuild.sh` does. The SDK's `make/host_os.mk` treats macOS (Darwin) as Linux, and `imports_am13e230x.mk` then looks for CCStudio in `~/ti/ccs2100/ccs` and SysConfig in `~/ti/sysconfig_1.28.0`. TI's Mac installers put them in `/Applications/ti`. The SDK also builds through CMake and Ninja, and CMake is not part of CCStudio. On this Mac: `CCS_PATH=/Applications/ti/ccs2101/ccs`, `SYSCFG_PATH=/Applications/ti/sysconfig_1.28.0`, and SysConfig's own Node.js at `/Applications/ti/sysconfig_1.28.0/nodejs/node`. Written from the SDK files, not seen on a board.

### A new LP-AM13E230 shows no blinking LED and prints nothing. Is it broken?

No. Load a program and it will run. The board ships with empty flash, so only the power LEDs light until you load something. On a revision E2 board (MCU178E2 on the back) all 512 kB of main flash read twice as 0xFF in every byte. The 8 kB NONMAIN area held data, which was saved. The serial port stayed silent for 5 seconds after power-up, as expected with no program. The debug probe works the whole time, so flash an SDK example over USB and the LED blinks. Checked 2026-10-08 with AM13E2x SDK 26.01.00.03.STS and probe firmware 3.0.0.41.

### Every read of an LP-AM13E230 fails with error -614 (or -615). What do you do?

Unplug the USB cable, plug it back in, and read again. After a debug connection in CCStudio where `restart` was sent while the target was still running, the debug port stops answering until the board is power-cycled over USB. To avoid it, read the target state after loading, `pause` if it says Running, and only then `restart` and `continue`. The program already sits in flash, so the board runs it after the replug. Seen 2026-10-08 with CCStudio 21.0.1 and probe firmware 3.0.0.41. A second cause, found 2026-10-10: a connection file you write or copy yourself can fail with -615 on every try, even after a replug, while `am13e230/AM13E23019_AL130001.ccxml` works. Setting the property `SWD.SWJ_DP_DEFAULT_MODE` to 0 in your own file clears it. Use this repo's file (change the serial) or set that property.

## Running and measuring

### TI's CoreMark example for the LP-AM13E230 prints nothing and the score is not shown. Where is it?

Send `printf` to a RAM buffer and read the buffer with the debugger. The example's `ee_printf` is the C library `printf`, which writes only through the debugger's console, and its timing variables are file-local. A 15-line `ee_printf` replacement that appends to a RAM buffer keeps TI's other files unchanged. TI's example also sets 1k iterations, a run of 1.15 s; the script raises it to 9,000 so a run lasts about 10 s, as CoreMark requires. Result on this board: 871.6 per second at -O3 (data sheet: 870). Checked 2026-10-08. The build uses `-fno-builtin-printf` so that CoreMark's closing "Correct operation validated" line (a plain `printf("...\n")` the compiler would turn into `puts`, which skips the buffer) reaches the buffer; read 2 kB (`cm_log:2048`).

### Which header pins carry the outputs of the motor PWM unit MCPWM4?

All six are on BoosterPack header J4: 1A on pin 40 (PB3), 1B on pin 39 (PA7), 2A on pin 38 (PB1), 2B on pin 37 (PA2), 3A on pin 36 (PB2) and 3B on pin 35 (PA25). Source: the user guide SLVUDH9 Figure 2-9 (BoosterPack header pinout), and the SDK's `examples/driverlib/mcpwm/mcpwm_basic_pwm_generation/README_AM13E230X.md` lists the same pins. The stepper and L293D demos use the three A outputs. The user guide's Table 3-2 shows 1B, 2A and 3B routed differently on a revision E1 board. Checked 2026-10-10 against three sources, not with a scope.

### A program does not start after DSLite flashes it, or stops when memory is read. What do you do?

Flash with a system reset and run, and read once after the program should be finished. `DSLite flash -c <ccxml> -e -f -v -r 1 -u -t 60 <image.out>` started the program every time on 2026-10-08. A DSLite memory read connects with halt-on-connect, and on this board it sometimes left the core halted, so a loop that polls a result word can end the run. `am13e230/tools/run_image.py` flashes this way, waits, then reads once. Do not stop a DSLite process that you did not start: a killed one can hold the probe and the next call fails with error -260 until it is gone.

### The CoreMark score drops 30 percent at -O2 against -O3. Is something wrong?

No. On the AM13E230 with TI Arm Clang 5.1.1.LTS the same files give 872.4 at -O3, 612.7 at -O2, 477.1 at -Os and 438.1 at -Oz, and TI's own port gives the same four figures within 0.1 percent, so it is the compiler and not the port. Quote the flag with the score.

### Can you spin a motor from this board without a motor BoosterPack?

Yes. The Freenove kit's 28BYJ-48 stepper on its ULN2003 board runs from four output pins (J4 pins 40, 38, 36, 34), 5 V from J3 pin 21 and GND from pin 22: `am13e230/stepper-uln2003/`, `./build.sh --flash`. The kit's DC motor through its L293D chip in the breadboard uses two MCPWM outputs instead (`am13e230/motor-l293d/`, program built, not run on a motor). TI's own motor examples all need a BOOSTXL-DRV8323RH or DRV8329A BoosterPack and a three-phase motor. If the stepper buzzes without turning, two of the four signal wires are swapped.

### The SDK's mathlib/fastrts example stops with a usage fault (CFSR 0x80000). What do you do?

Add a `_system_pre_init` function that turns the TMU on, or build the example at -O0. CFSR 0x80000 means an instruction for a coprocessor that is off. The SDK sets the TMU access bits in `Device_Init()`, but the compiler treats the TMU intrinsics as pure and, when their inputs are constants as in this example, moves them to the top of `main()`, before `Device_Init()`. The faulting address, read on 2026-10-10, was inside `main()` with CPACR still at 0x00F00000 (FPU on, TMU off). Putting `SCB->CPACR |= 3` as the first line of `main()` does not help, because the compiler moves the instructions above it as well. This works at -O2:

```c
int _system_pre_init(void)   /* the C startup calls this before main() */
{
    SCB->CPACR |= (3UL << 0);  /* CDE coprocessor 0, the TMU */
    __DSB(); __ISB();
    return 1;
}
```

With it the example runs with CFSR and HFSR at zero. The same fix applies to any program that calls the TMU before `Device_Init()` returns. Checked with AM13E2x SDK 26.01.00.03.STS and TI Arm Clang 5.1.1.LTS.

### A flash write passes verify but the old program still runs, or the new one never starts. What do you do?

Clear the flash caches while the core is halted, then write and reset. A program that switched the flash caches on (TI's examples do at 160 MHz) leaves old flash lines in them. The debugger reads flash past the caches, so verify passes, and the reset then fetches the old vector table from the cache. Clear `FRI FRD_INTF_CTRL` (0x4002900C: bit 1 data cache, bit 2 code cache) before the write. The LED toggles at the new image's rate after the reset. Seen 2026-10-22 on the AM13 and MSPM33 chips: the PC sat inside the previous image after a correct write.

### `Failed: Target is not halted` appears at the end of a DSLite flash. Did the flash fail?

No. The line comes after `Program verification successful`: DSLite tries one more step after the program has already started running. The image is written and verified. Check with a pin read or 10 ms sampling of PA5 (LED4).

### The NNC build for the neural accelerator prints `GCONV layer NOT offloaded to TI-NPU`, or the `type=m33cde` build faults with `CFSR 0x100`. What do you do?

Use the NPU build or plain C. The GCONV message means that layer runs on the CPU, so the 30k occupancy model gains only 2.4 times instead of 4.7 to 8.8; expect it. The `type=m33cde` build (custom instructions on the CPU) does not compile cleanly with TI Arm Clang 5.1.1: NNC 2.1.2's generated code gives an incompatible-pointer-types error in `lib2.c`, and with that error turned into a warning the program takes an instruction bus error (`CFSR 0x100`, `HFSR` forced) before the first result. The cause was not chased.

## Zephyr

### `west build -b lp_am13e230 samples/basic/blinky` stops with `'__device_dts_ord_DT_N_ALIAS_led0_P_gpios_IDX_0_PH_ORD' undeclared here`. What is missing?

Zephyr's board for this LaunchPad kit has no LED, GPIO or serial driver, so there is no `led0` for blinky to use. Use `am13e230/zephyr/led-proof`, which writes the GPIO registers directly, or TI's own SDK example. Seen on Zephyr `main` (commit `11808e9ea`, 2026-10-10). `hello_world` builds for the board but prints nowhere, because the board file chooses no console. The 4.4.2 release has no `lp_am13e230` board at all.

### `west flash` for the LP-AM13E230 says the OpenOCD cannot find the chip or a flash driver. What do you use?

Build upstream OpenOCD from source: the OpenOCD in Zephyr's SDK and the xPack builds have no AM13E230 flash driver. On macOS run `brew install autoconf automake` first, then the steps in `am13e230/zephyr/README.md` (3.5 minutes). Built 2026-10-10 on an Intel Mac; flashing with it was not run.

## DRV8376EVM motor bench

### Why did selecting AVDD at J2 stop useful motor drive?

Set J2 and J4 together. With J2 on AVDD and J4 on DIV, the current-sense zero and the current-limit threshold are both nominally AVDD/2, so an AVDD-only change can leave the driver without usable drive. The auxiliary current-limit connection did not behave as though it were tied to the divider; a measured DAC reference on that input restored drive. TI's [DRV8376EVM guide](https://www.ti.com/lit/ug/slvud20/slvud20.pdf), figures 2-6, 2-8 and 4-2, identifies the pins.

### What happens if a voltage check fails, and does a pass prove the motor is repaired?

An out-of-range measured-reference check rejects the trial before drive enable and restores the reference configuration. Diagnose the failed preflight before another attempt. A passing voltage or a dark fault LED alone does not prove a repair. The recovery was checked by six current-response tests followed by two bounded trapezoidal runs with consecutive forward Hall travel and no fault.

### Can the TI SDK or InstaSPIN example run the DRV8376EVM directly?

Use the AM13 three-shunt Hall FOC example as the nearest controller-compatible starting point, with the DRV8376 board adaptation in `foc-drv8376`. The supplied AM13 examples target different power boards. TI's DRV8376EVM demo targets the F280049C controller, so its binary cannot run on the AM13. InstaSPIN/FAST and TI Tune are available in the AM13 SDK; they still require a working voltage/current and gate-driver interface.

### Why does the FOC port need a separate commissioning entry point?

 TI's normal startup enables the gate driver and performs calibration before the run command. The commissioning entry point keeps DRVOFF high and PWM forced low through calibration, then lets the run start only after the guard checks pass.

### Can you read the DRV8376 fault registers over SPI?

 Only the DRV8376S variant exposes SPI registers. The photographed bench chip is marked DRV8376H, whose settings come from hardware pins and resistors. Neither variant provides I2C. The H variant reports an aggregate nFAULT signal; it cannot return a register identifying the fault. See TI's [datasheet](https://www.ti.com/lit/ds/symlink/drv8376.pdf), sections 7.3.3 and 8.

### Did TI's FOC software run on this setup?

Yes, unloaded. TI initialization, comparator calibration and ADC offset calibration ran, and TI's three-shunt Hall FOC then drove the motor closed loop (one motor, 12.4 V, no load) on the Halls, and with the FAST and ESMO observers in place of them. Loaded starts, speeds below about 20 Hz and reversals are not run.

### Why are pulse commands rejected after sleep/wake clears nFAULT?

 Driver reset and MCU PWM trip latches are separate. The initial commissioning log had `fault=0` with `trip=458752`: the driver indication cleared while motor-stop, driver-fault and comparator latches remained set. Pulse commands reject retained fault trips. Leave drive stopped while diagnosing the original fault; clearing a latch alone is not evidence that its cause is gone.

### Why do phase A commands appear on the channel called C?

Use PA29 for phase A, PA30 for B and PA27 for C. TI's DRV8376EVM header table lists the A and C current-sense pins the other way round from its schematic. Six line-vector tests matched the schematic. Check the labels in any ported ADC code.

### Why did the old Hall table stall after the driver fault was resolved?

 It no longer matched the present phase/Hall relationship. A slow open-loop field sequence supplied a new candidate table, which then produced consecutive forward rotation in two bounded trials. The exact earlier physical action that changed the relationship is unproven. Keep the table with its wiring state.

### Why sample current after the short source pulse ends?

 The sink low-side transistor still carries recirculating winding current. Sampling at least seven microseconds after enabling it allows the driver's worst-case amplifier turn-on and settling delay. An early ADC conversion can be fresh while its analog source is not settled. The recorded maximum is a sampled recirculation-current deviation, not the instantaneous ON-current peak.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-24. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- Deep-sleep lockout: an image in STANDBY0 with only the watchdog interval timer as wake source never wakes; every connect fails (DSLite -613, -6310, -6311; OpenOCD -613; all access ports stalled behind DPIDR 0x6ba02477) (`ports/msp430-to-mspm0/lpm3-wake/PORT.md`, `docs/remote-board-recovery.md`, 2026-10-21)
- Boot fault after an interrupted write or an erased vector table (CFG-AP BOOTDIAG 0x08000007) (`am13e230/AGENTS.md`, 2026-10-22)
- restart sent while the target was running in CCStudio: error -614 on every read (`am13e230/AGENTS.md`, `am13e230/FAQ.md`)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| xds110reset toggle and assert (deep-sleep lockout: the target never printed again) | remote | failed | `ports/msp430-to-mspm0/lpm3-wake/PORT.md`, 2026-10-21 |
| uhubctl power-off of the probe port, 2 s, 20 s and 150 s (on two hubs; 15 s on the second) (the probe came back, the chip stayed stalled; this board most likely keeps power) | remote | failed | `ports/msp430-to-mspm0/lpm3-wake/PORT.md`, `docs/remote-board-recovery.md`, 2026-10-21 |
| Connect under reset, and a hand-written OpenOCD connection, within 3 s of the probe appearing; 80 fast connects in a row | remote | failed | `ports/msp430-to-mspm0/lpm3-wake/PORT.md`, `docs/remote-board-recovery.md`, 2026-10-21 |
| Unplug USB and replug (the probe enumerated, the chip still did not answer) | hands | failed | `docs/remote-board-recovery.md`, 2026-10-21 |
| Hold S5 (BSL invoke), tap S1 (nRST), release S5; then flash the restore hex and read the first 64 bytes back (for the deep-sleep lockout) | hands | proven | 2026-10-24 |
| Wait-for-debug (SPREC/DPREC0 sequence at AP 4 from CCStudio's own log) (for the boot-fault lockup only; it needs the CS_DAP, which the deep-sleep lockout does not give (Error -613)) | remote | proven | `am13e230/AGENTS.md`, `mspm33c321a/AGENTS.md`, 2026-10-22 |
| Unplug and replug USB after a restart sent while running (error -614 case) | hands | proven | `am13e230/FAQ.md`, `am13e230/AGENTS.md`, date not recorded |

USB hub ports recorded for this board:

- The probe's hub port power cut not checked, 2026-10-09. A 5 s cycle, then 15 s off, left the AM13 probe on the bus without enabling it (Error -260); the cable had to be pulled.
- The probe's hub port does not cut power, 2026-10-21. A 15 s power-off of the probe port: the hub reported the port off and the neighbor kept running, but the chip stayed stalled, so the board most likely kept power; the L1306 on its own port did restart.

Restore: DSLite flash of the restore hex; main flash backed up twice before a risky image (main-a.bin, main-b.bin identical). Regions: main flash. (`ports/msp430-to-mspm0/lpm3-wake/PORT.md`, `docs/remote-board-recovery.md`)

<!-- recovery:end -->
