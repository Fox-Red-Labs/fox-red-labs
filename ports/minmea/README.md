# minmea, a GPS sentence parser, on an MSPM0 LaunchPad kit with its files unchanged

Yes, with one missing call. [minmea](https://github.com/kosma/minmea) (MIT; it parses the NMEA sentences GPS modules print) builds for the LP-MSPM0G3507 with TI Arm Clang 5.1.1.LTS and the MSPM0 SDK 2.11.00.07, and the only thing TI's C library lacks is `timegm`. A 21-line `shim/timegm_shim.c` and a 3-line `shim/minmea_shim.h` answer it; minmea's own files are not edited (commit `c43c9e7`, cloned by the build). The linked image is 5.7 kB of flash and 628 B of RAM, including the soft-float code `minmea_tocoord` pulls in. **It ran on an LP-MSPM0G3519** (the same Cortex-M0+ family; the G3507 was a borrowed board and is not available now): all eight results read back over the debugger, and seven match the expected values exactly. The eighth, the latitude, reads 481172 against 481173 because the test program truncates a float when it scales the value; minmea parsed it right.

Terms: [vocab](../../docs/vocab.md). 1 kB = 1,024 bytes. A shim is a small file that answers the other project's function calls with TI's own. Checked 2026-10-14 on macOS (Intel).

## Build

```bash
bash build.sh
```

Builds minmea as published and stops at the link with the missing call:

```text
 timegm    <temporary object for minmea.c>
error: unresolved symbols remain
```

```bash
bash build.sh shim
```

Adds the shim and links: `text 5848, data 0, bss 628`. `DEVICE=mspm0g3519 bash build.sh shim` builds the same program for the LP-MSPM0G3519: `text 5848, bss 372`. The shim computes the seconds in 64 bits, because a 32-bit `long` overflows for dates after 2038. The board run below used an earlier 32-bit version (5.6 kB of flash); for its 1994 test sentence both versions give the same 764426119, and a Mac test of the fixed shim matches the expected values for 1900, 1969, 1994, 2000 and 2040. `SDK` and `TC` default to TI's install folders on macOS; set them to use other copies.

No SysConfig is needed: the program uses no peripheral. The build compiles the SDK's startup file for the chip (`startup_mspm0g350x_ticlang.c` for the G3507, `startup_mspm0g351x_ticlang.c` for the G3519) and links with the SDK's linker file (`mspm0g3507.cmd`, `mspm0g3519.cmd`). Use the startup file that matches the chip: `startup_mspm0g310x_ticlang.c` belongs to a smaller sibling and is the wrong one here. This script is the template for any plain-C library on an MSPM0 board.

## What it does on the board

`app/main.c` parses one `$GPRMC` and one `$GPGGA` sentence and leaves the results in globals, then sleeps. There is no LED or serial output, so the check is a debugger read after flashing `out/<DEVICE>/minmea.out`. Expected: `g_rmc_ok` 1, `g_gga_ok` 1, `g_ts_ok` 1, `g_sats` 8, `g_ts` 764426119 (1994-03-23 12:35:19 UTC, the same value macOS's `timegm` gives), `g_alt_dm` 5454, `g_lat_e4` 481173, `g_lon_e4` 115166 (degrees times 10,000). A visible result would need a UART print through DriverLib.

Run on the LP-MSPM0G3519 on 2026-10-14: flashed in 7 s with DSLite, the eight globals read with `DSLite memory -r 0x20200054,8`, every value as expected except `g_lat_e4` 481172 (the test program's `(int32_t)(x * 1e4f)` truncates 481172.97; `lroundf` would give 481173). The board's own program was backed up first (two identical 512 kB reads) and restored after; the read-back matched the backup.

## Using it for another plain-C library

- A library with no peripheral needs no SysConfig step. Copy this folder's `build.sh` and swap the source list: it compiles the startup file and links the linker file for the chip, which is all a plain-C library needs.
- With no LED or serial output, read result globals with a debugger after flashing, or add a UART print through DriverLib.
