# What goes wrong on the TI LP-AM243 LaunchPad development kit from a Mac, and how do you fix it?

Checked 2026-11-08 with MCU+ SDK for AM243x 12.01.00.33, TI Arm Clang 4.0.4.LTS, SysConfig 1.27.0 and CCStudio 21.0.1 on an Intel Mac, probe firmware 3.0.0.15. Each answer starts with the fix and names what caused it.

**What caused it**, one tag per answer: Our mistake, Setup or handling, TI example default, Tool or docs, Chip limit, Third-party behavior, Unknown.

## Power and connecting

### DSLite lists the cores but every connect fails with error -1170 at CS_DAP_0, and a red LED is on. What is wrong?

Give the board a 5 V, 3 A USB-C charger of its own, then unplug and replug the micro-B debug cable. The red LED is LD9, the power-error light. The board does not use USB PD: it reads the charger's current advertisement on the USB-C CC pins and turns on its 5 V rail only at 3 A (TI user guide SPRUJ12F section 3.1). Some dual-port chargers drop each port to 1.5 A when both are in use. The probe has its own supply, so it still answers while the chip has no power. LD9 off and LD7, LD6 and LD8 on mean the rails are up. Seen 2026-11-05 on a shared dual-port 65 W charger; the fix with a single 3 A source is not yet confirmed here.

### Do I have to change the boot switches to load a program?

No. Run `/Applications/ti/ccs2101/ccs/scripting/run.sh am243x/tools/ram_run.js <image.out>` with the board as it came. It writes the program to RAM and starts it, and flash is left alone, so a power-cycle returns the board to its shipped firmware. TI's documented route is debug boot: SW4 switches 1 to 4 ON and 5 to 8 OFF, then a power-cycle. CCS's `memory.loadProgram` call hangs in the shipped mode, which is why the script writes the program's segments straight to RAM instead.

### Zephyr stops with "Synchronous External Abort" and a faulting instruction in `slice_reset`. What is wrong?

Start it with `AM243X_BARE=1 am243x/tools/ram_capture.py <zephyr.elf> <log>`. `lp_am243` images set up no memory protection or cache of their own: they expect TI's bootloader to have done it. Without it the first exclusive load on on-chip RAM faults. The stub in `ram_run.js` sets those up first, and blinky then toggles repeatedly on the console (interval not timed) with no flash or switch change.

### A program prints nothing after another one ran, and the debugger finds it in `HwiP_irq_handler_c`. Why?

Load it with `am243x/tools/ram_run.js`, which clears this first. A reset of the core leaves TIMER8 and the interrupt controller as the last program set them. When that timer's match interrupt stays on, TI's SDK clears only the overflow bit, so the handler runs forever. Zephyr's timer driver leaves it this way.

## Building

### The bootloader examples stop in SysConfig with "Cannot read properties of undefined (reading 'configurables')". How do I build them?

Install SysConfig 1.27.0 beside the newer one and build with `SYSCFG_PATH=/Applications/ti/sysconfig_1.27.0`. The SDK's `imports.mak` names 1.27.0. With 1.28.0 and CCStudio's 1.28.1, `sbl_null`, `sbl_uart`, `sbl_uart_uniflash`, `sbl_ospi` and `ospi_flash_diag` fail on the flash module; with 1.27.0 all five build. The macOS installer runs unattended: `installbuilder.sh --mode unattended --prefix /Applications/ti/sysconfig_1.27.0`. Checked 2026-11-05.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-11-08. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

No lockup is recorded for this board.

<!-- recovery:end -->

## Console, flash and SDK

### The console stays empty after a load. Which port is it, and when do I open it?

Open `/dev/cu.usbmodem<serial>1` (list them with `ls /dev/cu.usbmodem*`; the XDS110's second port, 115200 baud; on this board `<serial>` is `S24L0096`) before the load, or use `am243x/tools/ram_capture.py <image.out> <log>`, which does both in order. A program that prints its result once at start is missed by a reader that opens the port later (`am243x/tools/capload.py` exists for this reason). In the SDK sweep, 7 of 121 loaded examples printed nothing in 30 s (`am243x/SWEEP.md`); most examples there need wires, a BoosterPack or a network, so silence usually means that, not a fault.

### Which SDK examples erase flash or burn fuses?

`ospi_flash_diag` erases the first flash block: its source calls `OSPI_norFlashErase`. Do not run it on a board whose flash matters. Do not run any OTP key writer: it burns fuses and cannot be undone (TI user guide SPRUJ12F section 2.6). Neither was run here. A RAM load with `am243x/tools/ram_run.js` writes no flash.

### Why does the flash backup repeat after 32 MB?

The debugger's window onto the flash at `0x60000000` repeats after 32 MB, so a backup read through it proves the first 32 MB of the 64 MB part only. The first backup, 2026-11-05, was read twice and the hashes matched; the contents end near 17 MB. Nothing here has written to the flash.

### How do I get the SDK on a Mac?

TI ships MCU+ SDK for AM243x 12.01.00.33 as a Linux `.run` and a Windows `.exe`, with no Mac installer. Run the Linux installer in an amd64 Linux container, then copy the folder to `~/ti`; the result builds with CCStudio 21.0.1 and TI Arm Clang 4.0.4.LTS on an Intel Mac (673 MB installed, 777 MB after a build). An Apple Silicon Mac was not tried.

### How do I make my program survive a power cycle?

This page's route does not: a RAM load is gone after a power cycle, which returns the board to the firmware in its flash. TI's route is a bootloader (`sbl_ospi`) in the flash with the application at offset 0x80000, or debug boot with SW4 switches 1 to 4 ON and 5 to 8 OFF. Both build (`am243x/prebuild.sh`); no flash write and no switch move has been run on this board.

### Why is a pin I toggle from C slow, and does the AM243 run Linux?

A GPIO store takes 147 cycles (0.184 us) on an R5F core here against 10.02 cycles on the AM263P (`gpio_toggle` rows). A PRU core writes its output register in 5.3 ns (`pru_toggle_ns`), by its own cycle counter; no scope has seen the pin. The AM2434 in this ALX package has no DDR (SPRSP65J Table 4-1), so there is no room for Linux here; the R5F cores run bare-metal, FreeRTOS or Zephyr.

### Why is CoreMark 2,931 here when TI's page lists 3,015.63?

Our number is TI's own example, -O3, code in OCSRAM, three runs, with the clock checked against the Mac's at 799.98 MHz (`data/results/am243x.csv` row `coremark` `ti-port-o3-ocsram`). The cause of the 2.8% gap is not isolated. TI Arm Clang 5.1.1, the copy inside CCStudio, scores 3,151 on the same source.

## Several cores and the SDK examples

### TI's `ipc_notify_echo` ran once and then hangs in `IpcNotify_syncAll` at every later start. What do I do?

Run `am243x/tools/sysreset.js` (the debugger's System Reset) and wait 12 s before each start; no power cycle is needed. Then load the M4F and the three R5F cores first (`AM243X_CORE=Cortex_M4F_0` or `Cortex_R5_0_1`, `Cortex_R5_1_0`, `Cortex_R5_1_1` with `am243x/tools/ram_run.js`), then core 0 with the console open (`am243x/tools/ipc_run.sh`). Without the chip reset, a core reset before each load, emptying the eight hardware mailboxes (`am243x/tools/mbox_drain.js`) and loading core 0 first did not change the hang in `IpcNotify_syncAll`; with it, nine starts in one power-up all echoed (2026-11-08). Which state survives a core reset is not known.

### Which SDK examples write to the board's EEPROM?

`i2c_memory_read`, `i2c_memory_read_interrupt_lld` and `i2c_memory_read_polling_lld` write ten bytes (0 to 9) at offset 0x400 of the I2C EEPROM at address 0x50 and read them back; the FreeRTOS variant printed a failure in the sweep because its EEPROM write returned an error. Skip them unless that EEPROM is scratch for you. `i2c_read` only reads; offset 0 reads 0xFF on this board. The other example families the sweep ran (`am243x/SWEEP.md`) do not write the EEPROM; flash and bootloader examples were not run.

### A core built on its own from a multi-core example stops at the link with "memory range not found: USER_SHM_MEM". How do I build it?

Run the system project's `syscfg` step, then build each core with `SYSTEM_FLAG=true` (`am243x/tools/ipc_build.sh` does this for `ipc_notify_echo` and builds all five cores). The shared-memory ranges are defined by the system project's SysConfig; without that step the core's linker file lacks them. Checked 2026-11-06: 19 of the 194 projects fail this way when built one core at a time.

## Watching the chip run (live chip view)

### Which browsers and boards does the live chip view work on for the LP-AM243?

It does not run in a browser yet, and the boards it works on are the MSPM0 LaunchPad kits. The LP-AM243 view is read by `am243x/tools/live_probe.js`, run from Node with the board's XDS110, and drawn by a page that follows the file it writes. The browser view works on the MSPM0 LaunchPad kits; see the [LP-MSPM0L1306 FAQ](../mspm0l1306/FAQ.md). [E318](../docs/experiments/2026-11-07-am243x-live-chip-view.md)

### Does the live view halt or slow my program? Does it change a power measurement?

It does not halt the core, but connecting a debugger to the core does, so the view reads only through the debug port's system view. Reads of the running chip cost the program no instructions. Do not take a power measurement while the debug connection is attached, because it may change what the chip does in sleep (suspected, not measured). [E318](../docs/experiments/2026-11-07-am243x-live-chip-view.md)

### On the AM243, why does CPU load need a change to my code?

The Cortex-R5 cores have no program counter sample register, and the chip's sleep status bit reads 0 from the debug port. `DBGDIDR` reads 0x77040013 with bit 13 clear and `DBGDEVID` reads 0. Replace a bare WFI with `frl_wfi()` from `am243x/bench/chipview/frl_load.h`, which counts time asleep in TCMB where the debug port can read it. Under FreeRTOS use `frl_pmu_load.h`, which stamps the cycle counter from a timer interrupt, because the first method books the time other tasks ran as sleep. [E318](../docs/experiments/2026-11-07-am243x-live-chip-view.md)

### I read a variable in OCSRAM through the debug port and it never changes. Why?

The R5F is probably holding the value in its data cache (suspected; not tested). Put the variable in TCM, which is not cached, or read it from the core's own view while it is halted.

### Does the live view work with my own AM243 program?

Power, pad settings, pin levels and timer counts read with no change to the program. CPU load, task load and the enabled interrupt lines need the program to copy a counter or mask into TCM, so a program you cannot change shows no load. [E318](../docs/experiments/2026-11-07-am243x-live-chip-view.md)

### The SDK's lwIP example prints "link_callback==UP" for both netifs, but nothing answers a ping. What do I check?

Do not trust that line; it comes from lwIP, not from the PHY. Build `source/networking/enet/core/examples/lwip/enet_lwip_cpsw` for `am243x-lp`, and print `EnetExtPhy_isLinked()` for each port (the example's `ghExtPhy[]`). On 2026-11-08 PHY address 3 (MAC port 1) was linked at 1 Gb full duplex and PHY address 15 (MAC port 2) was not linked, while both netifs reported UP. Then check the far end of the cable that has link. DHCP got no lease in 100 s and a static address on the house subnet did not answer a ping, so that cable did not reach the house network (`am243x/bench/logs/enet/`). A second start of the example without `am243x/tools/sysreset.js` first stops on `hCpsw->hRxRsvdFlow != NULL`; reset the chip and wait 12 s.

### The AM243 has link lights and the example says UP, but DHCP never gives it an address. What do I do?

Check the other end of the cable: it must go to a switch or a mesh node, not to another board. A board-to-board cable gives link lights and a `link_callback==UP` and still no lease, because no DHCP server is on the wire (2026-11-08, the AM243 cabled to an AM62L). Plug into the house node and read the link per port. On 2026-11-08 after that recable the PHY at address 15 (MAC port 2) was linked, the one at address 3 (MAC port 1) was not, and 130 s later both netifs were still 0.0.0.0 with no answer from the reserved addresses, so the lease is still unsolved; making the port-2 netif the lwIP default changed nothing, and a static <board address> on it got 0 of 5 pings with the board's MAC absent from the node's client list, so frames from the board do not reach the network (`am243x/bench/logs/enet/static205-port2.log`). A static address left at the SDK default can also collide with a device you own, so set both before the first run; with only port 1 cabled to the node the probe's CPSW counters (`am243x/tools/cpsw_stats.js`) showed port 1 sending 1,167 frames in 150 s and receiving 12 broadcast frames, no multicast and no unicast, on a busy house network; the second counter block matches the chip's host port, not a loop (its receive count equals what the host sent). Frames leave the chip and almost none arrive, so the PHY registers read through the example's own MDIO handle show 1 Gb full duplex with the partner's receiver OK and no receive errors in the PHY or the CPSW, and taking 1000BASE-T out of the advertisement (register 9 bits 8 and 9 cleared, autonegotiation restarted; `am243x/bench/logs/enet/dhcp-100m-120s.log`) brought the link back up but still gave no lease. A link with no errors and no traffic is not a speed or timing fault we can see, so the guide (SPRUJ12F, section 4.7, page 36) puts the port 1 PHY behind a mux selected by GPIO0_27 on revision E3, and TI's example drives it to CPSW, which we read back high; GPIO0_70, the revision E2 pin, made no difference. Check what the far end does with the board's frames (client list, a second cable, a USB-Ethernet adapter on the same node port); try the other jack and a known-good cable, and check the node's client list for the board's MAC (`am243x/bench/logs/enet/dhcp-direct-mesh.log`). No throughput row exists yet.

### `am243x/tools/ram_capture.py` stops with "No module named serial". What do I install?

It needs pyserial and the Mac's own `python3` does not ship it. Make a virtual environment once (`python3 -m venv ~/venv-am243 && ~/venv-am243/bin/pip install pyserial`) and run the capture scripts with that interpreter. Seen on 2026-11-09 during the uLisp and post-quantum RAM loads.
