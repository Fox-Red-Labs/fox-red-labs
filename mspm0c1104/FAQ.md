# TI LP-MSPM0C1104 LaunchPad on a Mac: errors and fixes

Checked 2026-10-08 to 2026-10-25 on one LP-MSPM0C1104. Terms: [docs/vocab.md](../docs/vocab.md). 1 kB = 1,024 bytes.

## TI's blink example builds and flashes, but the LED does not blink

Check the board version printed on it. The red LED moved between board versions: version E2 wires it to pin PA4, version E3 and later to PA22 (user guide SLAU908B, Table 2-1, jumper J7). The SDK's `gpio_toggle_output` README for this board says "Example requires Rev E3 or later of the MSPM0C1104 LaunchPad" and toggles PA22. On an E2 board, change the LED pin to PA4 in `gpio_toggle_output.syscfg`, or watch PA22 on header J1 pin 8 instead. Jumper J7 must be on for the LED to light at all.

## Will a small AI model fit the MSPM0C1104

Not the room-occupancy models in this repo: the smallest needs 1.2 kB of RAM for its buffers and the chip has 1 kB. That is a link-only result from 2026-10-08, before the board arrived, for these models as built; BitNetMCU's 1k handwritten-digit model does link (7.2 kB of code, 804 B of RAM).

## The cycle counter reads zero on the MSPM0C1104

SysTick does nothing on this chip. The SDK header `mspm0c110x.h` names `SysTick_IRQn`, but reading the SysTick registers with a debugger (`mdw 0xE000E010 4`) gives `0 0 0 0` and a write to LOAD and CTRL does not stick, so the cycle counter used on the other MSPM0 boards in this repo reads zero. Use a general-purpose timer on the CPU clock instead: start TIMG8 as a free-running 16-bit counter (clock `DL_TIMER_CLOCK_BUSCLK`, no divide). Checked against `delay_cycles(30000)`, it reads 30,010 counts. It wraps every 66k cycles, so time only spans shorter than that.

## The reduced CoreMark builds link but hard fault

Not found. The 400-byte and 200-byte data-set builds link and flash, then stop in a hard fault inside `core_bench_list` with the stack pointer below the start of RAM (`0x1FFFFF30`), even with a 920-byte stack and 10 iterations. RAM painted with `0xA5A5A5A5` before the run shows the whole stack used. The standard 2 kB build does not link at all. Do not report a CoreMark score for this chip from these builds.

## The LED blinks before anything is flashed

Yes. The shipped out-of-box image blinks the red LED (PA22) about every 0.5 s; pressing the S2 button changes its mode. A pin read three times a second apart can show a steady level for a blinking LED, so sample well under the blink period: 25 ms works, read through the debugger without halting the chip.

## OpenOCD says "Could not find MEM-AP" and DSLite "Connection to MSPM0 core failed", but the chip's ID still reads

Halt the core through the power access port, then reflash. If the program in flash faults before `main` (for example a write that stopped short), the fault handler faults too, and the chip resets itself in a loop (TRM SLAU893C section 3.3.3). The memory port is never up for long, and a reset pulse only restarts the loop; this chip has no ROM bootloader to fall back to. First check OpenOCD's `nRESET =` line: if it reads 0, the probe is holding the reset pin from an earlier run, and `adapter deassert srst` releases it.

Then run TI's wait-for-debug writes from CCStudio's `mspm0c1104.gel` on the power access port (AP4), which holds the core in reset with the memory port up. Set a vector catch, release the hold, and the core stops at its first instruction. Nothing is written to flash. Save this as `recover_halt.tcl` (it needs upstream OpenOCD with the `cmsis-dap` driver; set your probe serial):

```text
proc c1104_rd {addr} { c1104.dap apreg 0 0x0 0x23000002; c1104.dap apreg 0 0x4 $addr; return [c1104.dap apreg 0 0xC] }
proc c1104_wr {addr v} { c1104.dap apreg 0 0x0 0x23000002; c1104.dap apreg 0 0x4 $addr; c1104.dap apreg 0 0xC $v }
proc c1104_release_nrst {} { adapter deassert srst; sleep 500 }
proc c1104_halt {} {
  for {set i 0} {$i < 10} {incr i} {
    c1104.dap apreg 4 0xF0 [expr {[c1104.dap apreg 4 0xF0] | 1}]
    c1104.dap apreg 4 0x00 [expr {[c1104.dap apreg 4 0x00] | 0x4000}]
    c1104.dap apreg 4 0xF0 [expr {[c1104.dap apreg 4 0xF0] | 1}]
  }
  sleep 100
  c1104_wr 0xE000EDF0 0xA05F0001
  c1104_wr 0xE000EDFC 0x00000001
  c1104.dap apreg 4 0x00 [expr {[c1104.dap apreg 4 0x00] & ~0x4000}]
  sleep 100
  echo [format "DHCSR 0x%08X (bit 17 set = halted)" [c1104_rd 0xE000EDF0]]
}
```

```bash
openocd -c "adapter driver cmsis-dap; adapter serial <probe-serial>; transport select swd; adapter speed 100; reset_config srst_only srst_nogate" -c "swd newdap c1104 cpu -expected-id 0; dap create c1104.dap -tap c1104.cpu" -f recover_halt.tcl -c init -c c1104_release_nrst -c c1104_halt -c shutdown
```

If the last line shows DHCSR with bit 17 set, the core is halted. Program with the normal target file, but skip its examine-start hook, which pulses a system reset:

```bash
openocd -c "adapter driver cmsis-dap; adapter serial <probe-serial>; transport select swd; adapter speed 100; reset_config srst_only srst_nogate" -c "swd newdap c1104 cpu -expected-id 0; dap create c1104.dap -tap c1104.cpu" -c "set DAP_SWD_ID 0x6ba02477" -f target/ti/mspm0.cfg -c "mspm0x.cpu configure -event examine-start {}" -c init -c halt -c "flash write_image erase image.bin 0x0 bin" -c "verify_image image.bin 0x0 bin" -c "mww 0xE000EDFC 0" -c "mww 0xE000EDF0 0xA05F0000" -c mspm0_board_reset -c shutdown
```

Checked on 2026-10-19 with xPack OpenOCD 0.12.0+dev.

## The board stopped answering after a ported example ran

If you see `Could not find MEM-AP to control the core` after running a G3507 DriverLib example moved to the C1104 (seen with `tima_timer_mode_pwm_dead_band` on 2026-10-24), try the halt above first. If the core never halts, the program faults on every boot: the boot diagnostic register (CFG-AP BOOTDIAG) cycles through values such as 0x07, which CCStudio's GEL reads as a nested exception, and the loop resets the debug logic, so no halt sticks and a USB unplug does not help. Then the way back is TI's DSSM factory reset (mailbox command 0x020A, OpenOCD's `mspm0_factory_reset`): it erases main flash and resets NONMAIN to TI's defaults, so save what you can first. Write `data/oob-flash.bin` (the shipped image, in this folder) back with OpenOCD and start it with an nRST board reset (`mspm0_board_reset`); a `reset run` from the halted state left the core in HardFault. Keep a flash backup before any ported example and run the images that end in a reset (a PWM or timer example) last. The cause of the loop is not established. The same recovery worked after two more ported examples on 2026-10-25.

## A DSSM mass erase does nothing on the MSPM0C1104

The C1104 does not respond to it. CCStudio's `mspm0c1104.gel` says "Mass Erase is not available for this device", and the TRM's NONMAIN layout for this chip (Type B) has a factory reset policy only. The boot ROM reads 0x020C from the mailbox and never answers (seen on 2026-10-15 and again after a replug). Factory reset (0x020A) is the command TI documents for this chip. It erases main flash and resets NONMAIN, and it recovered the board in the entry above.

## The probe says its firmware needs updating

Not for OpenOCD or DSLite here. This board's probe shipped with firmware 3.0.0.26 and read 3.0.0.43 on 2026-10-19. DSLite prints `Warning: A firmware update is required for the XDS110 probe. The current firmware is version 3.0.0.26. The probe must be upgraded to firmware version 3.0.0.43` on every call and still reads, flashes and verifies; OpenOCD printed no warning. If a tool refuses to connect instead, `xdsdfu -I <serial> -m` then `xdsdfu -I 00000000 -f firmware_3.0.0.43.bin -s <serial> -r`, run from `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/`.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-25. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- A program that faults before main (a write that stopped short): the fault handler faults too and the chip resets in a loop; MEM-AP is up only briefly. The chip has no ROM bootloader (TRM SLAU893C Table 1-6, no BSL support) (`docs/experiments/2026-10-19-mspm0c1104-lockout-root-cause.md`, `mspm0c1104/FAQ.md`, 2026-10-19)
- The probe left nRST held low after a connect under reset; every connect fails until it is released (`docs/experiments/2026-10-19-mspm0c1104-lockout-root-cause.md`, 2026-10-19)
- Second lockout after the ported tima_timer_mode_pwm_dead_band: the debug port answers, BOOTDIAG cycles (0x07 = nested exception, CCStudio GEL) and the loop resets the debug logic too, so AP0 flickers and power-AP holds do not stick; restored 2026-10-24 by DSSM factory reset (`mspm0c1104/AGENTS.md`, 2026-10-24)
- Third lockout after the ported smbus_smbus_controller (G3507 port): the debug port answers (DPIDR 0x6ba02477), the core is not examined (CPUID reads 0x0), the same signature as the second lockout; restored 2026-10-25 by DSSM factory reset, oob-flash.bin written back (2026-10-25)
- Fourth lockout after the ported L1306 spi_controller_internal_loopback_poll (it passed preflight; SPI0 on PA2, PA11, PA16, PA18): repeated external reset, the debug port answers (DPIDR 0x6ba02477) and no MEM-AP is found; recovered the same day by the DSSM factory reset (0x020A), oob-flash.bin written and read back equal, nRST start, PA22 497 ms (2026-10-25)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| Check OpenOCD's nRESET line; if 0, adapter deassert srst (for the held-nRST case) | remote | proven | `docs/experiments/2026-10-19-mspm0c1104-lockout-root-cause.md`, 2026-10-19 |
| bench/recover_halt.tcl (wait-for-debug writes, vector catch, halt at first instruction), then program with OpenOCD (for the reset loop) | remote | proven | `mspm0c1104/bench/recover_halt.tcl`, `docs/experiments/2026-10-19-mspm0c1104-lockout-root-cause.md`, 2026-10-19 |
| bench/recover_halt.tcl, about 7 variants and 72 halt-and-program retries (second lockout: DHCSR stays 0, the core never halts) | remote | failed | `mspm0c1104/FAQ.md`, 2026-10-24 |
| Unplug USB for 5 s, replug, run recover_halt.tcl again (one try) (second lockout) | hands | failed | `mspm0c1104/AGENTS.md`, 2026-10-24 |
| uhubctl power cycle of the probe's hub port, 30 s, four times (second lockout; the probe re-enumerated each time, whether VBUS dropped is unknown) | remote | failed | 2026-10-24 |
| DSSM mass erase (0x020C) (the C1104 does not answer it; CCStudio's GEL says mass erase is not available) | remote | failed | `mspm0c1104/FAQ.md`, 2026-10-15 |
| DSSM wait-for-debug (0x0206) through the SEC-AP mailbox, then nRST (bench/lockout_diag.tcl c1104_wfd) (second lockout: the ROM answers (RDR 0x00010003) but the loop continues; CPUID reads 0) | remote | failed | `mspm0c1104/bench/lockout_diag.tcl`, 2026-10-24 |
| Power-AP hold as OpenOCD's ti_mspm0.cfg does it (DPREC 0x104008, SPREC 1), 400 writes (second lockout: DPREC reads back its reset value) | remote | failed | 2026-10-24 |
| DSSM factory reset (0x020A): the command TI documents for this chip, erases main flash and resets NONMAIN; the core may still need the wait-for-debug hold afterwards (second lockout: the ROM answered (RCR 0x0A, RDR 0x00010003), the loop stopped, main flash read all 0xFF and NONMAIN TI's default; then write oob-flash.bin and start with an nRST board reset. Erases main flash; back it up first) | remote | proven | `mspm0c1104/bench/lockout_diag.tcl`, 2026-10-24 |

USB hub ports recorded for this board:

- The probe's hub port power cut not checked, 2026-10-24. Four 30 s cycles; the probe re-enumerated each time, whether VBUS dropped was not measured.

Restore: xPack OpenOCD (flash write_image erase, verify_image); back up before every ported example. Regions: main flash (oob-flash.bin); NONMAIN is left as is (factory reset policy only, Type B).

<!-- recovery:end -->
