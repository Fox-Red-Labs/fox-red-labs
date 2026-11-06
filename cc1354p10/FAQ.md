# What goes wrong with the TI LP-CC1354P10-1 LaunchPad development kit on a Mac, and how do you fix it?

Every problem seen on the TI LP-CC1354P10-1 LaunchPad development kit (CC1354P10 chip: Arm Cortex-M33, 48 MHz, 1 MB flash, 296 kB RAM in TI's count (the linker gives programs 256 kB of SRAM), Bluetooth LE, Zigbee, Thread and sub-1 GHz radios; onboard XDS110 probe) from a Mac. Each answer starts with the fix, then why. Measured on an Intel Mac, macOS 15.7, the F2 SDK (SimpleLink CC13xx CC26xx SDK 8.33.00.16), TI Arm Clang 4.0.4 (the sleepy sensor) and 5.1.1 (the border-router host), Home Assistant with ZHA on a separate host.

Terms: [docs/vocab.md](../docs/vocab.md) (ZHA, sleepy end device, poll, border router, standby, node descriptor, trust center, factory new). 1 kB = 1,024 bytes. For the benchmark answers: the DWT cycle counter is a counter in the Arm core's debug block that adds one every CPU cycle; a probe pin reset (`xds110reset`) is the debug probe pulsing the chip's reset line; the FPU is the floating-point unit; a FIFO is the ADC's small queue of results; ksps is thousands of samples a second.

Contents: [Joining Home Assistant over Zigbee](#joining-home-assistant-over-zigbee), [Building and loading](#building-and-loading), [Thread border router host](#thread-border-router-host), [Bluetooth](#bluetooth), [Benchmarks](#benchmarks).

## Joining Home Assistant over Zigbee

### Why does a sleepy Zigbee sensor associate but not get the network key, and have to try again?

Open joining on the router nearest the board (`zha.permit` with that router's `ieee`, here a smart plug), not only on the coordinator, and keep the board within a few metres of it for the first join. In 4 of 6 associations the board's association was accepted but the network key arrived after the board had stopped waiting; the coordinator was out of direct range and every join went through a router. Why the key was late was not found. The clean join took 6.0 s from boot (1 run). Details in `cc1354p10/zigbee-sleepy-sensor/README.md`, trap 5.

### TI's `zed_temperaturesensor` does not join by itself and shows 17 °C. Is it broken?

No: as shipped it joins only from the left button (BTN-1) or its serial menu, and its temperature is a fixed 17.00 °C that moves only from the menu. At boot the example resumes a saved network and nothing else, and it never reads the chip's temperature sensor. The patch in this repo starts joining 1 s after boot when there is no saved network, retries every 15 s, and reads the chip temperature every 10 s with TI's Temperature driver. F2 SDK 8.33.00.16. `cc1354p10/zigbee-sleepy-sensor/README.md`, traps 1 and 2, changes 1 and 4.

### Can you make a TI Zigbee end device wait longer than 3 s for the network key?

Only by editing the stack header in a copy of the SDK. Z-Stack waits 3 s (`BDB_DEFAULT_DEVICE_UNAUTH_TIMEOUT 3000` in `bdb_interface.h`), and it is a plain `#define`, not a SysConfig setting.

### The sensor joined, but Home Assistant took almost three minutes to show an entity. Is it stuck?

Wait. The entity appears about 160 s after the join (162 s measured, one run); nothing is wrong on the board. About half of ZHA's first setup requests to the sleeping board got no answer; zigpy sent each again about 28 s later and every resend was answered within one 3 s poll period. The cause of the lost first tries was not found.

### Home Assistant shows the board as "unk_manufacturer unk_model". Did the join fail?

No. The join worked; Home Assistant could not read the names, because TI's example answers every attribute read with `0xC1` (`SOFTWARE_FAILURE`). ZHA reads the manufacturer and model attributes during setup; when the read fails it shows `unk_manufacturer unk_model`. The fix is the read callback in the next entry. With reads working, TI's example would show the manufacturer "TexasInstruments" and no model (it has no Model Identifier attribute); the patch in `cc1354p10/zigbee-sleepy-sensor/` sets "Fox Red Labs" and "Temp Sensor". `cc1354p10/zigbee-sleepy-sensor/README.md`, traps 2 and 3.

### After a re-pair, the entity is still called `sensor.unk_manufacturer_unk_model`. Why?

Rename the entity in Home Assistant. After removing and re-adding the device, Home Assistant reused the old entity id while the device name became "Fox Red Labs Temp Sensor". That is Home Assistant's doing, not TI's. Trap 13 in `cc1354p10/zigbee-sleepy-sensor/README.md`.

### Every attribute read from Home Assistant returns `0xC1` (`SOFTWARE_FAILURE`). Why?

Register a read callback in the example: one line, `zcl_registerReadWriteCB(...)`, as TI's light example does. In this Z-Stack, `zclProcessInReadCmd` sends every read through the endpoint's read callback, and `zcl_registerReadWriteCB`'s comment says one is needed only when an attribute's data pointer is NULL; `zed_temperaturesensor` registers none. Home Assistant's log says `failure: [{0: <Status.SOFTWARE_FAILURE: 193>}]`. After the patch, 20 of 20 reads succeeded. Patch and logs in `cc1354p10/zigbee-sleepy-sensor/`.

### The first temperature report took 15 minutes. Is the sensor broken?

No. Home Assistant sets the report interval, not the device. ZHA set min 30 s, max 900 s and change 0.50 °C, replacing the device's defaults, so at a steady temperature the first report came 899.9 s after ZHA set it up. Trap 4 in `cc1354p10/zigbee-sleepy-sensor/README.md`.

### Why did the board join someone else's network on channel 11 or 20?

List only your network's channel in the `.syscfg` (`zstack.rf.primaryChannels`). TI's stack joins the first open network it finds. Searching all 16 channels, the board also associated with PAN 0xe99d on channel 11 (suspected to be kept by another board nearby) and PAN 0xdc20 on channel 20 (owner not known); each cost a 3 s key wait. Trap 9 in `cc1354p10/zigbee-sleepy-sensor/README.md`.

### The device was removed in Home Assistant, and the board still says "In the network". Why?

Also use the serial menu's RESET TO FN (reset to factory new) on the board. The coordinator logged `DEVICE_LEFT` from the board's parent 2 s after the removal, but the board's screen still said "In the network". Why the leave did not reach the board was not found. Trap 10 in `cc1354p10/zigbee-sleepy-sensor/README.md`.

### After RESET TO FN the board stays silent and does not rejoin. How do you get it back?

Load the image again with `cc1354p10/tools/ccs_load_run.py`; it comes back factory new. On this board a program loaded by the debugger does not come back when it restarts itself, and the reset is a restart. Not checked: whether a USB replug alone brings it back. Trap 7 in `cc1354p10/zigbee-sleepy-sensor/README.md`.

### TI's DMM light joined Home Assistant and no light appears; `zha.issue_zigbee_cluster_command` answers `HTTP Error 400: Bad Request`. Why?

Remove the old device for this board in Home Assistant, then commission again from the board's menu. Home Assistant's Zigbee integration is suspected of not reading a known address's endpoints again when it rejoins (not checked). The board had joined before as a temperature sensor; after `dmm_zr_light_remote_display_app` joined, Home Assistant still showed the "Temp Sensor" record with no light, and a toggle to endpoint 8 failed with that error. `cc1354p10/dmm-zigbee-ble/README.md`.

### Does the serial menu stop the sleepy sensor from reaching its lowest current?

Possibly; it was not measured. TI's UART driver keeps the chip out of standby while it listens (`UART2_rxEnable` in `source/ti/drivers/UART2.c`), and the menu always listens. TI's Zigbee power page says to define `CUI_DISABLE` and remove `BOARD_DISPLAY_USE_UART` for low power. Not checked: current with and without `CUI_DISABLE`; no meter was on the bench. Trap 8 in `cc1354p10/zigbee-sleepy-sensor/README.md`.

## Building and loading

### The debugger will not connect to the board once a program is running. How do you get back in?

Start a loop of loads first, then unplug and replug the USB cable: one load gets in during the few seconds the chip stays awake at boot. While the image sleeps between polls, every connect failed, including with `IcePick_C: Error connecting to the target: (Error -242) A router subpath could not be accessed`. On 2026-10-07 the load went in on the 21st try of a loop, about 10 s after the replug; a replug with no loop running missed the window twice. The loop command is in `cc1354p10/zigbee-sleepy-sensor/README.md`, "Before you load it".

### The headless CCStudio build says "Compiler version 'TICLANG_3.2.2' is not currently installed". Does the build fail?

No: it builds with the compiler you have. The Zigbee project asks for TI Arm Clang 3.2.2; CCStudio 21.0.1's headless import says `Compiler version 'TICLANG_3.2.2' is not currently installed - defaulting to 'TICLANG_4.0.4.LTS'` and builds with that. The build then has one warning, from TI's own linker file (`redefinition of macro 'NVOCMP_NVPAGES'`). Trap 12 in `cc1354p10/zigbee-sleepy-sensor/README.md`.

### TI's README for `zed_temperaturesensor` lists other LaunchPad development kits, not the LP-CC1354P10-1. Does the example work on it?

Yes: the example is in the SDK's `LP_EM_CC1354P10_1` folder, and it built and joined on this board. The README names the CC26x2R1, CC1352P, CC1352R1, CC2652RB and CC2652RSIP boards only. Trap 11 in `cc1354p10/zigbee-sleepy-sensor/README.md`.

### Can you power-cycle the LP-CC1354P10-1 from the Mac so loads work without a replug?

Yes, if the board sits on a USB port `uhubctl` can switch: switch it off for 6 s, on again, and start the load at once. The debugger connects only in the first seconds after power-on, and a reset-line pulse does not reopen that window. Install with `brew install uhubctl` (2.6.0 here) and run `uhubctl` alone to list hubs: a hub marked `ppps` (per-port power switching) can switch its ports. Here that was the USB-A port of Apple's USB-C Digital AV Multiport Adapter (hub `20-4`, port `1`). A 3 s off time did not reopen the window (observed 2026-10-08; not saved), and switching power while a load is connected left the probe answering `IcePick_C: Error connecting to the target: (Error -261 @ 0x0) Invalid response was received from the XDS110.` until the next off and on. The bench runner does it all with `UHUB=20-4:1 ./run-all.sh`:.

### The headless CCStudio build says `0 out of 1 projects have errors` but there is no `.out` file. Did it build?

No. Check that the `.out` exists after every headless build. The summary line `CCS headless build complete - 0 out of 1 projects have errors` is printed even when SysConfig stopped and nothing was compiled. Seen with the Zigbee and DMM projects of SDK 8.33.00.16 on CCStudio 21. `cc1354p10/README.md`, "Building Zigbee and DMM examples from a terminal".

## Thread border router host

### The OpenThread host on the CC1354P10 goes silent within a second of `ifconfig up` or `thread start`. Is the board dead?

Build the host with standby off (the SysConfig power policy), power-cycle the board, and it runs: network formed and leader in 16 s from a fresh dataset, 37 to 39 s after a restart. With standby allowed, the console stopped echoing within a second of the first radio commands and the capture ended with one flushed byte; whether the radio link to the CC2340R5 or the console dropped first is not known. The build and the hang log are in `border-router/host-cc1354/README.md`.

### Connecting with CCStudio resets the board, and after a BLE demo it needed a power cycle (error -242). Why?

Unplug and replug the board; the debugger has no post-mortem on this chip, so a connect always resets it. It can happen after a Bluetooth demo has been running; a USB hub with per-port power switching makes the power cycle a command.

## Bluetooth

### Why does a BTHome or custom-service UUID arrive byte-swapped from SysConfig?

Type the UUID byte-reversed in the `.syscfg`, then check the bytes in the generated `ti_ble_config.c`. With the CC13xx CC26xx SDK 8.33.00.16, SysConfig writes a 128-bit UUID in the scan-response UUID list most significant byte first; Bluetooth sends UUIDs least significant byte first. The F3 SDK does the same to a 16-bit service-data UUID: for BTHome's 0xFCD2 you type `UUID016SD = 0xD2FC`. The worked 128-bit line is in `ports/phone-app-demos/blinky-cc1354p10/project_zero.patch`.

### macOS lists the board under an old Bluetooth name

Find it by the name it advertises or by its service, not by the name in a list. macOS remembers a device's name by its address. After `dmm_zr_light_remote_display_app` was loaded (advertising "Fox Red DMM"), the Mac still showed "Fox Red Lab CC1354" from an earlier image.

## Benchmarks

### Why does `DWT->CYCCNT` read 0 on a CC1354P10 after a probe pin reset, even with `TRCENA` and `CYCCNTENA` set?

Load and start the program from the debugger, or time with SysTick instead. On the LP-CC1354P10-1 the Cortex-M33 cycle counter stayed at 0 after a probe pin reset (`R,rtos,timer_rate,0,FAIL,counter 0 systick 45015`) and counted normally when CCStudio had loaded and started the same image (`counter 45014 systick 45017`). A power-on boot with no debugger and the reset button were not run. No TI document found says why; Arm's [ARMv8-M processor debug guide](https://developer.arm.com/documentation/100734/0100/Debug-for-ARMv8-M/Other-debug-functionality/DWT) says the DWT counters stop when non-invasive debug is not enabled. To time code without it, use SysTick with a wrap counter. F2 SDK 8.33.00.16, 2026-10-08.

### Why does a CC1354P10 ADC interrupt (`INT_AUX_ADC_IRQ`) fire for ever and stop the program?

Flush the ADC FIFO in your handler on an overflow, an underflow or a sample left after the read, as TI's ADCBuf driver does, and stop the interrupt line once you have the interrupts you need. The power-loop and control-loop bench images printed nothing at first. With four changes (that flush, the handler stopping its own line after 1,000 interrupts, the slow passes first, and a timeout counted on SysTick) all 30 of 30 runs reported, in about 20 s each. Which change cured it is not known: the flush never fired in the -O2 runs. The CC13x4's ADC interrupt event includes the FIFO's overflow and underflow flags, and both stay set until the FIFO is flushed. F2 SDK 8.33.00.16, 2026-10-08.

### How fast can the CC1354P10 run a digital-power PID loop or an FOC control loop, and how much CPU does it take?

With no missed period in 1,000, the PID loop holds 117.1 kHz in float and 120.6 kHz in Q15, and the FOC loop 46.2 kHz (-O2, code in flash, 48 MHz). At 100 kHz each PID interrupt takes 48.5 percent (float) or 42.9 percent (Q15) of the CPU; CPU time alone would allow about 206 and 233 kHz, so the AUX ADC's 2.7 µs sample and its conversion before the interrupt are the suspected limit, not tested by changing them. Each FOC interrupt takes 180.4 percent of a 100 kHz period.

### What does a CPU load of 180 percent mean for the CC1354P10's FOC loop, and why does it still hold 46 kHz?

Each interrupt takes 866 cycles of CPU, 1.8 times a 100 kHz period of 480 cycles, so the loop cannot run at 100 kHz; at 46.2 kHz (1k cycles a period) it fits. The estimate was 38 to 42 kHz, from the worst case seen at 50 kHz, where every loop ran past its period; at the sweep's limit no new value was written late. The float power loop, 117.1 kHz against an estimate of 120 to 135, missed for the same reason: one rare slow interrupt per pass that the sweep's trials did not meet is suspected.

### Why does a rate sweep on the CC1354P10 fail its first trial with an ADC overflow and report 10 kHz?

Read the AUX ADC event flags back after clearing them, before you clear the interrupt's pending bit, and let any conversion in flight finish before you flush the FIFO. The first sweep cleared the flag and then the pending bit at once; after a run that ended with the loop overrunning, the interrupt pended again on an empty FIFO, the read underflowed and the trial counted an overflow. The fixed image stops the trigger, waits 10 µs, flushes, clears the flags, reads them back, then clears the pending bit. F2 SDK 8.33.00.16, 2026-10-08.

### Why does each CC1354P10 control-loop interrupt take about 100 cycles more CPU time than its loop work?

Hardware entry and return, the handler saving and restoring 8 registers, its setup before the first timestamp, and the FPU's register save are all CPU time too. The estimate counted only the loop work and came out 26 to 146 cycles short, depending on the loop; the disassembly of the handler shows each of these parts.

### Why is the CC1354P10's AES hardware only 2.5 to 3.4 times faster than software for one block, when the results say 51 times?

The 51 times is for 1 kB; for one 16-byte block most of the hardware time is the driver call. One block takes 1.5k cycles through TI's AESECB driver, which loads the key on every call, against 3.7k for tiny-AES-c with its key expanded beforehand and 5k with the key expansion timed. A Bluetooth or Zigbee packet is closer to one block than to 1 kB. TI Arm Clang 5.1.1 -O2, F2 SDK 8.33.00.16.

### Why does `cosf` take 447 cycles on the CC1354P10 when `sinf` takes 75?

TI Arm Clang's `cosf` does one step in `double`, which this FPU cannot do, so that step runs in software. The disassembly of the linked `cosf` (TI Arm Clang 5.1.1, `s_cosf.c.obj`) converts its quadrant number with `__floatsidf` and adds -0.5 with `__aeabi_dadd`, both software double routines; `sinf` calls nothing. The library source was not read and no other `cosf` was tried. As arithmetic only, a `cosf` as fast as `sinf` would take about 370 cycles off a motor-control (FOC) step of 671.

### A control loop takes longer than its PWM period but drops no triggers. Is the count wrong?

Probably not: the ADC can sample the next period while the CPU finishes this one. At a 50 kHz trigger (960 cycles) the loop took 1k cycles from trigger to new PWM value, and every one of 3,000 triggers was dated one period after the last from the cycle counter. About 280 of those cycles are the AUX ADC's 2.7 µs sample and conversion, done in hardware; the CPU part is 747 cycles. So the loop keeps up, but each PWM value lands about 1.4 µs into the next period. Why the timer event reached the ADC while the interrupt still ran was not found.

### Does `double` math use the CC1354P10's FPU?

No. The FPU does single-precision `float` only, so `double` add, multiply and divide are library calls. In the bench, `double`-heavy Embench programs ran only 1.5 to 1.8 times faster per cycle than on the Cortex-M0+ CC2340R5, against 12.3 times for the `float` program `minver`..

### The CC1354P10 ADC (`ADCBufCC26X2`) asked for 250 ksps gives only 125 ksps. Why?

TI's default synchronous mode (the ADC samples when a trigger arrives) takes 2.7 µs of sampling plus the conversion, so at 250 ksps every second trigger is missed. In the ADCBuf driver's asynchronous mode (the ADC samples continuously and the trigger only takes the result) the same pin held 250, 300 and 400 ksps (500 fell back to 249.8); the data sheet (SWRS267B section 8.19.1.1) gives 200 ksps. The pin was floating, so these are rates only, not accuracy.

### The FreeRTOS heap bench prints `I,heap,total 32768` but the map shows a bigger heap. Which is right?

The map: the heap that links is 242.6 kB (`.bss.ucHeap`). The report line prints TI's default from a file compiled without this repo's heap setting. That heap held 388 tasks with 512-byte stacks, 5 fewer than the arithmetic predicts; where the other 3.0 kB went was not found.

### Why does `memcpy` take 8 cycles a byte in a TI Arm Clang build?

Pass your `-O` flag to the link step too. Linked without one, TI Arm Clang 5.1.1 pulls in `TI_memcpy_small`, a byte loop, for `__aeabi_memcpy8`; linked with `-O3` it uses the word-copy `memcpy`. The linker map names which one you got.

### A bench reports `stack_used 4096 of 4096`. Did the stack overflow?

Possibly; give that image a bigger stack. `stack_used` counts painted stack words that were overwritten, so 4096 of 4096 means every word was used and the stack may have gone past its end. In the `embench_b` image the linker map shows about 231 kB of unused RAM below the stack, so nothing is suspected to have been overwritten, and all five programs passed their own checks.

### Does the CC1354P10 have a CRC accelerator?

No. The F2 SDK's `cc13x4_cc26x4` memory map lists no CRC block and its driverlib has no CRC module; the CRYPTO block does AES, SHA-2 and public-key math. Use a table-driven CRC in software.

## Post-quantum key exchange and signatures

### Does ML-KEM-768 (post-quantum key exchange, formerly Kyber) run on the LP-CC1354P10-1, and how fast?

Yes: 20.8 ms to make a key pair, 24.0 ms to encapsulate and 29.1 ms to decapsulate at 48 MHz, with mlkem-native 2.0.0 in plain C at `-O3`, and its five known-answer checks pass (NIST's three vectors plus the rejection and key checks). The full exchange with a Mac over USB serial completes on the board: key pair from the chip's random-number generator, key check matching on both sides, and one AES-128-GCM line decrypted. Not reached: it uses 14.7 to 20.0 kB of stack and 80.5 kB of flash at `-O3` (9.4 kB at `-Os`, built, not run), well inside the chip's RAM and 1 MB of flash. SimpleLink CC13xx/CC26xx SDK 8.33.00.16, TI Arm Clang 5.1.1.LTS, 2026-10-08. `cc1354p10/pqc-join/README.md`,.

### The ML-KEM demo stops after `join: making an ML-KEM-768 key pair` and the Mac says `timed out waiting for @PK from the board`. Why?

The image never called `TRNG_init()`. `Board_init()` sets up only the power and GPIO drivers; without `TRNG_init()`, `TRNG_open()` returns NULL, and the first `TRNG_getRandomBytes()` call with that NULL handle faults (the debugger shows the CPU in an exception handler). If you see this, call `TRNG_init()` before `TRNG_open()` and check that the handle is not NULL; TI's `TRNG.h` says the same. With the fix, 64 random bytes take 20 ms and the exchange completes. F2 SDK 8.33.00.16, 2026-10-08. `cc1354p10/pqc-join/README.md`.

### Does ML-DSA-44 (post-quantum signatures, formerly Dilithium) run on the LP-CC1354P10-1, and how fast?

Yes: 49.5 ms to make a key pair, 50.5 ms to verify, and 154 ms on average to sign (77 to 361 ms over 16 messages, because signing retries a varying number of times) at 48 MHz, with mldsa-native 2.0.0 in plain C at `-O3`. NIST's known-answer vectors pass on the board first. RAM is the constraint to watch: signing needs 50 kB of stack, so the image sets a 64 kB stack; the chip has 256 kB. Code size 106.7 kB at `-O3`, 13.3 kB at `-Os` (built, not run). 2026-10-08. `cc1354p10/pqc-join/README.md`.

### Can you try the ML-KEM-768 exchange without a board?

Yes: `portability/pqc-join/test/run.sh` compiles the board's code for the Mac and runs the whole exchange against OpenSSL's ML-KEM through a pipe; it prints `pqc-join host test: OK`. It needs a C compiler, Python with the `cryptography` package and OpenSSL 3.5 or later; the board run needs a power-switchable USB port (`uhubctl`), because this board's debugger connects only just after power-on.

### How long does ML-KEM-768 take over a 115,200-baud serial link, against X25519?

About 420 ms of wire time for 2.2 kB of key material, written as hex (4,864 characters in all), against 34 ms for X25519's 32 bytes each way. The slow part is the 115,200-baud serial link chosen here, not the chip: on the board run of 2026-10-08 the board's own work was 73 ms (random bytes 20 ms, key pair 21 ms, decapsulate and derive the key 30 ms, decrypt 2.5 ms), and the Mac measured 346 ms from receiving the key to the board's answer. Wire times are computed from the byte counts, not measured.

### Is the ML-KEM demo safe for real keys?

No. Neither mlkem-native, mldsa-native nor this demo claims protection against power or fault attacks, and the demo prints a key check; it shows the math and its cost, not a hardened product.

## Sub-1 GHz receiver for the CC1407P weather station

### TI's GitHub copy of the F2 SDK will not link `rfPacketRx`: `drivers_cc13x4.a`, `driverlib.lib` and `nortos_cc13x4.a` are missing. Why?

The GitHub repository (`TexasInstruments/simplelink-lowpower-f2-sdk`, tag `lpf2-8.33.00.16`) carries those libraries as source only; TI's installer ships them built. `python3 cc1354p10/radio-rx/build_f2_libs.py <sdk>` builds them with TI Arm Clang 5.1.1 from the SDK's own CMake source lists in a few minutes: 43 of 43 driverlib files, 14 of 14 no-RTOS files, 83 of 90 driver files (CAN, SD card and PDM fail and the receiver does not use them). TI packaging, 2026-10-10. Details: [radio-rx README](radio-rx/README.md).

### DSLite says `Error -242 @ 0x0 A router subpath could not be accessed` when flashing the receiver. What now?

A program was already running and its one debug connection had been used (here by a flash backup read). Unplug the kit's USB for 3 seconds and flash again; `xds110reset` alone did not clear it. 2026-10-10.

### The receiver flashed fine but its serial port prints nothing. Is it broken?

The image does not start on its own after DSLite. Run `xds110reset -s <probe serial>` (in CCStudio's `ccs_base/common/uscif/xds110/`); the banner `rfrx: CC1354P10 listening at 915 MHz` appears within a second. 2026-10-10.

### The receiver's printout skips packets, but `count ok=` says none were lost. Which is right?

The count. It comes from the radio core's own counters. The XDS110 serial bridge drops lines while no program on the Mac has the port open, so a printout started late, or read by two programs at once, has gaps, and the first line after opening can be a stale replay. From 2026-10-10 23:46 to 2026-10-11 06:00:48 the count matched the sender: 21,658 of 21,658. 2026-10-11.

### The `sensor.frl_radio_*` entities vanished from Home Assistant. Did the bridge stop?

Probably Home Assistant restarted. `ha_bridge.py` creates the four sensors through the REST API's states endpoint, not through an integration, so Home Assistant does not keep them across a restart; they come back on the bridge's next post, within 10 s, if the bridge is still running. If they do not come back, check the bridge's output for an HTTP error, usually an expired token.

### Wi-SUN packets from the CC1407P arrive, but every byte reads backwards. Is the data corrupt?

No. With TI's 802.15.4g settings and `CMD_PROP_RX_ADV`, this radio stores each payload byte bit-reversed: 0x62 is 0x46, an `F`. The two-byte header comes low byte first, so `08 40` is length 64 with the whitening bit set. The radio checks the checksum either way; 260 of 260 sent arrived with a good checksum in each of Wi-SUN modes 2b and 4b (rx-check.txt). Reverse the bits of each byte in your code.

### The CC1407P sends SUN O-QPSK, and this board's SUN O-QPSK setting sees every packet with a bad checksum. Why?

Not known for sure. With SysConfig's `qpsk50kbpsrm3` setting at 915 MHz, 260 packets sent from the CC1407P gave only bad-checksum receptions (rx-check.txt). The receive command SysConfig generates for that setting expects TI's proprietary frame (sync word 0x930B51DE, a 1-byte length), while the CC1407P's driver names its header for 802.15.4 SUN O-QPSK. A frame mismatch is the suspected cause; no sniffer checked it. To test the CC1407P's O-QPSK, receive it on a second CC1407P.

### The CC1354P10 receives no packets with `maxPktLen` set above 255. Why?

`maxPktLen` is an 8-bit field in `CMD_PROP_RX` (`rf_prop_cmd.h`). The suspected cause is that a larger value keeps only its low byte, so longer packets are dropped; not checked in a register dump. With 259 the receiver counted no packets; with 255 it did (observed 2026-10-11, log not saved). Keep it at 255 or less there; `CMD_PROP_RX_ADV`, used for TI's Wi-SUN settings, has a 16-bit field.

### The CC1354P10 receives one packet, then nothing. Why?

The receive command ends after one packet unless it is told to repeat. Set `pktConf.bRepeatOk` and `bRepeatNok` before posting it, as `partner.c` does; the first build lost those two lines to a bad edit.

### With an external LP-XDS110 probe, CCStudio `loadProgram` on a CC1354P10 stops at `OnPreFileLoaded(): Connect failed`. How do I flash?

The load's second connect misses the few seconds after power-on in which this board accepts a debugger. Do this: power-cycle the probe's USB port while a CCStudio scripting connection retries `target.connect()`, then `target.halt()` and `memory.loadBinaryProgram(<image flash bytes from address 0>, 0x0)`; it loaded 12 of 12 images, where `loadProgram` and `DSLite flash` failed on every attempt in a run of 14. The image is the `.out` segment at address 0, leaving the board's CCFG alone. Read it back to confirm. 2026-10-24, probe LS5600RE.

### How do I back up, change and check this board from a terminal?

Run `frl run --board cc1354p10 --pattern 150,150,150,900 --probe <serial>`: it backs up main flash and the CCFG (the boot configuration), builds the pattern project, writes main flash and times the red LED. It needs CCStudio (its Python scripting runs every debug step), the SimpleLink CC13xx CC26xx SDK (`export FRL_F2_SDK=<the SDK folder>`), SysConfig and TI Arm Clang. If you see `FRL-E006 ... connect refused` (CCStudio may also show `Error -242`), unplug the probe's USB cable for 6 s, plug it back in and run the same command at once; on a hub that `uhubctl` (a command-line tool that switches power on USB hub ports) switches, `FRL_POWER_CYCLE=<hub>:<port>` does it for you. Put the board back with `frl restore <backup folder>`. 3 of 3 runs on 2026-10-25, probe LS5600RE; `docs/frl.md`; more ways back are in "When the board stops answering" below.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-25. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- The debugger connects only in the first seconds after power-on (cause unknown, suspected chip limit); a sleeping image misses it: Error -242, A router subpath could not be accessed (`cc1354p10/FAQ.md`, `cc1354p10/AGENTS.md`, 2026-10-07)
- A flash backup read, or a connect after a BLE demo, uses the one debug session: Error -242 on the next load (`cc1354p10/FAQ.md`, 2026-10-10)
- External LP-XDS110 probe: loadProgram and DSLite flash fail at the second connect (Error -242, OnPreFileLoaded(): Connect failed) (`cc1354p10/FAQ.md`, 2026-10-24)
- Loading a whole .out rewrites the CCFG at 0x50000000 with the SDK default, which differed from the board's original (`cc1354p10/AGENTS.md`, 2026-10-24)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| frl's ccs13 route: one CCStudio Python scripting session per step, connect retried after a 6 s power cycle of the probe port when FRL_POWER_CYCLE names it, write with loadBinaryProgram from 0 (CCFG left alone) (three full cycles and four restores on the external LP-XDS110 probe, no connect refused, so the power-cycle branch did not run) | remote | proven | `docs/frl.md`, `tools/frl/ccs13.py`, 2026-10-25 |
| xds110reset -s <serial> (does not reopen the post-power-on window and did not clear -242 after a flash read; it does start a DSLite-flashed image) | remote | failed | `cc1354p10/FAQ.md`, 2026-10-10 |
| uhubctl off 6 s on the probe's port of a USB-C adapter, then load at once (3 s off failed; never switch while a load is connected (Error -261)) | remote | proven | `cc1354p10/FAQ.md`, 2026-10-08 |
| Power-cycle the probe port during a retrying connect(), then halt() and loadBinaryProgram(<flash bytes from 0>, 0x0) (12 of 12 images on the external probe; leaves the CCFG alone) | remote | proven | `cc1354p10/FAQ.md`, 2026-10-24 |
| Start a loop of loads, then unplug and replug USB (in on the 21st try; a replug with no loop missed the window twice) | hands | proven | `cc1354p10/FAQ.md`, 2026-10-07 |
| Unplug USB for 3 s, flash again (for -242 after a flash read) | hands | proven | `cc1354p10/FAQ.md`, 2026-10-10 |

USB hub ports recorded for this board:

- The probe's hub port power cut not checked, 2026-10-25. The external LP-XDS110 probe leaves USB within 1 s of `uhubctl -a off` and is back about 1 s after `on`, and a CCStudio connect works at once after each cycle; whether the chip itself lost power was not measured, because every connect resets this chip and clears what would show it (SRAM, reset source).
- The probe's hub port power cut not checked, date not recorded. The hands-free load route in the blink README; no separate before-and-after check.
- The probe's hub port cuts power, 2026-10-08. 6 s off, then load at once, reopened the debugger window every time; 3 s did not. Never switch it while a load is connected: the probe then answers Error -261 until the next off and on.

Restore: CCStudio scripting loadBinaryProgram. Regions: 1 MB main flash and 2 kB CCFG at 0x50000000: back up both, restore both. (`cc1354p10/AGENTS.md`)

<!-- recovery:end -->
