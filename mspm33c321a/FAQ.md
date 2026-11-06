# What goes wrong with the TI LP-MSPM33C321A LaunchPad development kit, and how do you avoid it?

Built from the MSPM33 SDK 1.04.00.00 on an Intel Mac. Results are in `data/results/mspm33c321a.csv`. Each answer starts with the fix. Terms: [docs/vocab.md](../docs/vocab.md). 1 kB = 1,024 bytes.

**What caused it**, one tag per answer: Our mistake, Setup or handling, TI example default, Tool or docs, Chip limit, Third-party behavior, Unknown.

## Before you flash anything

### Which SDK examples can lock or rewrite the chip's boot settings, and how do you keep them off the board?

Skip the examples that put a boot configuration into NONMAIN, and check every image's map file before flashing. Two of them do not look dangerous from their names: `driverlib/aesadv_gcm_128_enc_dec` and `driverlib/aesadv_gcm_auto_128_enc_dec` add SysConfig's NONMAIN module with `cscExists = true` (their `.syscfg` files in SDK 1.04.00.00). The others are `driverlib/flashctl_nonmain_memory_write` and the `boot_manager` examples (`customer_secure_code` sets NONMAIN the same way). Every example's linker file has a `.BCRConfig` section at 0x80101800, so the check is the same for all of them: `grep -E '^ +(BCR_CONFIG|BSL_CONFIG) ' <image>.map` must show 0 bytes used. Written from the SDK files, not seen on a board.

## Measuring and wiring

### TI's `hsadc_max_freq_dma` example says it samples at 9.375 MHz. How do you measure the ADC's real rate?

Change the DMA from single-block to single-transfer mode, so each finished conversion moves one result, and time 2,000 transfers with the cycle counter. The example's DMA channel is in single-block mode, where one trigger copies the whole block, so the first ADC result is copied 2,000 times in about 50 microseconds and the array holds repeats. With one transfer per trigger the measured time was 200 ns per sample (5.0 million samples a second) whenever the trigger period was 106 ns or shorter, on 2026-10-08 with SDK 1.04.00.00. The data sheet's 9.4 million was not reached and the cause is unknown. Source: `mspm33c321a/tools/build_adc.sh`.

### An I2C scan on the Education BoosterPack finds nothing. Which pins are SCL and SDA?

Use PB13 for SCL (BoosterPack pin 9) and PB12 for SDA (pin 10). Several SDK example READMEs list those header pins as PB0 and PB1; a scan there found nothing, while the light sensor answered at address 0x44 on PB13 and PB12 (manufacturer ID 0x5449, device ID 0x3001). The pin table in CCStudio's board file for this LaunchPad matches the measured result.

### The Education BoosterPack's microphone pin ignores speech and reads noise. Is the pack seated?

Check for the pack before debugging the code: scan I2C for 0x44 and read the microphone pin (BoosterPack pin 6, PA22) over 256 conversions. On 2026-10-30 the scan found no address, the pin read mean 3,095 with a peak to peak of 3 counts at a slow rate and 60 to 85 counts when sampled every 62.5 µs, and it did not move while a Mac spoke next to the board; the joystick and accelerometer counts wandered and S2 read pressed. A seated Education BoosterPack answers at 0x44 and its joystick sits near 2,048 (`portability/boosterpacks/edumkii/examples/probe/probe.c`, build with `APP=probe` prints all of it). The stack that day was not the pack the code expected; the cause on the bench was not confirmed.

### A C++ image that allocates memory stops after its first console line. What is missing?

Give the linker a heap on its command line: `-Wl,--heap_size=32768` (and `-Wl,--stack_size=...` for a bigger stack). SysConfig rewrites `device_linker.cmd` on every build, so a `sed` on that file does nothing, and the linker warns "creating .sysmem section with default size of 0x800". An image that built `new` objects of about 12 kB printed its first line and nothing more with the 2 kB default; with 32 kB it ran. Not isolated further than that.

### A software I2C or GPIO read-back always reads low. Why?

Turn the pin's input stage on after SysConfig has set it up. A GPIO that SysConfig configures as an output has its input stage off, so reading the pin returns 0 even when the line is high. `DL_GPIO_initDigitalInputFeatures` turns it on without changing the output enable.

### CoreMark prints "ERROR! Must execute for at least 10 secs" or a garbage result at 160 MHz. What is wrong?

Pick the iteration count so the run lasts between 10 s and 26 s. CoreMark refuses a result under 10 s, and the 32-bit cycle counter wraps after 26.8 s at 160 MHz, which makes longer runs read wrong. The runs used 1.6k iterations at 32 MHz and 6k to 9k at 160 MHz.

### CoreMark per MHz is much lower at 160 MHz than at 32 MHz. Is something misconfigured?

No, this is expected and probably the flash. At 32 MHz the registers read 0 flash wait states with the code cache off and the score is 4.38 CoreMark per MHz; at 160 MHz the flash needs 2 wait states with both caches on and the score is 3.40. The registers are `FRDCNTL_M33` at 0x40029000 and `FRD_INTF_CTRL_M33` at 0x4002900C.

### After running the FreeRTOS blink example, DSLite says "Device Connection Error ... low power mode". How do you get the board back?

Pulse the board's reset line from the probe: `xds110reset -s <serial> -a toggle -d 200` (it is next to `xdsdfu` in CCStudio's `xds110` folder). TI's `rtos/kernel/blink_led` lets the chip sleep so deeply that the debug port stops answering, and DSLite cannot connect until the chip is reset. No unplugging was needed on 2026-10-08. Flash a normal image first, then continue.

### A FreeRTOS build flashes with "Verification failed: Values at address 0x00000020 do not match". What is wrong?

Change `-T./mspm33c321a.cmd` to `./mspm33c321a.cmd` in the example's makefile. TI Arm Clang 5.1.1 ignores the `-T` form (it warns `Case insensitivity of options has been deprecated; T must be written as t`), so the image links with its code at address 0x20 instead of 0x10000000 and cannot be written to flash. The failed load also leaves the flash erased, so DSLite then says `Device Connection Error`; pulse the reset line (`xds110reset -s <serial> -a toggle -d 200`) and flash a good image. Checked 2026-10-09 on SDK 1.04.00.00.

### A FreeRTOS test hangs inside an interrupt that calls `xSemaphoreGiveFromISR`. What priority does it need?

Set the interrupt's raw priority to 0x40: `NVIC_SetPriority(irq, 0x40u >> (8 - __NVIC_PRIO_BITS))`. At raw priority 0x04, which is what the config file's "maximum system-call priority 1" means with the device's 6 priority bits, the kernel's assert stops the core inside the interrupt with no fault flag set. At 0x40 the same test ran 1,000 passes clean. The prebuilt kernel library in the SDK also has no static allocation, so create tasks and queues from the heap. Checked 2026-10-09.

### A TI MSPM0 DriverLib example builds for the MSPM33 but its UART test never finishes. What is wrong?

Several MSPM0 UART examples (`uart_rw_multibyte_fifo_poll`) expect a wire from TX to RX and wait for bytes that never arrive. Add `UART1.enableInternalLoopback = true;` to the example's `.syscfg` (the app stays as TI wrote it). With it, `rxPacket` reads `MSP!` (checked over the probe 2026-10-24).

### A carried-over MSPM0 example stops at build with `DL_I2C_ClockConfig`, `ADC12_0_INST_INT_IRQN` or `DL_INTERRUPT_GROUP_1`. Can it be fixed?

Not by a rename. The MSPM33 I2C is the UNICOMM block (`DL_I2CC_` and `DL_I2CT_` names), its ADC is the HSADC block, and the interrupt-group alias is not in its DriverLib. GPIO, SysTick, timer, PWM, SPI, UART and CRC examples do move with the app untouched (`portability/everywhere/PORTING.md` section 7.1). Checked 2026-10-24.

## MicroPython on Zephyr

### MicroPython built for the lp_mspm33c321a Zephyr board prints nothing on the serial port. What is missing?

Build in a Zephyr tree that carries TI's open UART and GPIO pull requests (`ports/micropython/zephyr-mspm33c321a/zephyr-tree.sh`). Zephyr `main` (15 October 2026) describes only flash and RAM for this board: no console node, no clock setup, GPIO nodes disabled. With pull requests 116768 and 118511 merged, the banner and prompt appear on the probe's serial port at 115200 baud, and a 1 MB image with MicroPython's own settings uses 28.5 percent of flash and 32.7 percent of RAM (`ports/micropython/zephyr-mspm33c321a/README.md`).

### DSLite says "Length of block is 25172, but it should be divisible by 16" on a Zephyr image. How do you flash it?

Pad the binary with 0xFF to a multiple of 16 bytes, convert it to an Intel HEX file at 0x10000000 with `objcopy`, and flash that. DSLite writes 128-bit words, and the Zephyr hex ends in the middle of one. If DSLite was stopped partway and the next run says `Error initializing emulator (Error -261)`, `xds110reset -s <serial> -a toggle -d 200` clears it.

### In MicroPython on Zephyr, an I2C read never returns and the console goes silent. What do you do?

Reset the board and avoid reads; writes return. `readfrom` and `readfrom_mem` on the UNICOMM I2C driver from Zephyr pull request 110526 hung in three tries, while a write to an absent address returned 1 instead of an error. The cause is not traced.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-24. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- rtos/kernel/blink_led sleeps so deeply that the debug port stops answering (Device Connection Error, low power mode) (`mspm33c321a/FAQ.md`, `mspm33c321a/AGENTS.md`)
- A flash write cut off half-way: the image faults at boot (CFGAP_BOOTDIAG 0x07), the core never stops for the debugger (`mspm33c321a/AGENTS.md`, 2026-10-22)
- An image that writes NONMAIN or the boot configuration at 0x80101800 (`mspm33c321a/FAQ.md`, `mspm33c321a/AGENTS.md`)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| xds110reset -s <serial> -a toggle -d 200, then flash a normal image (for the deep-sleep lockup) | remote | proven | `mspm33c321a/FAQ.md`, date not recorded |
| xds110reset and a USB replug (for the half-written image) | remote | failed | `mspm33c321a/AGENTS.md`, 2026-10-22 |
| Wait-for-debug (EnableWaitForDebug before connect, or the pwrap.js register sequence), then loadBinaryProgram of the saved main flash (1 MB read-back hashed the same; no mass erase, NONMAIN untouched; also runs from the Digital Lab) | remote | proven | `mspm33c321a/AGENTS.md`, 2026-10-22 |

Restore: CCStudio scripting loadBinaryProgram with FlashEraseSelection Erase MAIN memory only. Regions: main flash at 0x10000000 (1 MB); never NONMAIN or 0x80101800. (`mspm33c321a/AGENTS.md`)

<!-- recovery:end -->
