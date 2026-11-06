<!-- experiment
id: E219
board: LP-MSPM0C1104
date: 2026-10-19
status: works
reached: verified
line: A program cut off 40 bytes short faulted before main, so the chip reset in a loop and hid its core; TI's wait-for-debug hold on the power access port stopped the core, and the out-of-box image went back and read identical (all 16 kB).
stop: none
-->
# Why does an LP-MSPM0C1104 stop answering its debugger after a flash write that stops short, and how do you bring it back?

The program in flash faulted before `main`, and on MSPM0 a fault inside the fault handler resets the chip. It reset in a loop, so the core's memory port was never up long enough to answer. TI's wait-for-debug hold on the power access port stopped the core at its first instruction, and the out-of-box image went back and read identical. NONMAIN was never changed.

Terms: [docs/vocab.md](../vocab.md). More on this chip: [LP-MSPM0C1104 FAQ](../../mspm0c1104/FAQ.md). **AP** is an access port behind the chip's debug port: AP0 the memory port, AP1 configuration, AP2 the mailbox (DSSM), AP4 power. **NONMAIN** is the configuration flash the boot ROM reads. **nRST** is the reset pin, which the XDS110 can drive. **DSSM** is the debug mailbox the boot ROM reads commands from. **TRM** is TI's technical reference manual, here SLAU893C. A **GEL** file is the script CCStudio runs for a device. A **vector catch** stops the core at its first instruction after a reset. DHCSR and DEMCR are the Arm core's debug control registers; DPREC0 and SPREC are registers in the power port; BOOTDIAG is a boot status word in the configuration port. 1 kB = 1,024 bytes.

| What was suspected | Test | Result |
|---|---|---|
| Sector 0 erased, no vector table | Dump flash with the core halted | Partly: the vector table is there, but bytes 0x380 to 0x3A7 of a 936-byte image were never written. The startup code calls 0x380 and the fault handler sits at 0x384, both erased. **The cause**; why the write stopped short is inferred (below) |
| NONMAIN corrupted, debug turned off | Read NONMAIN | Ruled out: it holds TI's defaults, byte for byte the SDK's default boot configuration |
| SWD pins taken by the program | Debug port reads | Ruled out: the debug, configuration and mailbox ports answered every time; the program never reaches `main` |
| A low-power mode | Power-port reads, upstream OpenOCD's low-power handling | Ruled out: the power port showed the chip resetting, not asleep |
| Probe firmware | `xdsdfu -e` | Ruled out: 3.0.0.43, the version CCStudio 21.0.1 and UniFlash 9.6.0 ship |
| Wrong access port in OpenOCD | TRM Table 22-1; upstream and xPack configs after the fix | Ruled out: both connect normally once the chip runs |
| Reset wiring or jumpers | The XDS110's pin readout | The jumpers were not the cause: the probe itself was holding nRST low, left asserted by an earlier connect-under-reset, and `adapter deassert srst` released it |

**Question.** An LP-MSPM0C1104 answers its debug port and names itself through the configuration port, but every tool fails at the core (OpenOCD: "Could not find MEM-AP"; DSLite: "Connection to MSPM0 core failed") and a DSSM mass erase does nothing. Why, and can it be brought back with TI's documented tools?

**Vendor position.** The C-series technical reference manual (SLAU893C, July 2025):

- Section 3.3.3: "On MSPM0 devices, a processor lockup is considered a fatal fault which always triggers a SYSRST to clear the lockup condition and restart the system." Its first example: "The processor cannot fetch the stack pointer or reset vector at reset."
- Table 22-2: with nRST held, processor debug and memory access are N; debug status through the debug port is Y.
- Table 22-3: with "Debug Disabled" in NONMAIN, the debug port, configuration port and mailbox stay on and the memory port is off. That is the same picture as a reset loop, so the port pattern alone cannot tell the two apart.
- Table 1-6: the MSPM0C1104 uses NONMAIN layout Type B, "No BSL support". Section 1.4.4.3: its BOOTCFG3 holds `FACTORYRESETCMDACCESS` and no mass-erase policy.
- Table 22-10 lists Mass Erase 0x020C and Factory Reset 0x020A for MSPM0 in general, and section 22.1.2 says a mass erase DSSM command can be sent to regain access.

CCStudio 21.0.1's `ccs_base/emulation/gel/mspm0c1104.gel` says, for both mass-erase menu items: "Mass Erase is not available for this device." The same file's `MSPM0_ResetCtl_WaitForDebug()` writes the power port: `PWRAP_SPREC |= 0x1`, `PWRAP_DPREC0 |= 0x4000`, `PWRAP_SPREC |= 0x1`, ten times. Upstream OpenOCD's `tcl/target/ti/mspm0.cfg` (its header: "Copyright (C) 2023-2025 Texas Instruments") offers `mspm0_mass_erase` for every MSPM0. The LP-MSPM0C1104 user's guide (SLAU908B) routes the XDS110's reset output to the chip's PA1/NRST through J101 pins 9 and 10.

**What was tried.** Intel Mac, macOS 15.7.9, 2026-10-19 19:13 to 19:32. Upstream OpenOCD built from git (`bb5a52da`) with the CMSIS-DAP driver, a bare debug-port configuration (no target) so that nothing reset the chip, and the XDS110 MC110001 on firmware 3.0.0.43. Every write is listed; everything else was a read.

1. Read the debug port and every access port. The XDS110 reported `nRESET = 0`. AP0, AP3 and AP4 read 0; AP1 gave DEVICEID 0x3BBA102F; BOOTDIAG read 0.
2. `adapter deassert srst`. The pin read 1; AP4 came back (IDR 0x002E0002); AP0 answered once (0x84770001) and then faulted on 30 of 30 reads over 10 s. The power port's DPREC0 alternated between 0x00600003 and 0x00620003: the chip was resetting.
3. TI's wait-for-debug sequence from the GEL file (power port writes only). AP0 then read 0x84770001 on every try, with DHCSR 0x02000000: core held in reset.
4. While held: DHCSR = 0xA05F0001 (debug on), DEMCR = 1 (stop on core reset), then DPREC0 bit 14 cleared to release the hold. DHCSR read 0x00030003 and DFSR 0x8: the core stopped at its reset vector. Memory now read normally.
5. Dumped main flash twice (identical), NONMAIN and SRAM.
6. Wrote the board's out-of-box backup image with `flash write_image erase` and `verify_image`, read it back, compared with `cmp`, cleared the catch, pulsed nRST.

Steps 3 and 4 are the whole halt: a few OpenOCD register writes, no erase. The sequence was run again end to end on the working board: halted at the out-of-box reset vector (0x204D), released, LED toggling.

**How far it got.** Verified. All 16 kB written in 6.2 s and verified; the read-back is identical to the backup (sha256 `9cce3aa1…c78b`). PA22, the red LED, toggles every 487 to 535 ms, read 120 times over 4.9 s without halting (out of the box on 2026-10-08: 495 ms). Upstream `board/ti/mspm0-launchpad.cfg` and xPack OpenOCD 0.12.0-7 both connect normally afterwards.

**The line.** Nothing stopped the recovery. What locked the board:

- Flash held the first 896 bytes of a 936-byte LED program (the color-player demo). It was written by a browser flasher whose RAM loader assumed 4 kB of RAM and put its data buffer at 0x20000080. The flasher now reads each board's RAM size. A 936-byte buffer there runs exactly 40 bytes past the end of the chip's 1 kB of RAM at 0x20000400, and the 40 missing bytes are 0x380 to 0x3A7. The match is inferred; the page's log of that write was not saved.
- The image's startup code (`_c_int00_noinit_noargs` at 0x31C) sets the stack and calls `_system_pre_init` at 0x380: erased (0xFFFF). The fault goes to `HardFault_Handler` at 0x384: also erased. A fault inside the fault handler is a lockup, and the chip resets (TRM 3.3.3). It does that within microseconds of every reset, so the memory port is never up for long.
- A reset pulse on nRST only starts the same loop again.
- This chip does not answer a DSSM mass erase (0x020C); CCStudio's GEL lists it as not available here. Its boot ROM read it from the mailbox (TXCTL went from 0x20D to 0x20C) and never answered.
- On 2026-10-19 the probe was also holding nRST low, left asserted by an earlier connect-under-reset attempt. With nRST held, Table 22-2 rules out memory access, so every connect failed the same way until `adapter deassert srst`.

BOOTDIAG (configuration port offset 0x10) read 0x07 both during the loop and after the restore with the program running, and 0x00 while nRST was held. CCStudio's GEL lists 0x07 as "Nested exception ... 2) Debug access has been disabled". Since it reads 0x07 on a working board too, it was not used as evidence here.

The C1104 has no ROM bootloader (Table 1-6), so after any reset it goes straight to whatever is at 0x0.

- CCStudio's own connect with its `EnableWaitForDebug` property, which runs the same power-port hold on connect. It may connect to this state with no script.
- A DSSM factory reset (0x020A), the C1104's documented command. It erases main flash and resets NONMAIN. Per Table 1-6 and section 3.3.3, a blank vector table (0xFFFFFFFF) also locks up, so the core should still need the wait-for-debug hold afterwards. Not tested.
- A browser flasher that uses the same power-port hold before it puts back a saved copy. A write that stops half way on this chip leaves exactly this state.

**Ask.** Has anyone recovered an MSPM0C110x from a program that faults before `main` with CCStudio or UniFlash alone, and which option did it take? Does a factory reset on a C110x leave a core you can attach to without the wait-for-debug hold?
