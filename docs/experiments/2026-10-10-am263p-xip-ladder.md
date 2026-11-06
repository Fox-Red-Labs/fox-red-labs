<!-- experiment
id: E141
board: LP-AM263P
date: 2026-10-10
status: works
reached: verified
line: Running code straight from the LP-AM263P's flash works with only the USB cable. TI's own memcpy benchmark takes 8.7 times as long as from RAM with no extra cache and 2.4 times with TI's 64 kB cache; with smart placement it takes 9% less time than all-RAM. With the 64 kB cache, ten of TI's AI examples run within 2% of their RAM speed with their outputs checked (an eleventh is timed but fails TI's own check), and CoreMark matches RAM; one model, whose 167 kB of code and constants stream through the cache every inference, stays 6.9 times slower. The flash ran at 16.7 MHz because its high-speed read tuning failed, so TI's numbers with tuning on are lower.
stop: none
-->
# How much does it cost to run code from flash on the LP-AM263P, and do TI's AI models care?

**Short answer.** On the LP-AM263P (AM263P4 chip), code runs straight from the external flash with only the USB cable, and with TI's 64 kB flash cache most programs lose little or no speed. TI's AI examples and CoreMark run at RAM speed from flash with the cache, except one model too big for it.

| Code runs from | TI's memcpy benchmark | CoreMark | TI's AI examples |
|---|---|---|---|
| On-chip RAM | 14.6 ms | 1,464.5 per second | baseline |
| Flash, no extra cache | 8.7 times as long | 1,394.5 (4.8% lower) | same to 7.8 times as long |
| Flash + TI's 64 kB L2 cache | 2.4 times as long | 1,464.5 (equal) | ten within 2% of RAM, an eleventh within 2% with its output unconfirmed; one 6.9 times as long |
| Flash + TI's smart placement | 9% less time than RAM | not run | not run |

Three things to know before you rely on these numbers:

- **This repo's flash ran slow:** at 16.7 MHz, because the driver's high-speed read tuning (PHY tuning) failed on this repo's board. TI's pages, with tuning on, show plain flash at 3.7 times RAM where 8.7 was seen; the slower clock may not be the whole gap. Why tuning fails is unknown.
- **Opening the flash can write to it:** when that tuning fails, TI's flash driver erases and rewrites one 4 kB sector at offset `0x80000`. Back up the flash first (below).
- **One of TI's twelve AI examples fails its own check** on this board, with or without this repo's changes, and still prints "All tests have passed!!". Its timing is in the table; its output is not confirmed.

The first inference after a load is slower than the later (warm) ones, by 0.1 to 1.2 ms with no extra cache, while the caches fill.

Terms: [vocab](../vocab.md) (XIP, cache, OptiFlash, smart placement, PHY tuning, controlCARD, DSLite, development boot, golden values). 1 kB = 1,024 bytes. TI's benchmark is `examples/benchmarks/optiflash_benchmark` in the MCU+ SDK.

Problems and fixes: [LP-AM263P FAQ](../../am263p/FAQ.md). Rows: `data/results/am263p.csv`, tests `xip_memcpy_tasks`, `ai_vendor_example` (variants ending `-xip` and `-xip-l2-64k`) and `coremark`.

**Question.** If a program is bigger than the AM263P's 2 MB of usable RAM, or RAM must stay free for data, how much speed do I lose by running code from the flash, and do TI's caches get it back?

**Vendor position.** TI's MCU+ SDK 26.01 (read 2026-10-10):

- XIP page: "the flash memory is typically slower (~300MB/s bandwidth) vs RAM memory ( > 1600 MB/s)... execution speed, esp when code is not cached in the CPU cache, will be slower". It also says "The secondary bootloader needs to enable flash at highest throughput mode in this DAC mode, before booting the application."
- OptiFlash benchmark pages, with the sample output from TI's controlCARD (not the LaunchPad): 0 B memcpy test 12.45 ms from RAM, 46.57 ms plain XIP, 42.43 / 39.19 / 31.88 / 20.45 ms with an 8 / 16 / 32 / 64 kB L2 cache, 11.01 ms with smart placement.
- The OptiFlash examples ship for the controlCARD only; the flashing steps assume UART boot mode and the board's boot switches.

**What was tried.** LP-AM263P Rev A, MCU+ SDK 26.01.00.08, TI Arm Clang 5.1.1, SysConfig 1.28, Intel Mac. Programs were loaded into RAM with DSLite, the command-line loader that comes with CCStudio, while the chip waited (development boot). No switch was moved. TI's numbers come from its controlCARD, a plug-in card version of the board.

1. A RAM flash writer (TI's LaunchPad flash settings: octal DDR, 133 MHz input clock) erases and writes the code, reads it back through the driver and through the memory-mapped window, and refuses any address below 1 MB except the one block it writes back when restoring `0x80000`. Everything was written at 1 MB or 3 MB, space that read blank in this repo's backup of the board's flash.
2. `am263p/tools/xip_split.py` splits each build into the flash part and a RAM part that cannot write to flash.
3. TI's seven OptiFlash benchmarks, copied with the package changed to the one for the LaunchPad development kit.
4. TI's twelve AI examples that have test vectors and need no sensor, and TI's CoreMark example, in three layouts: TI's (all in RAM), code and constants in flash, and the same with the 64 kB L2 cache. A timer was added: one cold inference, then 101 timed inferences, median reported. Each layout was loaded three times.

**How far it got.** Every step ran, including putting the flash back: both test ranges erased and the `0x80000` block rewritten from this repo's backup, after which the lower 16 MB matched the backup byte for byte (2026-10-11). All 32 flash writes read back with zero mismatched bytes, through the driver and through the memory-mapped window (the writer prints `XIPFLASH OK ... mismatched_driver 0 mismatched_mmap 0`). Every run passed its own check: TI's "All tests have passed", CoreMark's checksums, and the AI examples' golden outputs, except one example that fails on its own (below).

**TI's OptiFlash benchmark** (16 tasks, 0 B copy, median of 3 loads; this repo's flash at 16.7 MHz, TI's with tuning on):

| Code runs from | LP-AM263P, ms | Against RAM | TI's page, ms | TI against RAM |
|---|---|---|---|---|
| On-chip RAM | 14.60 | 1.00x | 12.45 | 1.00x |
| Flash, no L2 cache | 126.55 | 8.67x | 46.57 | 3.74x |
| Flash, 8 kB L2 | 110.40 | 7.56x | 42.43 | 3.41x |
| Flash, 16 kB L2 | 91.82 | 6.29x | 39.19 | 3.15x |
| Flash, 32 kB L2 | 65.46 | 4.49x | 31.88 | 2.56x |
| Flash, 64 kB L2 | 34.57 | 2.37x | 20.45 | 1.64x |
| Flash, smart placement | 13.26 | 0.91x | 11.01 | 0.88x |

This repo's RAM time is 17% above TI's. Both are the AM263P at 400 MHz; TI's figure is from the controlCARD booted from flash through TI's bootloader, this repo's from the LaunchPad development kit loaded over the debugger. The cause is unknown. Smart placement beats all-RAM on both boards; the reason was not examined.

**TI's AI examples** (one inference, median of 101 after a cold first call, median of 3 loads, R5F at 400 MHz):

| Model | RAM, ms | Flash, no L2, ms | Flash, 64 kB L2, ms | First call from flash with no L2, ms |
|---|---|---|---|---|
| arc_fault | 2.928 | 3.114 | 2.929 | 3.679 |
| blower_imbalance | 1.717 | 1.786 | 1.717 | 2.369 |
| fan_blade_anomalydetection (output not confirmed, see below) | 2.955 | 4.957 | 3.012 | 5.296 |
| forecasting_pmsm_rotor_temp | 0.062 | 0.072 | 0.062 | 1.122 |
| generic_timeseries_anomalydetection | 4.719 | 6.521 | 4.738 | 6.677 |
| generic_timeseries_classification | 3.386 | 3.492 | 3.388 | 4.009 |
| generic_timeseries_forecasting | 0.915 | 7.165 | 6.293 | 7.262 |
| generic_timeseries_regression | 0.704 | 1.497 | 0.712 | 2.097 |
| hvac_indoor_temp_forecast | 0.117 | 0.147 | 0.117 | 1.249 |
| motor_fault | 0.960 | 0.960 | 0.960 | 1.528 |
| torque_measurement | 1.197 | 1.637 | 1.200 | 2.836 |
| washing_machine_load_weighing | 15.244 | 16.340 | 15.250 | 16.813 |

**CoreMark** (TI's example, -O3, 3 loads each): 1,464.5 per second from RAM, 1,394.5 from flash with no L2 (4.8% slower), 1,464.5 with the 64 kB L2.

**The line.** Nothing stopped. The cost of flash depends on how much code and constant data one pass touches.

- With no extra cache, the cost runs from nothing to 7.8 times (generic_timeseries_forecasting; the rest are under 2.2 times). motor_fault is 0.960 ms all three ways (suspected: its inner loop fits the core's own L1 cache). The two smallest models are not free: forecasting_pmsm_rotor_temp takes 16% longer and hvac_indoor_temp_forecast 26% longer. Each model's code size was not measured, so where the line falls is not known.
- With the 64 kB L2 cache, nine models land within 1% of RAM, two within 2% (one of them the model whose output is unconfirmed), and CoreMark matches RAM.
- A model larger than the cache streams from flash every time. generic_timeseries_forecasting links 100 kB of code and 66.8 kB of model constants into flash (from its map file) and stays 6.9 times slower with the cache. Keep a model like that in RAM, or let smart placement choose.

The first call after a load, with no extra cache, costs 0.1 to 1.2 ms more than a warm call on every model. It adds up when a model runs once after waking from sleep.

Three cautions from the run:

- **The flash ran slow.** On every open, TI's driver printed `Flash_norOspiPhyTune : PHY enabling failed!!! Continuing without PHY...`, so the flash clock was 133.33 MHz / 8 = 16.7 MHz. TI's flash debug guide (SPRADS5, section 5.3.3) says this message means the tuning pattern is in flash but does not read back correctly. The pattern is there: a JTAG read of 0x80000 shows its 128 bytes. Cause unknown: this repo's loading method has not been separated from the board. If you see that message, your XIP numbers are slow numbers.
- **Opening the flash can write to it.** When the tuning pattern does not read back, TI's flash driver (`Flash_norOspiPhyTune`, MCU+ SDK 26.01) erases the 4 kB sector at the tuning offset and writes the pattern there. On the LaunchPad that offset is `0x80000`, below the 1 MB guard of this repo's writer. Back up the flash first with `am263p/tools/flash_backup.sh`; The [LP-AM263P FAQ](../../am263p/FAQ.md) has the restore steps, and the block was written back, with the lower 16 MB then matching the backup exactly. On a board that has not run this repo's flasher, the script reads only the lower 16 MB of the 32 MB part (the flash wraps at 16 MB in its power-on mode); after a flasher run it reads all 32 MB, and this repo's upper half read blank.
- **fan_blade_anomalydetection fails its own check on this board,** with or without this repo's changes: TI's unmodified example matches 0 of 192 golden values, then prints "All tests have passed!!". Read the "Golden vectors matched" line, not the last line. Its timing above is real, but its output is not confirmed. The AM13E230 version of the same example checks the count and passes there.

- Get the high-speed read tuning to pass (TI's PHY grapher has no LaunchPad build), or boot from flash through TI's bootloader, then rerun the table. TI's own ratio suggests plain XIP would land near 3.7x.
- Smart placement for the AI models: TI's flow profiles a run, then a script picks the hot functions. Done here only for TI's memcpy benchmark, whose annotation file ships with the SDK.
- Keep the model constants in RAM and only the code in flash. That would probably fix the one model the cache cannot save; not run.
- Two or four cores running from flash at once. TI's XIP page says they split the flash bandwidth.

**Ask.** If you have run XIP on an LP-AM263P with PHY tuning passing, post your numbers for the same benchmark.
