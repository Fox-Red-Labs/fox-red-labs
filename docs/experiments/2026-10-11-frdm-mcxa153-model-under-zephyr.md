<!-- experiment
id: E56
board: FRDM-MCXA153
date: 2026-10-11
status: works
reached: verified
line: The largest room-occupancy model (32.8k weights) fits and passes 20 of 20 under Zephyr 4.4.2 on the FRDM-MCXA153 in 8.7 kB of the 24 kB the app region gets; the other 8 kB (SRAMX) is not needed.
stop: none
-->
# Do the room-occupancy models fit under Zephyr's 24 kB of RAM on the NXP FRDM-MCXA153?

**Short answer.** Yes, even the largest. The 32.8k-weight model built into a Zephyr 4.4.2 app takes 64.3 kB of flash and 8.7 kB of RAM (35 percent of the 24 kB region) and classified all 20 test windows as the reference model does. The plan guessed it would be tight; it was not, because the 32.8k-weight model's working buffers are small.

Terms: [vocab](../vocab.md). 1 kB = 1,024 bytes. The chip has 32 kB of RAM; Zephyr's board file gives the app 24 kB at 0x20000000 and leaves the other 8 kB (SRAMX at 0x04000000) unused.

**Question.** Does the MCXA153's smaller Zephyr RAM region stop the largest of the four models?

**Vendor position.** Zephyr's board `frdm_mcxa153` defines `sram0` as 24 kB (`boards/nxp/frdm_mcxa153`, read 2026-10-07).

**What was tried.** A minimal Zephyr app (CMake file and `main.c` in the scratch folder, not kept) that links `occ_cnn_30k.c` unchanged and runs the model on the 20 vectors in `test_vectors.h`, printing how many match the reference class. Zephyr 4.4.2, GCC 12.2 cross-compile, `west build -b frdm_mcxa153`, flashed with pyOCD. The other three models are smaller and were not rebuilt.

**How far it got.** Verified. Flash 64.3 kB of 128 kB (49 percent), RAM 8.7 kB of 24 kB (35 percent), and the serial line `occ30k zephyr: 20 of 20 correct`. The build log is kept outside the repo; the numbers are in this page.

**The line.** No limit reached. Bare metal the same model needs 16 kB of RAM for all four models together.

**Ask.** None.
