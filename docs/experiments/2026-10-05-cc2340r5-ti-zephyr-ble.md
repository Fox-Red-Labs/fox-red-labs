<!-- experiment
id: E3
board: LP-EM-CC2340R5
date: 2026-10-05
status: works
reached: verified
line: Partly works: with Zephyr's C library heap on (CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=2048; 1024 for TI's peripheral sample, which does not link at 2048), this repo's BTHome beacon and TI's peripheral sample run; TI's beacon sample hard-faults. At 0 bytes, the fork's setting for this board, the debugger shows TI's Bluetooth stack stopped in its assert handler under ICall_setTimer, read from the stack as a failed timer allocation (not stepped through).
stop: none
-->
# Does Bluetooth work on the TI LP-EM-CC2340R5 with TI's Zephyr fork?

**Short answer.** Partly: one line fixed two of three images. Turn on Zephyr's C library heap with `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=2048` in `prj.conf`. With it, two of three images ran on 2026-10-08: this repo's BTHome beacon and TI's peripheral sample were seen on air.

Why the heap matters: The fork's settings for this board leave the heap at 0 bytes, and the debugger's stack reads as TI's Bluetooth stack failing to get memory for its first timer and stopping before anything prints; the allocation was not stepped through.

The third image: TI's beacon sample started advertising, then stopped with a hard fault, and was not seen in one scan. Not chased.

| Flashed on 2026-10-08, heap on | Serial output | Seen on air by the Mac |
|---|---|---|
| This repo's BTHome beacon (heap 2 kB) | `FRL BTHome: bt_enable 0`, `adv start 0`, then one line per packet | Yes: "FRL Temp" at -35 dBm, BTHome v2 packets decoded |
| TI's `samples/bluetooth/peripheral` (heap 1 kB; 2 kB does not link) | `Bluetooth initialized`, `Advertising successfully started` | Yes: "Zephyr Peripheral Sample Long" at -35 dBm |
| TI's `samples/bluetooth/beacon` (heap 2 kB) | `Beacon started, advertising as 40:F3:B0:xx:xx:xx (public)`, then `***** HARD FAULT *****` | No: 0 devices named "Test beacon" in one scan |

Terms: [vocab](../vocab.md) (Zephyr, CCFG, CRC, west, BTHome). Errors and fixes: [CC2340R5 FAQ](../../cc2340r5/FAQ.md#zephyr). 1 kB = 1,024 bytes. The register-reading script is [`data/e3/halt_regs.py`](data/e3/halt_regs.py).

Words used below:

- **Heap**: memory a program asks for while it runs, through `malloc`. Zephyr's setting `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE` sets how many bytes `malloc` can hand out; at 0 every request fails.
- **Assert handler**: the function a program calls when one of its own checks fails. TI's Bluetooth stack's handler (`halAssertHandlerExt`) ends in a loop that waits for a debugger, so the program stops there without a message.
- **PC and LR**: two processor registers. The PC (program counter) holds the address of the instruction running now; the LR (link register) holds the address the current function will return to.
- **ICSR**: a register in every Arm Cortex-M chip that shows which interrupt or fault handler is running. 0 means none: the program is stuck in ordinary code, not in a crash handler.
- **addr2line**: a GNU tool that turns a code address into the function name and source line, using the built `zephyr.elf` file.
- **ICall**: the layer in TI's Bluetooth stack that passes messages and timers between the stack and the application.
- **Hard fault**: the Cortex-M's catch-all crash, raised for example by a read from an invalid address. Zephyr prints `***** HARD FAULT *****` and stops.

## The suspected cause, from the debugger (2026-10-08)

**If you see nothing on serial and nothing on air from a Bluetooth build on this fork, add `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=2048` to `prj.conf`** (or pass `-DCONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=2048` after `--` on the `west build` line). For TI's peripheral sample use 1024: at 2048 the link fails with this line:

```text
ld.bfd: region `RAM' overflowed by 346 bytes
```

To see the value your build used, run `grep MALLOC_ARENA build/zephyr/.config`.

This repo's beacon image from 2026-10-07, built with the heap at 0, was halted three times, one second apart, with CCStudio 21's scripting and the probe attached (`halt_regs.py`). The PC stayed in one loop every time (0x146b8, 0x146b8, 0x146bc), with ICSR 0, so no fault handler was running. `arm-zephyr-eabi-addr2line` on that image's `zephyr.elf` maps the PC, the LR and the return addresses on the stack to this chain (the frames that matter; `List_remove`, one more frame, is left out):

| Address | Function |
|---|---|
| 0x146b8 (PC) | `AssertHandler`, the assert loop in the fork's `drivers/bluetooth/hci/hci_ti.c`, line 146 |
| 0xd641 (LR) | `halAssertHandlerExt`, TI's Bluetooth assert handler |
| 0x15e23 | `ICall_abort` |
| 0xc9f9 | `ICall_setTimer` |
| 0x124ad | `osal_msec_timer_cback`, the stack's timer callback |

`ICall_setTimer` gets its memory from `malloc`. The fork's builds of TI's beacon, TI's peripheral, hello_world and this repo's beacon all had `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=0` in their `.config`, so the first timer request fails and the stack aborts. This is a reading of the call chain; the allocation was not stepped through.

**The fix was tested with one change.** Rebuilt with the heap at 2 kB and nothing else changed, this repo's beacon ran twice: once with the CCFG byte at 0x4E02001C changed to 0x07 and once as built, with the fork's own 0x0F. TI's two samples were also flashed as built, with 0x0F. So with the heap on, the CCFG byte change described below is not needed for these Bluetooth images.

**Two things seen and not chased:**

- TI's beacon sample: `>>> ZEPHYR FATAL ERROR 0: CPU exception on CPU 0`, `Faulting instruction address (r15/pc): 0x0000bf3c`, 0.122 s after boot, right after `Beacon started`. The address was not mapped to a function.
- TI's peripheral sample prints `<err> bt_ctlr_hci_driver: Failed to send buffer to device` and `<err> bt_hci_core: send_cmd: Unable to send 0x2025 to driver (err -2)` at start, then initializes and advertises. 0x2025 is the Bluetooth command that reads the chip's public key for pairing; pairing was not run.

## How it was done

**Question.** Upstream Zephyr 4.4.2 has no Bluetooth for this board. TI keeps its own Zephyr copy with Bluetooth. Does it give a builder a working Zephyr Bluetooth path?

**Vendor position.** TI's `simplelink-zephyr`, tag `v3.7.0-ti-1.00.01_ga` (2026-09-28), is a general-availability release with a Bluetooth driver (`drivers/bluetooth/hci/hci_ti.c`).

**What was tried.** That tag on an Intel Mac with macOS 15.7.9, 2026-10-05 to 2026-10-08, in a trimmed workspace (915 MB). This repo's setup differs from TI's: GCC 12.2 from the Zephyr SDK; version 1.0.0 of `lief` (a Python library TI's checksum tool uses to edit the built file) instead of the 0.12.3 the tool pins, because 0.12.3 would not install on this Mac's Python (**Tool or docs**); and a local fix to an objcopy path hard-coded to TI's SDK location (**Tool or docs**). Built `samples/bluetooth/beacon`, `samples/bluetooth/peripheral`, `samples/hello_world` and this repo's BTHome beacon (`ports/bthome/zephyr/`). Flashed `zephyr.hex` with UniFlash 9.6.0 (`dslite.sh --mode flash -e -f -u`), probe attached. Build steps and workarounds: `cc2340r5/zephyr-ti/README.md`.

**How far it got.**

- Builds, heap on: this repo's beacon 110.5 kB of flash and 30.5 kB of 36 kB of RAM; TI's beacon 110.3 kB and 30.5 kB; TI's peripheral, heap 1 kB, 202.9 kB and 35.3 kB (98.2% of RAM). Flash is out of 512 kB.
- Heap at 0 (2026-10-05 and 2026-10-07): every Bluetooth image flashed with `Success` and then printed nothing and sent nothing, with or without the CCFG byte change below, including a retry on 2026-10-07 with this repo's broadcaster-only beacon, the smallest Bluetooth build, with 7.6 kB of RAM free.
- Heap on (2026-10-08): this repo's beacon and TI's peripheral sample booted, printed their start-up lines and were seen on air from the Mac; TI's beacon sample printed `Beacon started`, then stopped with `***** HARD FAULT *****`, and was not seen in one scan.

**The CCFG byte, from 2026-10-05.** One byte of the boot configuration area (CCFG, at 0x4E02001C) differs from the upstream Zephyr 4.4.2 image that runs on this board: the fork sets 0x0F, upstream 0x07. Changing it to 0x07 and recomputing all four checksums with upstream's `soc/ti/simplelink/cc23x0/ti_ccfg_crc32_calculate.py` made `hello_world` print `Hello World! lp_em_cc2340r5/cc2340r5` (observed 2026-10-05; the serial capture was not saved). With the heap on, the Bluetooth images ran with 0x0F, so the byte did not stop them. Why hello_world needed the change on 2026-10-05 was not rechecked.

**The line.** Partly works: with the C library heap above 0, this repo's BTHome beacon and TI's peripheral sample run on this fork; TI's beacon sample hard-faults (not chased). The silent start at heap 0 reads from the debugger's call chain as a failed timer allocation (not stepped through).

- Why TI's beacon sample hard-faults at 0x0000bf3c with the heap on. To locate it, map the address with `arm-zephyr-eabi-addr2line -f -e build/zephyr/zephyr.elf 0xbf3c`.
- Whether `hello_world` runs with the fork's own CCFG byte 0x0F when flashed and reset the same way as the 2026-10-08 runs.
- Pairing on the peripheral sample, given the 0x2025 error at start.
- The heap size each image needs: only 2 kB and 1 kB were tried.

**Ask.** Has anyone seen TI's Zephyr fork beacon sample hard-fault on an LP-EM-CC2340R5 with the heap turned on, or found the heap size TI intends for these samples?
