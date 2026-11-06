<!-- experiment
id: E184
board: LP-AM263P
date: 2026-10-11
status: works
reached: verified
line: The LP-AM263P's PRU generates a parallel RGB565 bus from a frame buffer in on-chip RAM with no CPU time: 800x480 at 45.6 frames a second and 480x480 at 76.3 at a 22.2 MHz pixel clock, each frame's pixel sum matched at the output register and two data bits counted at the pads. 28.6 MHz fails: the PRU's read widget delivers 48.9 MB/s. CPU copying in the frame buffer's RAM bank breaks 800x480. No panel was attached.
stop: none
-->
# Can the TI LP-AM263P's PRU generate a parallel RGB display bus from on-chip RAM, and how does that compare with SPI and the AM263x's GPMC bus?

**Short answer.** Yes for the bus, not shown on a panel. The PRU-ICSS beside the four Cortex-R5F cores produced the full pixel stream and timing from RAM, checked by a pixel sum for every frame at its output register and by edge counts on two data pins; no panel was attached, and the clock, sync and enable pins were not routed to pads:

| Frame | Pixel clock | Frames a second | Frame sums matched (register level) | R5F time spent |
|---|---|---|---|---|
| 800x480 RGB565 | 22.2 MHz | 45.6 | every frame after the first (the first frame after loading was wrong until a start-up drain was added) | none measurable (under 0.01 percent of an H.264 decode) |
| 480x480 RGB565 | 22.2 MHz | 76.3 | every frame after the first, as above | none measurable |
| 800x480 RGB565 | 28.6 MHz (TI's example clock), one run | 58.3 | wrong in every frame | not applicable |

The PRU reads on-chip RAM at 48.9 MB/s through its read widget (56.9 MB/s with blocking reads), which on that path needs 9 PRU cycles a pixel: 7 was run and failed, 8 is arithmetic (256 cycles a block against 262) and was not run. The frame buffer must sit in RAM banks the CPUs leave alone (480x480 fits one 512 kB bank; 800x480 spans two): an R5F copying at 209 MB/s in the same 512 kB bank corrupted every 800x480 frame. The same bus over the AM263P's SPI would manage 10.7 full 540x540 frames a second at the chip's 50 MHz limit (arithmetic). The GPMC parallel memory bus that could also drive a display exists on the AM263x; it is absent from the AM263P's SDK and data sheet (removal from the silicon is suspected, not confirmed), so it is compared from the AM263x data sheet only.

Terms: [vocab](../vocab.md). The **PRU-ICSS** is TI's programmable real-time unit subsystem: two 200 MHz cores with single-cycle control of their own pins. **RGB565** is 16 bits a pixel. A **parallel RGB** panel takes a pixel clock, a data-enable line, horizontal and vertical sync and the pixel bits, and has no frame memory of its own, so it must be refreshed continuously. The **xfr2vbus read widget** is the PRU's DMA helper that fetches 64 bytes into its registers. **OCSRAM** is the AM263P's on-chip RAM, six banks of 512 kB (3 MB on the data sheet; 2 MB start in this repo's layout, see the board's FAQ). **eGPO** is a PRU's direct output pins. **ICSS_M** and **ICSS_G** are the two PRU subsystem versions (AM261, AM263, AM263P; AM243x, AM64x). **EDMA** is the SoC's DMA engine. An **8080 bus** is the parallel write-strobe interface of panels that keep their own frame memory, such as the ST7796S controller on the 3.5 inch touch panel used elsewhere in this project. The **scratchpad** is a set of shared register banks two PRUs can pass data through in one cycle. **GPMC** is TI's general-purpose memory controller, a 16-bit external bus. 1 kB = 1,024 bytes. Code: `am263p/bench/pru-display/`. Logs: `am263p/data/logs/pru-display/`. Rows: `data/results/am263p.csv`, tests `display_scanout_fps`, `pru_read_bw` and `display_scanout_cpu_cost`.

**Question.** If the SPI link to a screen is the limit on the AM263P, can the PRU act as a display controller, how much screen does it drive, what does it cost the CPU, and how does that compare with the GPMC bus on the AM263x?

**Vendor position.**

- TI's open-pru repository ships `examples/LCD_interface` (read 2026-10-11, commit e6cf59b): "PRU direct GPOs to output data from frame buffer memory to parallel RGB24 interface ... 800x480 pixel with 24 bit color and 60 Hz frame rate. PRU resources used are limited to 200 MHz clock and functions available across ICSS_M (AM261/AM263) and ICSS_G (AM243/AM64) devices. The demo is built with AM243x LP." It uses both PRU cores and a PWM block of the AM243x's ICSS_G for VSYNC; there is no AM263x or AM263P build.
- The AM263Px data sheet (SPRSP81D) lists "20-channel enhanced output (eGPO) per PRU". SysConfig's AM263Px device data (version 1.28.1) gives PRU1 pads for all 20 outputs and PRU0 pads for 16 (outputs 0 to 6 and 8 to 16).
- The AM263x data sheet (SPRSP74E) lists a "General-Purpose Memory Controller (GPMC) – 16-bit parallel data bus with 22-bit address bus", with a functional clock of up to 133 MHz in asynchronous mode. The AM263x SDK's GPMC driver "Currently enabled for Memory Type: PSRAM". In the AM263Px SDK 26.01 the GPMC clock code is commented out under `//FIXME REMOVE GPMC FOR AM263PX`, and there is no GPMC base address, pin or driver.

**What was tried.** LP-AM263P Rev A, MCU+ SDK 26.01.00.08, TI Arm Clang 5.1.1, TI's PRU compiler 2.3.3 (macOS installer, installed unattended in about a minute, 126 MB), TI open-pru e6cf59b for the AM263Px PRU linker file, Intel Mac. Everything was loaded into RAM over the debug probe; no flash was written and no switch was moved.

1. **A cycle-exact scan-out firmware for one PRU.** `gen_scan.py` writes PRU assembly in which every pixel clock is a fixed number of PRU cycles: clock low, pixel on the bus, a running sum of the pixel, clock high, and free slots for control work. Bits 0 to 15 carry the pixel, 16 the clock, 17 data-enable, 18 and 19 the syncs, so one PRU drives the whole bus (on the AM263P only PRU1 has pads for all 20). Pixels come from OCSRAM through the read widget, 64 bytes (32 pixels) at a time. Blanking clocks run in hardware loops. A Python check confirms every pixel and loop body is exactly one pixel clock long.
2. **Self-checks with no scope.** The firmware sums every pixel value it puts on the bus and compares each frame's sum with the sum the R5F computed from the buffer; it counts 64-byte blocks that are not ready two pixels before use; it counts frames. The R5F times frames against its own cycle counter. Two pixel bits are patterned (bit 15 set on even lines, bit 14 on line 0), their pads (PR0_PRU0_GPIO15 and GPIO14, the Ethernet PHY 2 transmit lines, which are inputs on the PHY) are switched to the PRU, and the R5F counts their rising edges through the GPIO input register.
3. **What the read path delivers.** `bw.asm` brings 4,096 blocks of 64 bytes into the PRU by each path and times them with the PRU's cycle counter.
4. **What it costs the CPU.** The same H.264 decode (320x240 baseline, 30 frames, two passes) before the PRU starts and while it scans out. Then the R5F copies memory for 5 s, first in other RAM banks, then in the frame buffer's own bank.
5. **Two widget behaviors that had to be found first**, with a small probe program: the widget's status byte reads 0x0B and 0x08 while a block arrives and 0x05 when it is complete, whatever the configuration; and a read before the block is complete returns a partly filled copy of the same block without moving on.

**How far it got.** Verified at the output register and two pads: frames, pixel sums and pad edges match the prediction to the frame, three runs each at 22.2 MHz (28.6 MHz: one run).

| Read path into the PRU, 64-byte blocks | PRU cycles a block | MB/s at 200 MHz |
|---|---|---|
| Read widget from OCSRAM (wait for ready, then read) | 262 | 48.9 |
| Blocking burst read (`lbbo`) from OCSRAM | 225 | 56.9 |
| Two 32-byte reads from OCSRAM | 237 | 54.0 |
| Read from the PRU's own data RAM or the shared RAM | 20 | 640 |

| Scan-out | Frames a second (predicted) | Sum matched | Late blocks | Pad edges a second, even-line bit and line-0 bit (predicted) |
|---|---|---|---|---|
| 800x480, 9 cycles, 22.2 MHz | 45.612 (45.612) | 276 of 276 | 0 | 10,950 and 45.5 (10,947 and 45.6) |
| 480x480, 9 cycles, 22.2 MHz | 76.312 (76.313) | 459 of 459 | 0 | 18,311 and 76.5 (18,315 and 76.3) |
| 800x480, 7 cycles, 28.6 MHz | 58.348 | 0 of 351 | 48 percent | not used |

| Load beside the scan-out | 800x480 | 480x480 |
|---|---|---|
| H.264 decode on core 0 (decoder in banks 0 and 1) | decode cycles changed by under 0.01 percent; 0 bad frames | the same |
| R5F copying at 275 MB/s in banks 0 and 1 | 0 late, 0 bad | 0 late, 0 bad |
| R5F copying at 209 MB/s in the frame buffer's bank | every frame bad, 51 percent of blocks late | every frame right, 30 percent of blocks flagged at the early check |

**The line.** 28.6 MHz is the line for one PRU on the read widget: at 7 cycles a pixel a 32-pixel block is used every 224 PRU cycles and arrives every 262. **Chip limit** on the read-widget path, as measured: one read in flight, about 1.3 µs (262 cycles) from request to the last byte. Blocking reads take 225 cycles but stall the PRU, so this firmware did not use them. The other line is RAM-bank sharing: the frame buffer needs RAM banks of its own (one 512 kB bank for 480x480, two for 800x480), or the CPU's traffic in that bank has to stay light. What this test does not show: a picture. No panel was attached, the clock, sync and data-enable pins were not on pads (bits 16 to 19 on PRU0 have one pad, the clock's, and it was not switched over), and the timing was not looked at with a scope. The frame sums prove the right pixels went to the output register on time; the pad counts prove two of the bits reach the pins.

**No TI example for this chip: how it was done, what went wrong, and how hard it was.** TI ships a PRU display example for the AM243x only, so this is the first PRU display scan-out on an AM263P known to this repo. What existed: TI's AM243x example (both PRUs, an AM243x-only PWM block for vertical sync, a different pin map), TI's AM261 PRU FFT (the only ICSS-M code found that uses the read widget), the AM263Px PRU linker file in open-pru, and the SDK's PRU register headers. What was done instead of porting the AM243x code: a Python script that writes cycle-exact firmware for one PRU, so any panel timing is one command, plus self-checks (frame sums, late-block counts, pad edge counts) so the result could be proven without a scope or a panel.

| What went wrong | Cause | Cost | Fix |
|---|---|---|---|
| Every 64-byte block counted as late | **Tool or docs**: the widget's status byte is read through register r18 and reads 0x05 when ready, whatever the configuration; no document says either | one wrong run | a ten-line probe program recorded the status as a block arrived |
| The picture came out shifted by 32 pixels | **Our mistake**: a drain after configuring the widget swallowed block 0 | one wrong run | stop and drain at the end of each frame, configure at the start |
| The first frame after loading was wrong | **Setup or handling**: the widget kept state from the previous program | one bad frame | drain once at start-up |
| 28.6 MHz could not work | **Chip limit**: one read in flight, 262 PRU cycles a block from OCSRAM | the clock dropped to 22.2 MHz | two PRUs or EDMA into PRU RAM would lift it (not run) |
| PRU0 has pads for 16 outputs, not 20 | **Tool or docs**: the data sheet says 20 per PRU; the device data in SysConfig shows 16 for PRU0 and 20 for PRU1 | design moved to PRU1 for a product, PRU0 for this test | put a 20-signal bus on PRU1 |
| A load test reported 12 GB/s | **Our mistake**: the compiler removed copies nothing read | one run | volatile copies with a printed result |
| The AM263P has no GPMC | **Tool or docs**: only an SDK code comment says it was removed (whether it is gone from the silicon is not confirmed) | the GPMC comparison is from the AM263x data sheet | none on this part |

| Effort | |
|---|---|
| Elapsed, compiler download to the first checked frame | about 35 minutes, with no hands needed |
| Board runs | 8 scan-out runs, 1 probe, 1 bandwidth run; RAM loads only, nothing written to flash |
| Hands needed at the bench | none |
| Tools added | TI's PRU compiler 2.3.3 (one unattended install, 126 MB) |

Verdict: the PRU did exactly what the arithmetic said, to the frame, once its read path was understood. The documentation side took the effort: the read widget is the only fast way for the PRU to read on-chip RAM, and its ready signal, its partial reads and its speed on this part had to be found by experiment. A builder new to PRU assembly was not timed; this run, with a probe-first approach and a generated firmware, took one sitting.

**Against SPI and GPMC.**

| Display path on these chips | What it is | Full-frame rate | CPU cost | Measured here |
|---|---|---|---|---|
| SPI to a panel with frame memory (ST7796S class), AM263P | One data line at 50 MHz at most; the panel keeps the image | 540x540: 10.7 a second at 50 MHz, 3.2 at the 15 MHz the ST7796S panel used here is rated for | the CPU or DMA feeds every changed pixel | arithmetic from the data sheet clock; an LP-MSPM0G3519 measured 67.2 ms for 320x480 at 40 MHz |
| PRU parallel RGB, AM263P | 16 bits a pixel clock, continuous refresh from a frame buffer | 800x480 at 45.6, 480x480 at 76.3 | none measurable | yes, this page |
| PRU 8080 bus to a panel with frame memory, AM263P | The PRU strobes 16-bit writes; only changed areas are sent | set by the panel's write cycle | none | not built |
| GPMC 16-bit bus, AM263x only | A memory bus that can strobe an 8080-style panel, fed by DMA | set by the panel's write cycle; the bus allows a write every few cycles of a clock of up to 133 MHz | DMA, little CPU | not on the AM263P; AM263x not on this bench |

The PRU path is the fastest of the four reachable on the AM263P, and the only one that refreshes a panel without frame memory. Its price is RAM: 450 kB for 480x480 or 750 kB for 800x480 of the 2 MB that starts in this repo's layout, in banks of its own (one for 480x480, two for 800x480).

- A real panel on PRU1's pads. On the LaunchPad those pads go to Ethernet PHY 1 (receive lines are outputs on the PHY), so it needs a board that routes them; on a product board it is the plan.
- 28.6 MHz or more with both PRUs: one fetches with blocking reads into the scratchpad or shared RAM while the other drives the bus (TI's AM243x example splits the work across both cores).
- The SoC's EDMA copying lines into the PRU's shared RAM, so the PRU reads at 640 MB/s and the pixel clock is set by the panel.
- 540x540 (a common square 4 inch panel): the line is not a whole number of 32-pixel blocks, so it needs a 544-pixel stride; 540x540 at 60 Hz needs about 20.8 MHz, under the 22.2 measured.
- An 8080 bus from the PRU to the ST7796S panel used here, with the touch screen.

**To repeat this.** You need TI Arm Clang 5.1.1, the MCU+ SDK, the PRU compiler (`ti-cgt-pru` 2.3.3), pyserial and your probe's serial. Build the PRU scan-out program for the panel timing, link it into an R5F image with the SDK makefile, load it over the probe and read the serial output:

- 800x480 at 9 PRU cycles per pixel: the default timing.
- 480x480 at 9 cycles: horizontal total 560, vertical total 520, horizontal sync 20, horizontal back porch 40.

Expect `FRL_PRU idle ... fps_x1000=45612`, a sum equal to the expected sum, `badsum=0` and `late=0`.

Common problems and fixes are also in the [LP-AM263P FAQ](../../am263p/FAQ.md), section "PRU display".

**Ask.** Has anyone run TI's open-pru LCD example on an AM261x or AM263x, and what read bandwidth did the widget give there? Is the AM263P's GPMC gone from the silicon or only from the SDK?
