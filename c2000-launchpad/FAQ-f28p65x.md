# What goes wrong with the TI LAUNCHXL-F28P65X LaunchPad development kit on a Mac, and how do you fix it?

Every problem hit while bringing up a new TI LAUNCHXL-F28P65X (chip TMS320F28P650DK9: dual-core C28x, 200 MHz, 1.28 MB flash, 248 kB RAM; onboard XDS110 probe) from a Mac terminal on 2026-10-08. Each answer starts with the fix, names what caused it, and quotes the exact error text so a search finds it. Versions: Intel Mac, macOS 15.7.9, CCStudio 21.0.1, C2000 compiler 25.11.1.LTS, C2000Ware 26.02.00.00 LTS, probe firmware 3.0.0.43. 1 kB = 1,024 bytes. This board shares most of its tooling with the TI LAUNCHXL-F2800137; see that board's own FAQ for problems common to C2000 LaunchPad development kits in general.

**What caused it**, one tag per answer. The tags used on this page:

| Tag | Meaning |
|---|---|
| Setup or handling | How the hardware was plugged in or set up |
| Tool or docs | A TI tool bug, a missing tool, or a missing or wrong instruction |
| Chip limit | The hardware cannot do it |

Words used below (more in [docs/vocab.md](../docs/vocab.md)): the **probe** is the XDS110 debugger built into the LaunchPad. A **connection file** (`.ccxml`) tells TI's tools which probe and chip to use. The **PIE** is the C28x's interrupt controller that routes peripheral lines to the CPU. The **CLA** is a second, smaller processor on some C2000 chips, one per CPU on this one; nothing on this page uses it.

Board overview and measured numbers: [README](README.md). Contents: [Connecting](#connecting), [Porting from another C2000 chip](#porting-from-another-c2000-chip).

## Connecting

### My hand-written connection file fails every operation with "An invalid processor ID has been found." What is wrong?

Start from TI's own connection file for this chip, not a single-core board's file. This chip adds a second processor (a CLA, control law accelerator) on each of its two CPUs, and a connection file built the way a single-core C2000 board's is (one entry pointing at the device database) fails every operation, including a plain probe identify, with `Failed: An invalid processor ID has been found.` The fix is C2000Ware's own file, `device_support/f28p65x/common/targetConfigs/TMS320F28P650DK9.ccxml`: copy it and add only the probe's serial number. It adds one driver line the single-core template lacks (`drivers/tixds510cla2.xml`) and narrows the connection to one subpath instead of the full default device tree.

### My probe's firmware is below 3.0.0.43. Do I need to update it for this error?

No, not for this one. The probe's firmware was updated from 3.0.0.26 to 3.0.0.43 along the way (the same update the F2800137 needed, with TI's `xdsdfu` tool), and the "invalid processor ID" error was unchanged before and after. The fix is the connection file above, not the firmware. Update the firmware anyway: CCStudio 21.0.1 and UniFlash 9.6.0 ask for 3.0.0.43 or newer before they will do anything else.

## Porting from another C2000 chip

### I ported a test from another C2000 board. It builds and runs with no error, but it never does the one thing it is supposed to measure

Check whether the test pends an interrupt by writing a specific bit number, and reread this chip's own `hw_ints.h` before trusting that number. An interrupt-latency test was ported from the LAUNCHXL-F2800137 that forces its own spare interrupt (`INT_FREERTOS`) from software, by writing a hard-coded PIE flag bit, to time how long the CPU takes to reach the handler. On the F2800137 that interrupt is PIE group 12, line 6. On this chip it is PIE group 12, line 12 (`driverlib/f28p65x/driverlib/inc/hw_ints.h`, `INT_FREERTOS 0x00DB0C0CU`). The program linked, ran and reported a clean result, except the result was always "zero samples": the handler never fired, because the test was pending bit 6 while the chip's own vector table was listening on bit 12. There is no compiler error for this, because the macro name is the same on both chips and only its value differs. To find the real line for any `INT_xxx` macro, read the macro's own value: the low byte is the line, the next byte the group (`0x00DB0C0CU` decodes to line 12, group 12).
