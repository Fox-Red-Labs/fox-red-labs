<!-- experiment
id: E42
board: ESP32-C6-DevKitC-1
date: 2026-10-11
status: works
reached: verified
line: The 8k room-occupancy network runs on the ESP32-C6's low-power core and passes 20 of 20 test windows with every logit equal to the reference, in 214 ms (3.36 million LP cycles); the float input step does not fit, so the main core quantizes the input.
stop: ram
-->
# Can the ESP32-C6's low-power core run the 8.5k-weight room-occupancy model?

**Short answer.** Yes, the network part. The low-power (LP) core classified all 20 test windows correctly with every logit equal to onnxruntime's, in 213.8 ms each, with the main core only handing over a 128-byte input and reading the answer back through shared memory. The whole model, including the float input step, does not link: it needs about 0.6 kB more than the 16 kB of LP SRAM before the stack. The stack-free fit of the network alone leaves 2.3 kB free.

Terms: [vocab](../vocab.md). 1 kB = 1,024 bytes. The LP core is a second RISC-V core (RV32IMAC, no float unit) that runs while the main core sleeps; its code, data and stack all live in the 16 kB LP SRAM.

**Question.** How small a core runs the 8k model, and does the C6's LP core, which can run on its own while the main core sleeps, hold it?

**Vendor position.** ESP-IDF v6.1 ships an LP core build system (`examples/system/ulp/lp_core`) and says the LP core runs "up to 20 MHz" with 16 kB of LP SRAM shared between its code, data and the variables it shares with the main core (read 2026-10-11). The expectation written before the run was about 130 ms, the L1306's 84 ms times 1.6 for the clock.

**What was tried.** ESP-IDF v6.1, GCC 15.2.0, the example `build_system` as base, the model file `occ_cnn_8k.c` unchanged (the same file every other board runs), `-Os`, newlib, the reserve for the LP program set to 16.0 kB (`CONFIG_ULP_COPROC_RESERVE_MEM`).

1. Whole model on the LP core (input window as 128 floats, quantize and run): link error. At the default 15.5 kB reserve with `-O3` the LP region overflows by 3.7 kB; at 16.0 kB and `-Os`, the data ends 0x24c (588) bytes past where the stack must start, before any stack. The sections: 9.4 kB of weights (`.rodata`), 4.1 kB of code (the float step pulls in the C compiler's software-float routines), 2.7 kB of buffers (two 1 kB working buffers inside the model, a 512 B float input, a 128 B int8 copy).
2. Split the model the way its two functions already split: the main core runs `occ_cnn_8k_quantize_input` (float to int8, 128 values) and writes 128 bytes into LP SRAM; the LP core runs `occ_cnn_8k_run_q` (the int8 network) and writes four logits and a class back. Code drops to 1.9 kB, buffers to 2.2 kB; the data ends 2.3 kB before the stack top.
3. Flash the image; the main core posts each of the 20 windows, waits for the LP core, compares class and logits, and times the round trip with the main core's timer. The LP core also reads its own cycle counter around the network call.

**How far it got.** Verified, twice from a reset. 20 of 20 classes and logits right. Round trip 213.80 to 213.86 ms in the first run and 213.83 to 213.91 ms in the second; the LP core's own counter reads 3.36 million cycles. Cycles over time give an effective clock of 15.7 MHz, not the 20 MHz the documentation gives as the top; the cause (the LP core's clock source is not set in this project) was not isolated.

**The line.** RAM. The whole model is over the LP SRAM by 588 B plus its stack. The network alone fits with 2.3 kB left for the stack. For comparison, the main core at 160 MHz runs the whole model in 7.1 ms at `-O3` (1.14 million cycles) and 15.1 ms at `-Os` (2.42 million cycles); the LP core takes 3.36 million cycles for the network alone, about 1.4 times the main core's `-Os` cycles for the whole model, and 30 times as long in wall time. The measured 214 ms is 1.6 times the 130 ms expected.

**Ask.** What clock does the C6's LP core run at by default in ESP-IDF v6.1? Measured: 15.7 MHz against the 20 MHz in the documentation.
