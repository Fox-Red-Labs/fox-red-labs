<!-- experiment
id: E62
board: Raspberry Pi Pico 2 H
date: 2026-10-11
status: works
reached: verified
line: CoreMark is the same from RAM and flash within 0.3 percent; the 30k occupancy model runs 4.3 percent (Cortex-M33) and 6.0 percent (Hazard3) faster from RAM.
stop: none
-->
# Does code in external flash cost anything on the Pico 2 with a 16 kB cache?

**Short answer.** Flash costs single-digit percent at most, and only on the largest model. CoreMark differs by less than 0.3 percent between flash and RAM. In five of six cases the smaller room-occupancy models run 0.6 to 2.5 percent slower from RAM (cause unknown; the 8k model is 2.5 percent slower on the Cortex-M33); the Hazard3 2k model is 0.3 percent faster. The 32.8k-weight model runs 4.3 percent faster from RAM on the Cortex-M33 and 6.0 percent faster on Hazard3. **Tag for the 30k gain: Unknown (suspected: cache misses).** **Tag for the small-model slowdown: Unknown.**

Terms: [vocab](../vocab.md). 1 kB = 1,024 bytes. The cache is the 16 kB execute-in-place (XIP) cache that holds recently read flash. `copy_to_ram` is the pico-sdk build type that copies the whole program from flash into RAM at boot. The "32.8k-weight model" is the 30k-parameter room-occupancy model (assumed about 30 kB of weights at one byte per parameter, larger than the cache; the size was not measured). Numbers: `data/results/pico2.csv`.

**Question.** Does code in external flash cost anything on the Pico 2 with a 16 kB cache?

**Vendor position.** Raspberry Pi Pico 2 datasheet, release 5, section 1 (read 2026-10-07): 4 MB external QSPI flash, read through a 16 kB execute-in-place cache.

**What was tried.** The same images built to run from flash and with `copy_to_ram` (code and weights copied at boot), three runs each, GCC 14.3.1 for the Cortex-M33 and 16.1.0 for Hazard3.

**How far it got.** Verified. CoreMark: 434.09 per second in both builds on the Cortex-M33, and 495.23 against 493.78 on Hazard3. Occupancy models, cycles in flash and RAM: 30k, 4.057M against 3.881M (Cortex-M33) and 4.350M against 4.088M (Hazard3); 8k, 6.823 ms against 6.993 ms (Cortex-M33).

**The line.** No limit reached. The only gain from RAM is on the 32.8k-weight model, whose weights do not fit the cache; that this is the cause was not tested. The small models getting slower in RAM is not explained.

Raw logs are not saved in the repo; the rows are in `data/results/pico2.csv`.

**Ask.** Has anyone read the RP2350 cache hit counters on a working set of about 30 kB?
