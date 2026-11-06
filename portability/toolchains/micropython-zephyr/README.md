# MicroPython on Zephyr for a TI LaunchPad: MSPM0G3519 and MSPM33C321A

The MSPM33C321A runs MicroPython (REPL at 115200 baud); the MSPM0G3519 does not link. Terms: [docs/vocab.md](../../../docs/vocab.md). Checked 2026-10-17, build only, nothing flashed. MicroPython `9a3a0c0` (17 Oct 2026) `ports/zephyr`, Zephyr 4.4.2, GCC 12.2 from Zephyr SDK 0.17.4 through `ZEPHYR_TOOLCHAIN_VARIANT=cross-compile`. MicroPython has no TI port of its own; this is the generic Zephyr port on a TI board.

| Board | Result | Why |
|---|---|---|
| lp_mspm0g3519 (Zephyr describes 256 kB flash, 64 kB RAM) | does not link | FLASH overflowed by 33.9 kB, RAM by 19.1 kB |
| lp_mspm33c321a (1 MB, 256 kB) | builds, links and runs (2026-10-25) | The alloca.h stop was the missing picolibc module (`west update picolibc`). Zephyr `main` has no UART, GPIO or clock setup for the board, so the image needs TI's open pull requests; with them MicroPython's own `prj.conf` links at 292.3 kB of flash and 83.6 kB of RAM and the REPL answers at 115200 baud. Result, files and the commands: `ports/micropython/zephyr-mspm33c321a/README.md` |

The port needs a board file that turns the watchdog module off when the board has no `watchdog0` alias: `boards/lp_mspm0g3519.conf` here (`CONFIG_WATCHDOG=n`). Without it the build stops in `machine_wdt.c`.

The smaller MSPM0 and CC23xx parts have less memory than the G3519, so they cannot hold it either. Command: `west build -b lp_mspm0g3519 <micropython>/ports/zephyr` with the two toolchain variables above.
