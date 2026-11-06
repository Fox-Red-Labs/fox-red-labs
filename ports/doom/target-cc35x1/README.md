# Doom engine on the LP-EM-CC35X1 (CC3551E): demo 1 runs to frame 2030, demos 2 and 3 stop on zone memory

**Short answer.** The engine boots from the unchanged WAD in the flash (SHA-1 matches), runs at 50 to 66 frames/s and draws frames identical to the Mac build for demo 1 (checksum `8ceeaad8` at frame 400). It stops on a zone failure: demo 1 at frame 2030 of 5,025, demo 2 at frame 52, demo 3 at frame 425. The zone is 480 kB; the demos need more than the chip leaves (667.8 kB of data RAM less 191.6 kB of static data and RTOS). Demo 3 also diverges from the Mac frames between frame 100 and 200 (cause not found). Experiment page: [Doom, smallest TI chip, section 9](../../../docs/experiments/2026-10-20-doom-smallest-ti-chip.md). License terms: [LICENSING.md](../LICENSING.md).

Terms: [vocab](../../../docs/vocab.md). 1 kB = 1,024 bytes. **Zone** is the engine's memory pool; **WAD** is the game data file; **XIP** is execute in place, the external flash read through the address space.

## Memory the application can use (SDK 10.20.00.39, read 2026-10-20)

From `examples/rtos/LP_EM_CC35X1/drivers/empty/freertos/ticlang/cc35xx_freertos.cmd` and `source/ti/devices/cc35xx/inc/hw_memmap.h`:

| Region | Address | Size | Use |
|---|---|---|---|
| DRAM | `0x28000000` | 512 kB, less 0xDB0 for two shared blocks: 507.8 kB | Data. The data sheet's 1.1 MB total includes memory the Wi-Fi and Bluetooth cores use; the linker file calls this the "M33 DRAM allocation (baseline mode)" |
| TCM (data) | `0x20000000` | 128 kB when there is no external RAM (64 kB otherwise) | Data; the linker file prefers it for `.bss` and `.data` |
| CRAM | `0x00000000` | 32 kB | Code RAM; the vector table sits here, plain data also works |
| External flash, secure alias | `0x14000000` | the application slot | Code and read-only data execute in place; `.text`, `.rodata`, `.const` go here |
| PSRAM | `0x60000000` | none on this board | |

So 667.8 kB of RAM can hold data (507.8 + 128 + 32), and the single largest block is the 507.8 kB of DRAM. No PSRAM and no OTA slot are used. Switching off OTA in the Memory Configurator (`Mem_cfg.ota = false`) grows the application slot from 2,688 kB to 6,248 kB (`ti_flash_map_config.c`, `external_memory_configurator.json` in the build). The 4,097.7 kB WAD does not fit the OTA layout and fits this one.

External flash is memory-mapped: the 8 MB IS25WJ064F is read through `0x14000000` upward. The WAD is not written with a separate flash tool. It is a section of the application image: `wad_blob.S` includes `ports/doom/wad-cache/DOOM1.WAD` byte for byte, the linker puts it in `.rodata`, and the Toolbox image builder signs and programs it with the code. The firmware hashes the mapped bytes at boot and prints the SHA-1 (`5b2e249b9c5133ec987b3ea77596381dc0d6bc1d` is the right answer), which proves the flash holds the unchanged file. The image is never committed.

## What is here

| File | What it does |
|---|---|
| `build.sh` | Copies the SDK `uart2echo` example, switches OTA off, sets a 22 kB FreeRTOS heap and a 16 kB thread stack, compiles the engine with squeeze set S3, links with a linker file that is the SDK's plus a 4 kB libc heap, the zone placed first in DRAM and `.bss` allowed to spill into CRAM, then signs with the Toolbox. `./build.sh` makes the zone as big as the linker allows; `./build.sh 400` sets 400 kB. `DEMO=2 ./build.sh` fixes the demo at build time (one image per demo); without `DEMO` the firmware waits 4 s for a character on the serial port, which never arrived (see below). `HEAPHEX=0x4C00` sets the FreeRTOS heap (19 kB, used for the three demo images). |
| `dg_cc35x1.c` | Platform layer: 1 ms FreeRTOS tick, `vTaskDelay`, WAD glue on the mapped flash, the serial console (921,600 baud on the probe's port), a CRC-32 chain over the first 400 frames, an 80 x 50 downscaled frame every 50th frame up to frame 400, and a demo pick by one character from the Mac within 4 s of boot. Keys come from the built-in demo (`-timedemo demoN`), so `DG_GetKey` returns nothing. |
| `wad_blob.S` | Includes the WAD. |
| `sizes.py` | Flash and RAM by region from the linker map. |
| `flash.sh`, `run_demo.py` | Program the board (set `PROBE` in the script to your probe serial); reset, pick a demo, log text and frames. |
| `RESULTS-ROWS.csv` | The measured rows. |

## Build and run

```bash
cd ports/doom && ./fetch.sh                  # engine and WAD, SHA-1 checked
target-cc35x1/build.sh                       # image and signed Toolbox files in the build folder the script prints
ports/doom/target-cc35x1/flash.sh <build>/ex/freertos/ticlang/toolbox/tool_settings.json <log>
python3 -I ports/doom/target-cc35x1/run_demo.py 1 <prefix> 150
```

The board has no read-back, so the restore is to program the previous application again (for example the DNS sinkhole build, [step 4](../../dns-sinkhole/README.md)).

## Numbers (2026-10-20, 160 MHz, serial 921,600 baud)

| Demo | Zone | Checksum at frame 400 (Mac) | Frame rate | Ended |
|---|---|---|---|---|
| 1 | 480 kB | `8ceeaad8` (equal) | 61.5 frames/s over the whole run; 66.2 after frame 400 | Z_Malloc failed on 32.8 kB at frame 2030 |
| 2 | 480 kB | not reached (`6ea9fea2`) | 51 frames/s (52 frames) | Z_Malloc failed on 16.0 kB at frame 52 |
| 3 | 480 kB | `2780475f` (Mac `5c59f266`; frame 100 equal, frame 200 differs) | 42.6 frames/s (425 frames) | Z_Malloc failed on 32.8 kB at frame 425 |

Flash 4,397 kB: 4,097.7 kB of WAD and 298.9 kB of code, read-only data and init image. RAM: zone 480 kB; all other data 191.6 kB (TCM 128 kB full, CRAM 32 kB full, 27.1 kB of DRAM; that includes the 19 kB FreeRTOS heap, 4 kB libc heap, 2 kB interrupt stack). The engine's static data is already squeezed (S3); the largest pieces are visplanes 41.5 kB, view-angle table 16 kB and the thread stack 16 kB. Demo 2 needs about 488 kB of zone with lumps read in place (the 593 kB floor of the experiment page less 105 kB), so the 593 kB target is not reachable on this chip without moving level data out of RAM. Rows in `RESULTS-ROWS.csv`.

**Demo select does not work.** The firmware waits 4 s for one byte on the probe's serial port; the host sent it every 0.3 s and the driver returned 0 bytes in three runs (`PICKGOT 0`). Cause not found. The three demos are separate images, each chosen at build time (`DEMO=n`).

## What a screen needs

The Waveshare panel is a later step over wires. A frame is 320 x 200 indexed plus a 768-byte palette, handed to `DG_DrawFrame8`; the platform scales and sends it in bands over SPI (the engine itself renders at 320 x 200 whatever the panel is). The panel needs a free SPI controller and four to five header pins (clock, data, chip select, data/command, reset), a SysConfig SPI instance in `build.sh`, and keys on the header buttons or an input board. Send time is added to every frame, so the frame rate will fall below the engine-only figure.
