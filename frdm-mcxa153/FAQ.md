# NXP FRDM-MCXA153 FAQ: errors on a Mac and their fixes

Each answer starts with the fix and names the cause. Board manual: NXP UM12012 Rev. 2.0. Checked on an Intel Mac, macOS 15.7.9, 2026-10-11 and 2026-10-12.

Terms: [docs/vocab.md](../docs/vocab.md).

**What caused it**, one tag per answer: Our mistake, Setup or handling, Tool or docs, Chip limit, Third-party behavior, Unknown (same tags as every board FAQ here).

### The board is plugged in and the Mac sees no debugger. Why?

Use the top USB-C port, J15 (labelled MCU-LINK). The board has two USB-C ports. J8, just below, is the MCXA153 chip's own USB port: it powers the board, so the LEDs light, but it has no debugger and no serial port. UM12012 Table 3 lists J15 as "MCU-Link USB connector" and J8 as "USB full-speed connector".

### `west update_board` stops with `No such file or directory: 'west'`. Why?

Put the SDK's virtual environment first on `PATH` (`export PATH=~/mcuxsdk/.venv/bin:$PATH`) and run it again. The command starts `west` by name from the shell, so calling `~/mcuxsdk/.venv/bin/west` by its full path is not enough.

### `pip install -r mcuxsdk/scripts/requirements.txt` fails building `cbor2`. What now?

If pip ends with `Failed to build installable wheels for some pyproject.toml based projects` and names `cbor2`, run `pip install --only-binary=cbor2 cbor2` first, then the requirements again. On Python 3.14 for an Intel Mac pip tries to build `cbor2` from source and needs a Rust compiler; a prebuilt wheel exists and the requirements file accepts it.

### `pyocd` says `Target type mcxa153 not recognized`. Why?

Install the pack: `pyocd pack find mcxa153`, then `pyocd pack install MCXA153VLH` (the first `find` downloads the descriptors and takes about 5 minutes; the pack itself is 1.3 MB, and pyOCD with the pack is about 110 MB on disk). pyOCD has no built-in support for this chip. If you clean the pack folder (`~/Library/Application Support/cmsis-pack-manager`), keep `index.json` and `aliases.json` or the target disappears again.

### Can OpenOCD flash this board?

Use pyOCD. xPack OpenOCD 0.12.0 connects through the MCU-Link as a generic Cortex-M33 (`adapter driver cmsis-dap`, `adapter serial <MCU-Link serial>`) and reads the memory-mapped flash correctly (two reads of 128 kB were identical), but it has no flash driver for this chip's flash controller, so writing with it was not run.

### `west build` says `doesn't contain a CMakeLists.txt` for some examples. Is something wrong?

No: those entries are folders that hold several examples. In `examples/driver_examples`, `pwm` and `wwdt` hold sub-folders such as `pwm_3ph`; and the five `se05x_*` folders are not projects. Build a sub-folder or pick another example.

### The blink was flashed and the LED did not change. What now?

Check the cable is in J15, not J8, and that the build you flashed is the one you edited. Read the pin: with the board running, the red LED pin is bit 12 of the register at 0x40105040 (GPIO3 output), which toggles at the blink rate; `pyocd commander -t mcxa153 -c "read32 0x40105040"` twice, a moment apart, shows it change. If the build step printed no error but the LED is unchanged, rebuild with `-p always` (pristine) so the old image is not reused.

### Two NXP examples fail to build: `safety_iec60730b` and `freertos_tickless`. Is the setup wrong?

No. `demo_apps/safety_iec60730b` stops on `error: ... may be used uninitialized [-Werror=maybe-uninitialized]` inside NXP's middleware, and `freertos_examples/freertos_tickless` stops on `fatal error: tickless_api.h: No such file or directory`. Both fail the same way with GCC 12.2 and with Arm GNU Toolchain 14.2.Rel1. Skip them, or build the first with `-Wno-error`.

### Can you install NXP's flasher, LinkServer, instead of pyOCD?

The installer could not be downloaded. On 2026-10-12 the macOS download link on NXP's LinkServer page (26.9.130, 32,144 kB) answered with a redirect to NXP's login page (`https://www.nxp.com/webapp/ruhp.login.framework`), and the older direct file paths NXP's staff blogs show (`https://www.nxp.com/lgfiles/updates/mcuxpresso/LinkServer_<version>.x86_64.pkg`) returned 404 for 26.9.130, 25.7.33 and 24.9.75. A free NXP account is the likely way in; no account was created. The page describes a macOS package installer; it was not run. pyOCD with the NXP pack needs neither and flashed and read the board every time.

### `./prebuild.sh` prints `prebuild: Mac is busy` and stops. What now?

Run `IGNORE_LOAD=1 ./prebuild.sh`. The script refuses to start when the Mac's one-minute load is at or above its core count, so a build does not distort a timing run on the same Mac. Cycle counts on the chip do not depend on the Mac's load.

### The CoreMark number is below NXP's 381 per second. Is something wrong?

Probably not. Measured: 282 per second (2.94 per MHz, 74 percent of NXP's 381) with GCC 12.2 and 275 (72 percent) with the Arm GNU Toolchain 14.2. The data sheet does not name the compiler or flags. Suspected, not tested: a different compiler or benchmark build.

### The CMSIS-DSP q31 FFT is faster than q15. Is that a mistake?

No. Suspected, not tested: this Cortex-M33 has no DSP instructions, so the library's q15 code cannot use its packed 16-bit multiply-accumulate (256-point complex: q15 31.6k cycles, q31 24.3k).

### Why does FreeRTOS stop at nine tasks?

NXP's example gives FreeRTOS a 10 kB heap and each minimal task takes about 1 kB; raise `configTOTAL_HEAP_SIZE`. The 10 kB heap is NXP's example default; suspected, not tested: the chip's 32 kB of RAM is the next limit.

### Does the chip have AES or a random-number generator?

No. The data sheet's security list is a CRC unit only, and the SDK's driver list for this board has `crc`, `cdog` and `glikey` and no crypto driver.

### All examples were built with a script and `trdc/basic` faults. Is something wrong?

No. That example resets or faults on purpose to show the memory-protection unit working.

### Zephyr will not build for this board: `hal_nxp` is missing. Why?

Run `west update hal_nxp` in your Zephyr checkout (about 1.3 GB, 72 s here). NXP's hardware layer for Zephyr is a separate module that Zephyr does not clone by default.

### A custom CoreMark image prints "CoreMark running" and never the result, and the debugger shows a bus fault. What is wrong?

Give the program a larger stack: add `-Wl,--defsym=__stack_size__=0x1800` to the link line. The SDK's linker script reserves 2 kB of stack by default, and CoreMark's data is kept on the stack. Also, the `nano` C library's `printf` has no `%llu`: format 64-bit numbers by hand, or the arguments after it shift and the program faults.

### Why is Embench split into six images?

 The SDK's linker script gives the program 24 kB of the 32 kB RAM (the other 8 kB is SRAMX), and Embench's benchmarks share global names and RAM, so they are built in groups; `huffbench` needs about 8 kB of stack alone. All 19 pass.

### Where is Zephyr's CoreMark?

In the results page: 251 per second at Zephyr's speed setting and 224 at its size setting. The shared Zephyr benchmark app includes TI headers, so this board uses a small app of its own (`frdm-mcxa153/zephyr-coremark`) with the same EEMBC sources.

### Why is the ADC at 2.5 million samples a second and not the data sheet's 4 million?

 Measured: 2.53 million at a 48 MHz ADC clock with the shortest sample time and no averaging, which is 19 ADC clocks per 12-bit conversion. Suspected, not tested: a faster ADC clock reaches the data sheet's figure. Neither a faster clock nor the DMA as the limit was tried.

### Does BitNetMCU's digit recognizer run on this board?

Yes. All four of its models read 10 of 10 test digits with the inference code unchanged ([experiment](../docs/experiments/2026-10-11-frdm-mcxa153-bitnetmcu.md)). The Zephyr CoreMark (251 per second) is below the bare-metal figure (264) at the same speed setting; the difference is not explained (suspected, not tested: Zephyr's kernel timer interrupts and its library).

### Did you try MCUXpresso for VS Code?

Only the extension, not a first blink. `code --install-extension NXPSemiconductors.mcuxpresso` installed version 26.9.44 in 6 seconds with no login: 4.5 MB for the extension and about 30 MB with the three extensions it pulls in (file downloader, memory inspector, serial monitor). It was not opened, so it is unknown whether it fetches its own SDK or LinkServer, or whether it asks for a login at that point; NXP's docs describe LinkServer as its debug server. The first-blink time through NXP's IDE is not measured.

### Why are 57 examples marked skipped?

 They need hardware that was not wired: a second board (18), an SE05x secure element (5), a cable in the chip's own USB port J8 (32, built only) or a motor shield (1), per the plan; the 57th is `safety_iec60730b`, which did not build. Four more (power-mode, tickless, two over-the-air examples) were held back because they can drop the debugger or change the flash layout; the power-mode one was not built.

### The UART and ADC loop tests have no result. Why?

 They need a jumper wire from J1 pin 2 to pin 4 and a resistor and capacitor from J2 pin 4 to J4 pin 2 and ground; the board was run without wires. The results file lists them as waiting.

### Is the fixed-point motor-control result comparable to the float one?

No. The 448-cycle RTCESL step is a timing only; its output was not compared with the float step's, and the two use different number formats.

### Does the chip have a floating-point unit or DSP instructions?

No to both. The data sheet (MCXAP64M96FS3 Rev. 5.1) describes a Cortex-M33 with no FPU and no DSP extension, so every float operation is a library call (5.4k cycles for one control step in float) and signal-processing code cannot use the packed multiply-accumulate instructions. NXP's fixed-point RTCESL library avoids the float cost (448 cycles).

### The Arm GNU Toolchain link fails or the download will not run on a Mac. Why?

The link in the README is the Intel Mac (x86_64) build. Apple silicon was not run; Arm publishes a separate `darwin-arm64` build on the same page.

### Why do power and the ADC-triggered control loop have no result?

 Power needs a series meter that was not on the bench. The ADC-triggered control loop program is built, but running it left the chip refusing debugger connections. Neither is a chip fault; the results file lists each as waiting or not run.

### pyOCD says `SWD/JTAG communication failure (WAIT ACK)` or `(FAULT ACK)` after flashing a custom program, and unplugging does not help. Is the board dead?

No. Put the chip in its ROM bootloader and reprogram it over the serial port: hold SW2 (ISP), press and release SW1 (reset), let go of SW2, then use NXP's `blhost` from SPSDK on the MCU-Link serial port at 57,600 baud. The ADC-triggered test program left the debug port answering `FAULT` or `WAIT` at every power-up, so pyOCD and OpenOCD could not connect, even under reset; which part of the program did it was not found. In ROM bootloader mode the debug port still refused, but the bootloader (version K3.2.0) answered `blhost` (checked 2026-10-11):

```text
blhost -p /dev/cu.usbmodemXXXX3,57600 flash-erase-all
```

```text
blhost -p /dev/cu.usbmodemXXXX3,57600 write-memory 0 your-image.bin
```

```text
blhost -p /dev/cu.usbmodemXXXX3,57600 reset
```

SPSDK installs with `pip install spsdk` in a Python 3.12 virtual environment (197 MB). After the reset the factory image ran and pyOCD connected again.

### How do you see the serial output of an example?

Open the board's serial port at 115200 baud, 8N1: `ls /dev/cu.usbmodem*` shows one named with the MCU-Link serial (here `/dev/cu.usbmodemJRICGYFOI2WUA3`), and `screen /dev/cu.usbmodem<your serial>3 115200` opens it. The USB cable must be in J15; J8 has no serial port.

### The build stops with `CMake Error ... ***Please set ARMGCC_DIR in environment variables***`. What now?

Set `ARMGCC_DIR` to the toolchain folder that contains `bin/arm-none-eabi-gcc` (step 2 of the README) in the same terminal you build from.

### `pyocd` says `No available debug probes are connected`. What now?

Check the cable is in J15 and run `pyocd list`; the board shows as `NXP Semiconductors MCU-LINK FRDM-MCXA153`. J8 alone shows no probe.

### Is a 7x CRC speed-up and 0.835 DMIPS per MHz good?

They are measured, not compared with a target. The CRC unit is fed by the CPU word by word (the G3507 gets 13.5 times). Dhrystone was run with `-fno-inline` and its rules followed, as on the other boards; CoreMark is the headline number because Dhrystone is easy to game with inlining and library choices.

**Mistakes to avoid:**

- The core was labeled "r0p4 class" from a document; the chip reads r1p0.
- The shared benchmark harness printed 64-bit numbers with `%llu`, which this C library does not support, and used a 2 kB stack, which CoreMark overflows (see the entry above).
- The first example-building loop treated every folder with a `prj.conf` as a project; seven are not.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-24. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- An ADC-triggered test program left the debug port answering FAULT or WAIT at every power-up (pyOCD SWD/JTAG communication failure), even under reset (`frdm-mcxa153/FAQ.md`, 2026-10-11)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| Unplug and replug USB | hands | failed | `frdm-mcxa153/FAQ.md`, 2026-10-11 |
| Connect under reset (pyOCD, OpenOCD) | remote | failed | `frdm-mcxa153/FAQ.md`, 2026-10-11 |
| Hold SW2 (ISP), press and release SW1 (reset), let go of SW2, then reprogram with blhost over the MCU-Link serial port at 57,600 baud (ROM bootloader K3.2.0 answered blhost; the debug port still refused) | hands | proven | `frdm-mcxa153/FAQ.md`, 2026-10-11 |

<!-- recovery:end -->
