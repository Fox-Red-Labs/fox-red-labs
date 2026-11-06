# Flashing a TI LP-EM-CC2755P10 or LP-EM-CC2340R5 with upstream OpenOCD through the XDS110

A flash driver (`cc23xx`) and target and board files that let upstream OpenOCD program a CC2340R5 or CC2755P10 through the XDS110's CMSIS-DAP interface. On an LP-EM-CC2340R5, `program blink.hex verify reset exit` erased, wrote, verified and restarted TI's blink in 0.37 s, and the LED pin toggled. A 511 kB image took 10.5 s to erase and write and 0.9 s to verify. The CC2755P10 path builds and is not run.

Terms: [docs/vocab.md](../../docs/vocab.md), [LP-EM-CC2755P10 FAQ](../../cc2755p10/FAQ.md), [LP-EM-CC2340R5 FAQ](../../cc2340r5/FAQ.md). **SACI** is the chip's device management interface on its security access port (SEC-AP, access port 2). **CCFG** and **SCFG** are the configuration areas at 0x4E020000 and 0x4E040000. **HAPI** is the boot ROM's function table at 0x0F00004C. 1 kB = 1,024 bytes.

## What is here

| File | What it is |
|---|---|
| `0001-flash-nor-cc23xx-ti-cc23xx-cc27xx.patch` | The whole change against OpenOCD commit `bb5a52da599c3dac61ddc51d2fa8c1166ee795d0` (master, 2026-10-16): the driver, its registration in `drivers.c`, `driver.h` and `Makefile.am`, and the four cfg files |
| `src/cc23xx.c` | The driver, the same file the patch adds as `src/flash/nor/cc23xx.c` |
| `tcl/target/ti_cc27xx.cfg`, `tcl/target/ti_cc23xx.cfg` | Target files: SWD DAP, Cortex-M target on access port 0, four banks on the CC27xx (main, CCFG, SCFG, HSM) and two on the CC23xx (main, CCFG), reset through SACI |
| `tcl/board/ti_lp_em_cc2755p10.cfg`, `tcl/board/ti_lp_em_cc2340r5.cfg` | Board files: `interface/cmsis-dap.cfg`, 4 MHz, the target file |
| `gate/` | Backup, read-only compare and one-sector test scripts (`*-cc23.tcl` are the ones run on the CC2340R5) |

## How the driver works

| Step | How | Source |
|---|---|---|
| Wake and connect | OpenOCD's own SWD connect: it retries with the dormant-to-SWD wake when the plain JTAG-to-SWD switch gets no answer | `src/target/adi_v5_swd.c`, `swd_connect_single` |
| Identify | DEVICEID and PARTID from the configuration access port (access port 1). Known, with DEVICEID's top 4 bits (the revision) masked: CC23x0R5 `0x0BB8402F` (not PARTID variant 0x9E), 512 kB; CC23x0R2 `0x0BB8502F`, 256 kB; CC27xxx10 `0x0BB9802F` with PARTID variant E8, DE, 9D, E6 or 31, 928 kB below the HSM area. Anything else is refused | CCStudio 21.0.1 `cc23xx_util.gel` |
| Reset | SEC-AP RSTCTL = 0x5C resets the chip into SACI. Then NOP, the debug check (CC27xx DEBUG_REQ_KEY_ID 0x1D, CC23xx DEBUG_REQ_PWD_ID 0x05; an answer that carries key or password words is refused), then EXIT_SACI_HALT and a halt in the boot ROM's debug wait loop (boot status 0xC1) with R3 = 0, or EXIT_SACI_RUN | TRM SWCU195A 9.3.2.2, SWCU193A 8.3.2.2, `cc23xx_xds.gel` `SecApReset` |
| Erase | One 2 kB sector at a time with the ROM's `HapiFlashSectorErase` (table entry 3), called on the halted CPU, cache off around the call as the SDK's `FlashEraseSector()` does | SDK 9.21 `driverlib/hapi.h`, `flash.h` |
| Write | The ROM's `HapiFlashProgram` (entry 5) from a 2 kB buffer in SRAM, never across a sector boundary, 16-byte aligned | same |
| Verify | OpenOCD's own `verify_image`; `cc23xx saci_verify <addr> <len> [crc32]` asks the chip for its own CRC32 or blank check (VERIFY_MAIN_SECTORS 0x10) | TRM 9.3.2.3.8 |
| Refusals | No chip erase and no main-application erase command is ever sent. The HSM bank (0xE8000 to 0xFFFFF) refuses erase and write. The CCFG and SCFG banks never erase, and a write goes through only when every byte equals the chip's (the CCFG user record, 0x750 to 0x7CF, is skipped, as TI's PROG_CCFG_SECTOR "skip user record" option does) | `cc2755p10/FAQ.md` |

Why the ROM and not SACI for erase: SACI has two erase commands, ERASE_CHIP and ERASE_MAIN_APP, and both erase every non-retained main sector. PROG_MAIN_SECTOR programs; every TRM flow erases before it. OpenOCD's `program` erases only the sectors an image covers, so a SACI-only driver would either erase too much or refuse. The ROM call erases one sector.

Two things to know when you use it:

- **An SDK hex fails `verify` by 128 bytes.** TI's SDK builds fill the CCFG user record (0x4E020750 to 0x4E0207CF) with 0x00, and a chip programmed from a read-back CCFG has it blank (0xFF). The driver does not write the user record, so `verify_image` reports `checksum mismatch` while main flash is correct. Program a hex with main flash only (or with the chip's own CCFG), or check with `flash verify_bank 0 <main.bin> 0`.
- **The debug port is left dormant on exit.** OpenOCD's own exit puts an SWD port in JTAG state. On these chips TI's DSLite, UniFlash and CCStudio then stop with `Error -615 ... The target failed to see a correctly formatted SWD header` until the board loses power. The target files run `cc23xx dormant` before shutdown, which puts the port back in the dormant state the chip powers up in; DSLite connected after that.

Why the image's CCFG must match: TI hex files carry the CCFG and the SCFG, and `program` writes every section in the file. A CC27xx opens its debug port only when both areas are valid (TRM SWCU195A 9.1.3), so this driver never changes them. TI's `drivers/empty` blink for the LP-EM-CC2755P10 and the Matter smoke alarm image differ only in the CCFG user record (0x00 fill against 0xFF), which is skipped; their SCFGs are identical.

## Build

Intel Mac, macOS 15.7.9, Homebrew in `/usr/local`, 2026-10-18. Times are wall clock.

```bash
brew install autoconf automake libtool pkg-config libusb hidapi jimtcl
```

```bash
git clone https://github.com/openocd-org/openocd.git && cd openocd && git checkout bb5a52da599c3dac61ddc51d2fa8c1166ee795d0
```

```bash
git apply <this folder>/0001-flash-nor-cc23xx-ti-cc23xx-cc27xx.patch
```

```bash
./bootstrap && ./configure --enable-cmsis-dap --enable-cmsis-dap-v2 --enable-xds110 --disable-werror && make -j8
```

| Step | Time |
|---|---|
| `git clone` | 4.8 s |
| `./bootstrap` | 4 s |
| `./configure` | 16 s |
| `make -j8`, first build | 37 s |
| Rebuild after adding the driver | 15 s |
| `make install` size | 9.6 MB (4.4 MB binary, 4.9 MB scripts) |

hidapi 0.15.0 and jimtcl 0.84 were new on this Mac (14 s and 47 s to install). OpenOCD master no longer builds its own jimtcl by default, so Homebrew's is needed.

## Use

Pick the probe by its serial when more than one XDS110 is plugged in:

```bash
openocd -c "adapter serial <XDS110 serial>" -f board/ti_lp_em_cc2755p10.cfg -c "program app.hex verify reset exit"
```

Read-only identity, without a reset:

```bash
openocd -c "adapter serial <XDS110 serial>" -f board/ti_lp_em_cc2755p10.cfg -c init -c "flash probe 0" -c "flash info 0" -c shutdown
```

The chip's own CRC32 over a range of whole sectors (it resets the chip into SACI and leaves it there until the next `reset`):

```bash
openocd -c "adapter serial <XDS110 serial>" -f board/ti_lp_em_cc2755p10.cfg -c init -c "cc23xx saci_verify 0x0 0x2000 <crc32>" -c "reset run" -c shutdown
```

## Test log

Run on 2026-10-18 on an LP-EM-CC2340R5 (CC2340R5, DEVICEID 0x1BB8402F, PARTID 0x803B2DDA) with its LP-XDS110ET, through CMSIS-DAP v2 at 4 MHz. The board held a Bluetooth tag image; it was backed up first and put back.

| Step | Result |
|---|---|
| Backup with DSLite, twice | Main flash 512 kB and CCFG 2 kB, both reads identical. The CC23xx has no SCFG |
| Read-only: identity and dump | DPIDR 0x6BA02477; AP0 0x84770001, AP1 0x002E0001, AP2 0x002E0000; CPUID 0x410CC601 (Cortex-M0+ r0p1). OpenOCD's 512 kB dump and CCFG dump identical to DSLite's, byte for byte |
| `reset halt` through SACI | 17 ms for the command; the CPU stopped in the boot ROM's wait loop (boot status 0x81, PC 0x0F002DC8) |
| One sector, 0x7C800 (sector 249, the highest blank sector below the image's top sectors) | Blank before. Write 2 kB of 0x0F: 34 ms, read back right. Erase: 10 ms, blank after |
| Does SACI PROG_MAIN_SECTOR erase first? | No. 16 bytes of 0xF0 programmed over 0x0F read back 0x00 (the AND), and the rest of the sector kept 0x0F. Result SUCCESS |
| `program empty.hex verify reset exit` (TI's `drivers/empty`, SDK hex) | Main flash written and right (`flash verify_bank`), CCFG equal and left alone; `verify` failed on the CCFG user record (above) |
| `program blink-main.hex verify reset exit` (same blink, main flash only) | Verified OK, 0.37 s for the whole command. Red LED pin DIO14 read through the debugger: on, off, on at about 1 s steps |
| Put the tag image back with OpenOCD (`flash write_image erase`, 511 kB) | Erase and write 10.5 s, verify 0.9 s. Read back with the CPU held in the boot ROM: main flash and CCFG identical to the backup |
| Read back with DSLite after the tag image booted | CCFG identical; main flash identical except 139 bytes at 0x7FC11 to 0x7FC9D, all in the last sector, all 1-to-0 changes: records the tag image appends to its settings sector when it starts |

The first restore attempt with DSLite failed to connect (`Error -615`), because OpenOCD had left the port in JTAG state; that is what `cc23xx dormant` now prevents.

Not run on the LP-EM-CC2755P10: the board was not available. The CC27xx differences (REQ_KEY_ID, the SCFG and HSM banks, 928 kB) are untested.

The scripts for the first board run are in `gate/`: `backup.sh <folder>` (DSLite backup of main flash, CCFG and SCFG, twice, compared), `ocd-readonly.sh <folder>` (identity, access port IDs, CPUID and a dump of main flash and CCFG through the AHB-AP, compared byte for byte with the DSLite backup) and `sector-test.tcl` (erase, blank check and write of sector 463 at 0xE7800, and the PROG_MAIN_SECTOR erase-first test). Keep the backup folder outside the repo: a flash image can hold network keys.

## Not done

- Nothing on a CC27xx: identity, the REQ_KEY_ID reset, the SCFG and HSM refusals, flash timing.
- The refusals on the CC2340R5 were exercised only for the CCFG (equal, skipped). A write that would change the CCFG, and the HSM bank, have not been tried on a chip.
- The CCFG user record is never written, so an SDK hex fails `verify` (above).
- Not supported: debug authentication (locked CCFG), CC27xxx7, CC27xxx15 and CC27xxx20, CC2340R22, CC2341R10 and CC2340R53 (refused by ID), writing the CCFG or SCFG, the CCFG user record, protection bits.
- Not upstreamed. The patch is in the shape of an OpenOCD change but has not been through OpenOCD's review or `checkpatch`.
