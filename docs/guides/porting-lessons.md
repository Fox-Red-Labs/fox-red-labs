# What do you change when you port a project to a TI LaunchPad kit such as the LP-MSPM0G3519 or LP-MSPM33C321A? Lessons from 16 ports and 4 BoosterPack drivers (2026-10-21)

Terms: [docs/vocab.md](../vocab.md). Chip by chip: [chip-capability-map.md](chip-capability-map.md). 1 kB = 1,024 bytes.

Nine groups of change came up again and again, and four of them cover most of the work:

- Put each piece of data in the memory that suits it. Doom on the LP-EM-CC35X1 went from 22.5 to 34.5 frames/s that way (36.7 with two more placements, 39.2 with the -O2 compiler setting on top).
- Stream a screen instead of buffering it. 609 B of RAM drove a 128 x 128 panel.
- Check the clock. A 32 MHz default left a Game Boy at 6.7 frames/s.
- Flash by a route that works. A padded binary loaded where the ELF stopped at 91 percent.

The guide covers 16 ports and 4 BoosterPack drivers built or run on 2026-10-20 and 21 on MSPM0, MSPM33, AM13 and CC35X1 LaunchPad kits, on a Mac. Numbers link to the port or experiment that measured them where a link sits beside them. Each table says which chip and SDK it is about. Next: pick your chip in [chip-capability-map.md](chip-capability-map.md), or jump to [the ten steps](#start-a-new-port-in-ten-steps).

Tested with MSPM0 SDK 2.11.00.07, MSPM33 SDK 1.04.00.00 and TI Arm Clang 5.1.1.LTS.

Short forms and terms:

- LP-MSPM0G3519 is G3519, LP-MSPM33C321A is M33 or MSPM33C321A, LP-AM13E230 is AM13, LP-EM-CC35X1 is CC35X1.
- TCM is the fast tightly coupled RAM. Zone is Doom's memory pool. ELF is the linked program file with symbols. ECC is error-correcting memory.
- DSLite and OpenOCD are flashing tools; Toolbox is the CC35X1 flashing app. STANDBY0 is a deep sleep state; a wake source is what ends it. NPU is the chip's neural processing unit.
- UBSan is the compiler's undefined-behavior checker. lwIP is the small TCP/IP stack. SWCLK is a debug pin. SYSPLL is the clock multiplier. OTA is over-the-air update.
- The [vocabulary page](../vocab.md) has the rest.

## The changes that came up again and again

Each group has the ports it applied to. Open a group for the before and after numbers.

<details>
<summary><strong>1. Memory placement</strong>: Doom, ZX Spectrum, llama2.c, Game Boy, wasm3, MicroPython, MP3, uLisp</summary>

The question is always which region holds which bytes. Measure the cost per region before blaming the code.

| Port | Change | Before | After |
|---|---|---|---|
| [Doom on CC35X1](../../ports/doom/target-cc35x1-whx/README.md) | Statics, zone, render list and thread stack into the 128 kB fast RAM (TCM) | 22.5 frames/s, same code | 36.7 frames/s (34.5 from placement alone, 39.2 with -O2). A load costs 1.06 cycles in TCM, 1.6 in code RAM, 6.2 in DRAM, 44 from external flash |
| [ZX Spectrum on G3519](../../ports/zx-spectrum/README.md) | Z80 core copied to SRAM bank 1 with one pragma and one linker word | 15.9 percent of a real Spectrum | 18.9 percent |
| [Doom on MSPM33C321A](../../ports/doom/README.md) | State and object tables `const` (stay in flash); render limits cut to the demos' maxima; wipe and output buffer removed | 295.6 kB static RAM (the first port build, not the stock 164 kB figure below) | 159.8 kB (`.data` 58.9 to 20.2 kB) |
| [Doom on MSPM33C321A](../../ports/doom/m33-whx/README.md) | Page cache (256-byte pages) over QSPI, which has no mapped window | data file 1.7 MB cannot be addressed | 98.3 to 99.1 percent hits at 8 to 19 kB; 23.8 microseconds a miss; 45.7 ms a frame (21.9 frames/s) at 16 kB |
| [Doom on CC35X1](../../ports/doom/target-cc35x1/README.md) | Application slot with over-the-air layout off | 2,688 kB slot | 6,248 kB slot for a 4,097.7 kB game file read in place |
| | Weights as a `const` array in flash; key and value cache sized to the steps run | 655 kB cache at 512 steps | 160 kB at 128 steps; 21.2 tokens/s on the board |
| [Game Boy](../../ports/peanut-gb/README.md) | 2-bit shades packed four to a byte; ROM read where it sits in flash | 23 kB unpacked picture | 5.6 kB |
| [wasm3](../../ports/wasm3/README.md) | Fixed heap in `.bss`, no `malloc` | G3507 needs a 16 kB heap | 6 kB heap and 1 kB Wasm stack: 11.1 kB RAM in all |
| [minimp3](../../ports/minimp3/README.md) | Reserve stack for one decode call | 4 kB stack overflows silently | 32 kB reserved; one call keeps a 17.3 kB frame |
| [uLisp](../../ports/ulisp/README.md) | Workspace sized against the stack | n/a | 2,800 objects (21.9 kB) leave 8.9 kB for the C stack on a 32 kB part |

</details>

<details>
<summary><strong>2. Display streaming without a frame buffer</strong>: Sharp panel, touch games, Arduboy, Game Boy, ZX Spectrum, CHIP-8, Doom</summary>

A color frame is larger than the RAM of most of these chips (a 320 x 480 RGB565 frame is 300 kB). Every port that drew a screen sent bands, dirty rows or tiles.

| Port | Technique | Result |
|---|---|---|
| [Sharp 128 x 128 on C1104](../../docs/experiments/2026-10-20-mspm0c1104-sharp128-1kb.md) | Draw and send a few rows at a time on a chip with 1 kB RAM | 609 B RAM with 1-row bands (153.7 ms a picture); 849 B with 16-row bands (41.1 ms) |
| [Touch games](../../ports/touch-games/README.md) | Redraw changed tiles through one 3 kB buffer | A 2048 swipe sends 66,459 pixels on average instead of 153,600 |
| [Arduboy layer](../../ports/arduboy/README.md) | Keep the 1 kB one-bit buffer, stream 4 lines at a time | 1 kB instead of a 32 kB RGB565 frame |
| [ZX Spectrum](../../ports/zx-spectrum/README.md) | Compare each 8-line band with a shadow copy, send only changed bands | about 1 ms a frame at BASIC idle; 35 ms when every band changes |
| [CHIP-8](../../ports/chip8/README.md) | Dirty-row mask, one 320 x 5 blit per row | 14.2 ms full redraw |
| [Game Boy on M33](../../ports/peanut-gb/README.md) | Scale inside the line callback, send rows by DMA | 2.85M to 0.59M instructions per drawn frame (frame skip); 0 late frames in 8,167 at 59.7 frames/s with the other steps |
| [Doom on M33](../../ports/doom/m33-whx/README.md) | Two 256-byte row buffers, the DMA end interrupt starts the next row | 80 rows in 14.6 ms in the background; the engine waited 2.7 ms in 7,585 frames |

On the G3519, polled sending and DMA gave the same full-screen fill, 67.2 ms, at a 40 MHz SPI clock. DMA pays when the CPU has other work, as in Doom and the Game Boy.

</details>

<details>
<summary><strong>3. Clock and timers</strong>: Game Boy, MicroPython, MP3, llama2.c, C1104, CC35X1</summary>

- **The 32 MHz default.** The SysConfig clock tree on the MSPM33C321A gave 32 MHz. The first Game Boy image ran there at 6.7 frames/s (8 to 13 percent of the console). Setting `PLL_QDIV` 10 and `EXTDIVMCLK` divide value 1 gave 160 MHz. An extra divide by 2 after the PLL gives 80 MHz even with the PLL set, so read `CPUCLK_FREQ` in the generated header.
- **PLL lock.** The MSPM0G3519 MicroPython port retries the PLL up to 20 times (the SYSPLL errata loop) and falls back to 32 MHz. It computes the baud divisor from the real clock.
- **Counters that are not there.** SysTick does nothing on the MSPM0C1104; the port counts milliseconds with TIMG14. The Cortex-M0+ has no cycle counter, so MicroPython builds `ticks_cpu` from SysTick. `CYCCNT` reads 0 on the CC35X1; Doom profiles from the RTOS tick plus SysTick.
- **Counters that wrap.** A 32-bit cycle counter wraps in 26.8 s at 160 MHz (a "30 s" timer in llama2.c restarted every 3 s). The 24-bit SysTick wraps at 16.8M cycles, and one MP3 frame costs about 16M cycles on the M0+.
- **SPI input clock.** SPI1 on the G3519 runs from MCLK (80 MHz), not the 40 MHz bus clock.

</details>

<details>
<summary><strong>4. Flashing traps</strong>: G3519, CC35X1, M33, C1104, AM13</summary>

| Symptom | Cause tag and fix | Port |
|---|---|---|
| DSLite stops near 91 percent with a verify mismatch at 0x27FB2 on the LP-MSPM0G3519 (macOS) | Tool or docs (suspected; not isolated): flash a raw binary padded to 16 bytes at 0x0 instead of the ELF | [MicroPython](../../ports/micropython/README.md) |
| OpenOCD reads device ID 0 after an image that sleeps at once | Flash with DSLite (cause not found) | MicroPython |
| CC35X1 console silent after programming an image with a different flash layout | Setup or handling: run the Toolbox `full_flash_erase`, then program again; check 115200 against 921600 baud | [CC35X1 FAQ](../../cc35x1/FAQ.md) |
| `dslite load` restarts the program | Tool or docs: place a mailbox outside every linked section; DSLite counts `-r addr,len` in 32-bit words | [QSPI dump](../../docs/experiments/2026-10-21-mspm33c321a-qspi-dump-doom-data.md) |
| A second OpenOCD attach resets the C1104 | Tool or docs: flash, run and read in one connection | [Sharp128](../../portability/boosterpacks/sharp128/README.md) |
| DSLite `-e` on the AM13 | Tool or docs: `-e` means verbose; erase scope is chosen separately | |

</details>

<details>
<summary><strong>5. Numeric differences</strong>: llama2.c, MP3, NPU filters, Doom</summary>

- **libm drift.** The Arm C library differs from the Mac's in the last bits. In llama2.c, with activations re-quantized to int8, logits differed by up to 0.12 by token 38. Greedy output matched for 128 tokens; sampled output split at token 39. Compare greedy first.
- **SIMD against scalar.** minimp3's SSE path differs from its scalar path by 1 in a few hundred samples. The M33 build (no fused multiply-add) matched the scalar Mac build: 194 of 194 frame checksums, maximum sample error 0.
- **Eight-bit weights.** NPU filters matched an integer reference byte for byte, but end to end the signal is about 35 dB (34.6 to 35.4) because the output floors at 8 bits; the taps alone gave 42.7 to 53.5 dB.
- **A checksum that moves with the optimization level.** In Doom, -O2 changed demo 3 until an unwritten stack array was zeroed (see item 7).
- **Group size.** In llama2.c, `hidden_dim` 172 does not divide by 64; `runq.c` drops the remainder with no error and the text looks plausible. The check is a greedy match against float32.

</details>

<details>
<summary><strong>6. Licenses and data files</strong>: Doom, ZX Spectrum, Game Boy, CHIP-8, DNS blocker</summary>

- Ports commit converters and build scripts, not data. The Doom engine is GPL-2.0 and lives in `ports/doom/` only; the shareware game file is fetched, SHA-1 checked and never committed. The converted file is made by a host tool.
- The ZX Spectrum 16 kB system ROM is copyrighted. The build takes `ZX_ROM=<file>` from the reader and otherwise runs a ROM-less test image.
- The Game Boy port fetches two freely licensed ROMs and stops on a hash mismatch. The CHIP-8 test suite (GPL-3.0) is fetched, not stored. The blocklist for the DNS blocker is downloaded and the 349 kB table is built locally.
- A dump of the board's own flash, which holds the kit's video, stays outside the repository.

</details>

<details>
<summary><strong>7. Upstream bugs the ports found</strong>: Doom, Game Boy, wasm3</summary>

| Project | Bug | Evidence | Fix |
|---|---|---|---|
| rp2040-doom | `draw_composite_columns` draws entries of `pixels[129]` it has not written, so a pixel depends on earlier stack contents | 11 of 3,881 frames differed by 20 pixels between two builds | Zero the array per column; frame checksum chain `fc7b6c2d` at -Os, -O1 and -O2; without it demo 3 at -O2 gave `53ccda64` |
| Peanut-GB 1.3.0 | MBC3 clock index `cart_ram_bank - 0x08` runs past a 5-byte array; header size codes index tables of 9 and 6 entries | UBSan: `index 9 out of bounds for type 'const uint16_t[9]'`; AddressSanitizer misses intra-struct overflows | Bounds-check patch on a build copy; the front end refuses the ROM header |
| wasm3 0.9.0 | `op_CallRef` lacks the native stack check; `memory.grow` adds page counts in 32 bits and wraps; a constant expression runs with a NULL memory header | Host tests: unpatched call_ref recursion aborts, patched traps with `stack overflow`; unpatched grow leaves size 0x3FFFFFFF | Three patches on a build copy; draft reports not filed |

wasm3's default native stack budget is about 8 MB and never trips on a part with a 4 to 8 kB stack, so set `d_m3MaxNativeStack` to the linker stack minus 512 bytes.

</details>

<details>
<summary><strong>8. Low-power traps</strong>: MSP430 examples on L1306 and AM13</summary>



- **The debugger changes the state under test.** On the L1306, OpenOCD left the debug power request set (measured on one board): STANDBY0 showed 15.1 million SysTick ticks a second, the same as SLEEP. After `mspm0x.dap dpreg 0x04 0x00000000` the same image gave 33.8 thousand.
- **Read the wake-source table first.** On the LP-AM13E230, STANDBY0 with only the watchdog timer as a wake source never woke, and no debugger connected afterwards (DSLite -6310, -6311, -613; OpenOCD -613). The SDK's own example wakes STANDBY from a GPIO. An image that sleeps at once cannot be debugged, so wait 5 s awake at boot.
- **UART in deep sleep.** A 32 kHz-clock UART kept 5 of a 38-byte burst; the bus-clock UART in STANDBY0 echoed 190 of 190.
- **Habits from FRAM chips that break on flash.** A flash counter becomes an append log in an erase sector (word write 56 microseconds, 1 kB sector erase 1.99 ms on the L1306); a counter updated every second lasts about 148 days in one sector (100k erases at 128 counts per erase). Two sectors used in turn close the power-loss window between an erase and the first program; they do not change wear.
- **A hub power-off is not a power cycle on every board.** It restarted the L1306 and left the AM13 running (measured on one unit of each; see the MSP430 port notes).

</details>

<details>
<summary><strong>9. Security fixes in network code</strong>: DNS blocker on CC35X1</summary>

Each forwarded query uses its own socket on a random port with a random-case name (0x20 encoding). A reply is accepted only on that socket, from the upstream on port 53, with a matching ID and an exact-case question. Queries from outside the board's subnet are dropped and relayed replies are capped at 1.2 kB. Blocked names still answered in a median 6 ms and forwarded names in 67 to 76 ms, with a 71,492-domain table of 349 kB. See [the DNS blocker experiment](../../docs/experiments/2026-10-20-cc35x1-dns-sinkhole.md). The lwIP raw UDP callbacks run on the tcpip thread, so no blocking calls go inside them.

</details>

## What did not work, and why

| Project | Chip | Result | Cause tag |
|---|---|---|---|
| Doom, stock engine | MSPM33C321A | Links; needs 164 kB static and an 800 kB zone | Chip limit (RAM, 256 kB) |
| Doom, any build | MSPM0G3519, MSPM0G3507 | Static data alone does not fit | Chip limit (RAM, 128 kB and 32 kB) |
| Doom, stock engine | CC35X1 | 50 to 66 frames/s, then `Z_Malloc` fails at frame 2030, 52 and 425 in the three demos | Chip limit for this engine (zone, 480 kB used) |
| ZX Spectrum at full speed | MSPM0G3519 | 15.9 to 19.4 percent of a real Spectrum, five times short of the 20 percent CoreMark predicted | Chip limit (CPU; the Z80 tick moves a 64-bit pin word) |
| MP3 in real time | MSPM0G3519 | 7.7 to 10.7 times real time (modelled); 25 times the M33's cycles a frame | Chip limit (no FPU) |
| Game Boy with the 256 kB ROM | MSPM0G3507, MSPM0G5187 | Link fails 140.4 kB short on 128 kB flash; 0.7 to 1 kB RAM left | Chip limit (flash, RAM) |
| MicroPython full build | MSPM0G3507 | 173.9 kB, 45.9 kB over the flash; the minimal build (72.9 kB) fits | Chip limit (flash) |
| BOOSTXL-AUDIO | MSPM0L1306 | The amplifier line (pin 13) is SWCLK | Board layout (pin 13 is a debug pin) |
| EDUMKII, SHARP128, BASSENSORS | CC35X1 | Eight pins (SHARP128: three) cannot be freed with a jumper | Board layout (header pins) |
| Zephyr shields | MSPM0 | Zephyr 4.4.2 (checked 2026-10-21) has no MSPM0 SPI driver; main builds, nothing flashed | Tool or docs (no driver in Zephyr 4.4.2) |
| Deep sleep with a watchdog wake | AM13E230 | Never woke; debugger locked out | Unknown (suspected: Setup or handling, wake source) |
| TMP117 and BMI160 | MSPM0G5187 | No I2C answer; cause not found (suspected: unfitted headers) | Unknown |

Two early blocks were fixed: the Game Boy at the 32 MHz default clock (6.7 frames/s, now 59.7 paced with the other steps) and the Doom data file that the MSPM33C321A could not address in place (a page cache over QSPI now serves it).

## Questions

**Does Doom run on the LP-MSPM0G3519?** No. The engine's static data alone exceeds its 128 kB of RAM. It runs on the LP-MSPM33C321A (21.9 frames/s, display stubbed) and the LP-EM-CC35X1 (39.2 frames/s).

**Why does DSLite stop at 91 percent loading my LP-MSPM0G3519 image?** The ELF verify failed at 0x27FB2; the cause is suspected, not isolated. Flash a raw binary padded to 16 bytes at 0x0.

**Why is the LP-EM-CC35X1 console silent after programming?** The flash layout changed. Run the Toolbox `full_flash_erase`, program again, and check 115200 against 921600 baud.

**Why does a sleeping LP-AM13E230 never wake and refuse the debugger?** Its only wake source was the watchdog; DSLite then fails with -613, -6310 and -6311. Pick a wake source from a TI example for the same chip and state.

**Why does my LP-MSPM33C321A run a game at 6.7 frames/s?** The SysConfig clock tree gave 32 MHz. Set `PLL_QDIV` 10 and `EXTDIVMCLK` divide value 1 for 160 MHz. The clock alone does not reach 59.7: the Game Boy port also needed a table scaler, DMA rows and frame skip.

**Doom stops with `Z_Malloc failed on 32792 bytes`.** The zone (the engine's memory pool, 480 kB here) is too small for the demo; on the LP-EM-CC35X1 this happened at frame 2030, 52 and 425 of the three demos. Use the rp2040-doom build, which needs a 30 to 40 kB zone.

**Doom stops with `Couldn't realloc lumpinfo`.** The engine allocates its lump directory (34.6 kB) outside the zone and the default heap is 2 kB. Patch it into the zone.

**uLisp stops with `#error "Board not supported!"`.** Add one board block to a build copy, not the sketch (uLisp notes).

**Zephyr stops with `undefined node label 'spi1'`.** Zephyr 4.4.2 has no MSPM0 SPI driver; use Zephyr main ([TRAPS.md](../../portability/boosterpacks/TRAPS.md)).

**The Toolbox fails with error -260 on a LaunchPad with a BoosterPack stacked.** It cleared after the pack was removed and the cable replugged; the cause is not confirmed ([the BoosterPack plug-in experiment](../experiments/2026-10-20-boosterpack-on-at-plug-in.md)).

**My Cortex-M0+ firmware halts in the NMI handler with `SRAMDED` set.** Seen on a Zephyr blinky on the LP-MSPM0G3519. Cause not established. The first SRAM bank has error correction (ECC), and Zephyr's board file maps only that 64 kB bank; one untested idea is to write RAM with 32-bit stores before byte stores.

**Does a 2 ms sector erase let me log every second?** Not forever: flash takes 100k erases in the lower 32 kB, about 148 days for a once-a-second counter kept as an append log (128 counts per erase) in one sector. Two sectors in turn do not add wear life; they close the power-loss window between an erase and the first program.

**Which problems have no entry above?** Each is in a table: the MicroPython full build is 45.9 kB over the G3507's flash (minimal fits); the 256 kB Game Boy ROM does not link on the G3507 and G5187; minimp3 with a 4 kB stack overflows silently; llama2.c with `hidden_dim` 172 gives plausible, wrong text; a 32-bit cycle counter wraps in 26.8 s at 160 MHz; a second OpenOCD attach resets the C1104; `dslite load` restarts the program; BOOSTXL-AUDIO pin 13 is SWCLK; the G5187 TMP117 and BMI160 gave no I2C answer; a 32 kHz-clock UART loses bytes in deep sleep.

**How much RAM does a port need before it is worth trying?** Budget it first (step 3 below): static data plus the deepest stack plus any cache. Doom's static data alone ruled out 128 kB.

**What does built against ran mean here?** Built: the image linked. Ran: it executed on the board and a number was read. The chip map shows Should work for built-only.

**Which BoosterPacks fit which kit?** [PINS.md](../../portability/boosterpacks/PINS.md).

## Start a new port in ten steps

1. **Run the unmodified source on the Mac** with a fixed seed and keep the output. Every later change is compared with it.
2. **Write the expectation** (flash, RAM, speed) before the first build. Wrong guesses caught real limits: sector erase took 2 ms, not the expected 5 to 40 ms.
3. **Budget memory first**: largest array, stack of the deepest call, any cache that grows with run length. Read the linker map for the real region sizes.
4. **Check the clock and the counters**: read `CPUCLK_FREQ`, find a counter that exists on the chip and does not wrap during the run.
5. **Remove files, stdio and `malloc`**: static arrays, a stub for `fopen`, no frame buffer.
6. **Make runs comparable**: one checksum per frame or token, a virtual clock, a fixed seed. Compare Mac, emulator and board.
7. **Check the toolchain**: count FPU instructions in the disassembly; build the Mac reference as scalar; compare checksums across -O levels.
8. **Flash by a known route**: padded binary with DSLite, a full erase after a layout change, one OpenOCD connection per run. Before a deep-sleep image, find its wake source in a TI example for the same chip.
9. **Fuzz what a file controls**: run ROM, model or packet parsers under UBSan with crafted headers.
10. **Record**: licenses and fetch scripts, a results row per number, a trick-map entry (`data/porting`), and a row for every test that did not run, with its reason.

A one-page porting pack lists the calls a project makes, the tested replacement and the traps. A fresh AI assistant ported a UART bootloader six of six checks with and without it. With the pack it took a median 19 turns against 27, but three runs a side is too few to call that a difference or to separate the cost (the porting pack race).
