# What goes wrong with the Raspberry Pi Pico 2 H (SC1632, RP2350) on a Mac, and how do you fix it?

Started 2026-10-07 from Raspberry Pi's Pico 2 datasheet, release 5, and extended on 2026-10-11 after the first runs. Each answer starts with the fix and names the cause.

Terms: [docs/vocab.md](../docs/vocab.md).

**What caused it**, one tag per answer: Our mistake, Setup or handling, Tool or docs, Chip limit, Third-party behavior, Unknown.

### BOOTSEL was held but no RP2350 disk appeared. What went wrong?

Hold BOOTSEL first, then plug the cable in, then let go. The chip reads the button only as it powers up; pressing it on a board that is already running does nothing. The datasheet, section 5.1: "depower the board, then hold the BOOTSEL button down during board power-up (e.g. hold BOOTSEL down while connecting the USB)". If the disk still does not appear, try another cable: some micro-USB cables carry power only.

### `cp file.uf2 /Volumes/RP2350/` printed "Permission denied". What now?

Use `picotool load -x file.uf2` instead. The first copy that day worked and the next one failed with the board still in the loader. picotool loaded the same file on the first attempt. The `cp` line was `cp: /Volumes/RP2350/blink.uf2: Permission denied` (observed, output not saved).

### After loading an example, picotool can no longer restart the board. What now?

Unplug the cable, hold BOOTSEL, plug it back in, then let go. `picotool load -f` restarts a running program into the loader only if the program has the SDK's USB reset interface, which `pico_stdio_usb` adds (observed 2026-10-11: picotool found no device in the loader; exact line not saved). An example that defines its own USB device (`usb/device/dev_multi_cdc`) does not have it. Programs that print on a UART (the default for most pico-examples) are expected to lack it too.

### Which compiler works on an Intel Mac?

Set `PICO_GCC_TRIPLE=arm-zephyr-eabi` and `PICO_TOOLCHAIN_PATH=<folder holding bin/arm-zephyr-eabi-gcc>` before running CMake (that is GCC 12.2, extracted from the Zephyr SDK), or `PICO_GCC_TRIPLE=arm-none-eabi` with the folder of "GNU Tools for STM32 14.3.rel1" (from ST's developer site) (GCC 14.3.1). The Arm GNU Toolchain 15.2 that Raspberry Pi's VS Code extension offers (its toolchain list, read 2026-10-07) has no Intel Mac download. Arm GNU 14.2.Rel1 is the newest Intel build known, and it was not run. On this Mac (macOS 15) GCC 12.2 built the CoreMark, Dhrystone, occupancy-model, math and control-step images for the Cortex-M33; GCC 14.3.1 built those plus the FFT, FreeRTOS, CRC, Embench, BitNetMCU and ADC images. The RISC-V cores use Raspberry Pi's own GCC 16.1.0.

### Zephyr's blinky does not compile for the RISC-V cores (`led0` undeclared). What now?

Add an overlay that defines the LED: `west build -b rpi_pico2/rp2350a/hazard3 samples/basic/blinky -- -DDTC_OVERLAY_FILE=pico2/zephyr/hazard3-led.overlay`. The error is `'__device_dts_ord_DT_N_ALIAS_led0_P_gpios_IDX_0_PH_ORD' undeclared`. Zephyr 4.4.2's Cortex-M33 board file includes the LED description for GPIO 25 and the Hazard3 board file does not, so the `led0` alias is missing. The build then finishes; it was not flashed.

### The RISC-V core is slow at sin and cos. Is something wrong?

No. Hazard3 has no floating-point unit, so sin plus cos takes about 2.3k cycles against 155 on the Cortex-M33. Use fixed-point maths on the RISC-V cores, or build for the Arm cores.

### Does it help to run code from RAM?

Only the 30k model got faster in the tests. Building with `copy_to_ram` made CoreMark no faster, a 30k-parameter model 4 to 6 percent faster, and three smaller models 0.6 to 2.5 percent slower in five of six cases (one was 0.3 percent faster).

### `picotool load -f` prints "Waiting for device to reboot" and nothing happens. What now?

Unplug the cable, hold BOOTSEL, plug it back in, then load a program that has USB (`picotool load -x blink.uf2`). The examples loaded had no USB code, and picotool's exit code did not show that the board never rebooted. `picotool load -f` can only restart a board whose running program has USB stdio, and it exits 0 even when the board never rebooted, so check by reading the new program's output, not the exit code. Examples that never call `stdio_init_all` (`dma_channel_irq`, `hello_pio`, `pio_blink`, `pio_squarewave`, `hello_pwm`, `pwm_led_fade`) or turn USB off in CMake (`onboard_temperature`) leave the board with no USB at all. Examples that finish (return from `main`, such as `hello_divider`) leave USB listed but dead. Optional, only with a USB hub that can switch port power (`uhubctl`): cycle the port and send `picotool load -f -x` in the first seconds; without such a hub, use the button.

### How do you load a new program onto an Arduino-built Pico 2 without the button?

Open its serial port at 1200 baud and close it, then run `picotool load -x file.uf2`. An arduino-pico image has no picotool reset interface, so `picotool load -f` says no device was found; the 1200-baud touch reboots it into the loader (checked 2026-10-12, core 6.2.0).

### Can an AI agent load a program on a Pico 2 H without the button?

Yes, when the running program has USB serial. `picotool load -f -x file.uf2` restarts the board into the loader, loads the image and runs it, and the result comes back over USB serial with no button press (checked 2026-10-12).

### A Zephyr or UART-printing example will not restart without a button. Why?

Hold BOOTSEL while plugging in. pico-examples that print on a UART do not have the reset interface that lets `picotool load -f` restart the board, and Zephyr images are expected to behave the same (none was flashed, so this is not checked); each new image then needs a button press.

### Why does the `hello_dcp` example not build for the RISC-V cores?

 It uses the double-precision coprocessor, which is part of the Arm cores only. That is why 155 pico-examples build for the Arm cores and 154 for RISC-V.

## Mistakes to avoid

- Load an example that has its own USB device (`usb/device/dev_multi_cdc`) last in a run: it cannot be restarted by picotool.
- When several boards are plugged in, find the Pico's serial port by its USB serial number (the RP2350 board ID), not by the order or name of the ports.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-24. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- A running program with no USB code cannot be restarted into the loader by picotool load -f (exit code 0, board never reboots) (`pico2/FAQ.md`, 2026-10-11)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| Unplug USB, hold BOOTSEL, plug in, release; then picotool load -x | hands | proven | `pico2/FAQ.md`, 2026-10-11 |
| uhubctl power cycle of the probe's hub port (the image booted back into itself and never showed; a BOOTSEL replug was still needed) | remote | failed | 2026-10-12 |

USB hub ports recorded for this board:

- The probe's hub port power cut not checked, 2026-10-12. The cycle restarted nothing the Mac could see: the image has no USB code, so the board never showed.

<!-- recovery:end -->
