# What goes wrong with the ST NUCLEO-U083RC on a Mac, and how do you fix it?

Problems found bringing up the ST NUCLEO-U083RC (STM32U083RC chip: Arm Cortex-M0+, 56 MHz, 256 kB flash, 40 kB RAM, hardware AES and random numbers; onboard STLINK-V2EC probe on USB-C) from a Mac terminal. Each answer starts with the fix and names the cause. The wiring entry comes from ST's user manual UM3256 Rev 2; the rest come from the first run on 2026-10-08, and the Arduino entry from 2026-10-09.

Terms: [docs/vocab.md](../docs/vocab.md). 1 kB = 1,024 bytes.

## Getting started and flashing

### The first `west build` stops with "Could not find a package configuration file provided by Zephyr-sdk". What is missing?

Tell Zephyr to use the GCC you have instead of the Zephyr SDK: `export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE=$HOME/zephyr-toolchain/arm-zephyr-eabi/bin/arm-zephyr-eabi-` before `west build`. The Zephyr install on this Mac has GCC 12.2 but not the full Zephyr SDK, so Zephyr's default search fails. Seen 2026-10-08.

### After ST's `FLASH_EraseProgram` example, OpenOCD says `Examination failed`. Is the board broken?

No. Wait a few minutes, or unplug the USB cable and plug it back in, then try again. In the run of all of ST's examples, the next 11 could not connect, and all of them loaded and ran on a second try a few minutes later. Suspected: the example's flash operations leave the chip unreachable for a while. Seen 2026-10-08.

### How do you put ST's demo back after flashing your own program?

Flash the copy of the demo you saved before your first program. Save it with OpenOCD (`-c "init; reset halt; dump_image demo.bin 0x08000000 0x40000; shutdown"`, using the same interface and target options as the flash command in the [README](README.md)), and put it back with `-c "program demo.bin 0x08000000 verify reset exit"`. The demo is not a project in STM32CubeU0, so without a saved copy there is no tested way back to it.

### Is STM32CubeIDE required?

No. Every result on these pages was built without it: Zephyr, `./blink.sh` with ST's command-line tools, and Arduino. STM32CubeIDE is ST's documented first path; it needs 6 GB of free disk, which this Mac did not have.

## Wiring

### TX/D1 is wired to RX/D0 for a serial loopback and nothing comes back. Why?

On this board, as shipped, the ARDUINO D0 and D1 sockets are not connected to the chip. Use D8 (pin PA9, USART1 transmit) and D2 (pin PA10, USART1 receive) instead, or close solder bridges. UM3256 Table 9 lists four solder bridges for D0 and D1: with SB23 and SB44 on they carry USART1 (PA9, PA10); with SB45 and SB48 on, USART2 (PA2, PA3). The default has all four off, and USART2 goes to the probe's serial port for the Mac instead ("USART2 from PA2/PA3 as VCP"). Not tested on the board: this is from UM3256 Table 9. The NUCLEO-G071RB differs: its D0 and D1 carry USART1 on PC5 and PC4 as shipped (UM2324 Table 12).

## Flashing and debugging

### OpenOCD stops with `stm32u0x.cfg:29: Error: invalid command name "swd"`. What now?

Choose the transport before the target file: `-f interface/stlink.cfg -c "transport select swd" -f target/stm32u0x.cfg`. The STM32U0 target file assumes SWD but does not select it, and the ST-LINK interface file leaves the choice open. Seen with xPack OpenOCD 0.12.0-7 on macOS 15.7.9 on 2026-10-08.

### OpenOCD says `couldn't bind gdb to socket on port 3333`. Is the board busy?

No: another OpenOCD on this Mac (often for a different board) holds the port. Add `-c "gdb port disabled; tcl port disabled; telnet port disabled"` before the interface file when you only want to flash or read. (OpenOCD 0.12 also accepts the older `gdb_port disabled` with a warning.)

### The LED pin, read over the debugger while the program runs, never changes. Is the LED broken?

Probably not. On this board, reading GPIOA's registers over SWD while the program runs returned wrong values (MODER read 0 although PA5 was an output). Halt, read, resume: `halt; read_memory 0x50000014 32 1; resume` shows bit 5 toggling. Each halt pauses the program's timer, so a 250 ms blink measures longer that way; judge the rate by eye or with few samples. Cause not known (2026-10-08).

## Installing ST's tools

### The STM32CubeCLT installer says "Apple could not verify ... is free of malware". How do you open it?

Click Done, then open System Settings, Privacy & Security, scroll to Security and click Open Anyway beside the installer's name; enter your password. ST's macOS package for STM32CubeCLT 1.22.0 is unsigned (`pkgutil --check-signature` prints "no signature"), and on macOS 15 right-click Open no longer gets past the warning. Seen 2026-10-08 on macOS 15.7.9.

### The installer stops with "not enough space" although the free space ST's release note asked for is available. How much does it need?

3.4 GB free. The package itself declares 3.2 GB (`xar -xf <package> Distribution`, then look for `installKBytes`); the installed folder measured 3.3 GB. Safari also leaves the `.zip` and `.tar.gz` copies of the download in the Trash (1.5 GB); empty it first.

## Compilers

### ST's Clang (`starm-clang`) fails to link ST's examples with "memory region not defined: RAM". What changes?

Call it in ST's hybrid mode, so it links with ST's GNU tools: add `--gcc-toolchain=/opt/ST/STM32CubeCLT_1.22.0/GNU-tools-for-STM32 --multi-lib-config=/opt/ST/STM32CubeCLT_1.22.0/st-arm-clang/multilib.gnu_tools_for_stm32.yaml` and write `--gcc-specs=nano.specs` where GCC takes `--specs=nano.specs`. ST's linker scripts are written for the GNU linker; Clang's own linker stops on them. ST's `starm-clang.md` (in the CubeCLT folder) lists the hybrid flags. For speed, add ST's `--config=Omax.cfg`: CoreMark went from 1.83 to 2.40 per MHz with it.

### Can a program use all 40 kB of RAM? ST's linker script says 32K

Yes. Change `LENGTH = 32K` to `LENGTH = 40K` on the `RAM` line of the linker script; the RAM is one block from 0x20000000 to 0x2000A000. A program filled and read back 38 kB with no error. 206 of ST's 209 linker scripts for this board say 32K, and Zephyr's default region is 32 kB too.

## Arduino (STM32duino)

### Arduino upload says "NOD_U083RC not found". What changes?

Set the upload method to STM32CubeProgrammer (SWD): in the Arduino IDE, Tools, Upload method; with arduino-cli, add `,upload_method=swdMethod` to the board name. If it then says "STM32CubeProgrammer not found", run `export PATH=/opt/ST/STM32CubeCLT_1.22.0/STM32CubeProgrammer/bin:$PATH` first. STM32duino 3.0.0 defaults to copying the program to the board's USB drive, and this board's debugger (firmware V2J43M28) showed no USB drive on a Mac, although ST's manual lists one; why was not found. Upload over SWD took 3 s. The error text was seen on 2026-10-09 and not saved.

## Tests that did not work without wires

### The DAC-to-ADC test through the chip's internal channel reads a flat 1.06 V. Is the ADC or DAC broken?

Probably not: use the wired loop instead, a jumper from A2 (PA4, the DAC output) to A0 (PA0, an ADC input). With the ADC set to the internal channel `ADC_CHANNEL_DACCH1`, it read 1,057 to 1,070 mV for all 16 DAC codes from 165 to 3,151 mV, buffered and unbuffered. Suspected: the setup did not select the internal route. Seen 2026-10-08.

### The UART loop in half-duplex (single-wire) mode gets no byte back. Does USART1 work?

Use the wired loop instead: a jumper from D8 (PA9, USART1 transmit) to D2 (PA10, USART1 receive). In half-duplex mode at 48 MHz, not one of 256 bytes came back at any speed from 115,200 baud. Suspected: single-wire mode does not loop the transmitter back to the receiver. Seen 2026-10-08.

### Why does one GPIO store take 2.0 cycles in one build and 2.5 in another?

The cause is not known. The same 16 back-to-back stores to GPIOA's BSRR register (the set and reset register) in assembly measured 2.0 cycles each at -O2 and -Os and 2.5 at -O3. Seen 2026-10-08.

### The ADC reads the internal reference low at its fastest sampling time. Which setting reads true?

Use a sampling time of 12.5 ADC clocks or more; that still converts at 0.96 Msps. At the shortest setting (1.5 clocks, 1.71 Msps) the internal reference averaged 1,469 counts against about 1,507 at 12.5 clocks and longer. Seen 2026-10-08.

### The chip stopped answering the debugger. How do you get it back?

Unplug the USB cable, wait 3 seconds and plug it back in. If it still does not answer, fit JP1 (the probe's reset line, off as shipped) and connect under reset: `STM32_Programmer_CLI -c port=SWD mode=UR`. Low-power and watchdog examples can stop the chip answering; those are left for a run with someone at the board.

## Mistakes to avoid

- Run the first Zephyr build with the toolchain variables above; without them it fails in 5 seconds.
- ST's release note gives STM32CubeCLT's disk need as 1.7 GB; the installer needs 3.4 GB. Read the number from the package.
- Do not halt the chip every half second to check for the end of a control-loop run: it caused two ADC overruns in early runs (not kept). The loop runs clean when nothing touches it.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-24. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- Examination failed after ST's FLASH_EraseProgram example; the next 11 examples could not connect at first; low-power and watchdog examples can stop the chip answering (`nucleo-u083rc/FAQ.md`, `nucleo-u083rc/AGENTS.md`)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| Wait a few minutes, or unplug USB and replug (all 11 loaded and ran on a second try a few minutes later) | hands | proven | `nucleo-u083rc/FAQ.md`, date not recorded |

<!-- recovery:end -->
