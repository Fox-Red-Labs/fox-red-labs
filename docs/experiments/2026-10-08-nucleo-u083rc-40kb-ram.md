<!-- experiment
id: E128
board: NUCLEO-U083RC
date: 2026-10-08
status: works
reached: verified
line: All 40 kB of the STM32U083's RAM works as one block from 0x20000000 to 0x2000A000; ST's linker scripts (206 of 209 for this board) and Zephyr's default give programs only 32 kB. A program linked for 40 kB filled and read back 38 kB with no error.
stop: none
-->
# Can a program on the ST NUCLEO-U083RC (STM32U083) use all 40 kB of RAM, or only the 32 kB ST's examples give it?

**Short answer.** All 40 kB. The RAM is one block from address 0x20000000 to 0x2000A000. ST's own example linker scripts give programs 32 kB, and so does Zephyr by default, but a program linked with the RAM length set to 40K filled and read back a 38 kB buffer with no error.

Terms: [vocab](../vocab.md). Board page: [NUCLEO-U083RC README](../../nucleo-u083rc/README.md). 1 kB = 1,024 bytes. A linker script tells the compiler where memory is and how big it is.

**Question.** The data sheet says 40 kB of RAM. Do the tools let a program use it?

**Vendor position.** ST data sheet DS14463 Rev 2: "40-Kbyte SRAM with parity" for the STM32U083 (read 2026-10-07). STM32CubeU0 v1.3.0: 206 of the 209 linker scripts under `Projects/NUCLEO-U083RC` say `RAM (xrw) : ORIGIN = 0x20000000, LENGTH = 32K`, with the header "32KBytes RAM"; the three secure-boot projects (`OEMiROT_Loader`, `OEMiROT_Appli`, `Templates_ROT`) say 40K. Zephyr 4.4.2's `stm32u083Xc.dtsi` lists two regions, `sram1` at 0x20000000 (32 kB, the one programs use) and `sram2` at 0x20008000 (8 kB). STM32duino 3.0.0 lists 40 kB for the board.

**What was tried.** NUCLEO-U083RC (STM32U083 Rev A), GCC 12.2. First, over the debugger with the chip halted: fill 0x20000000 to 0x2000A000 with 0x5A5A5A5A, read it all back, then the same with 0xA5A5A5A5, then read 0x2000A000. Second, a program: ST's `GPIO_IOToggle` startup and HAL files, ST's linker script with `LENGTH = 32K` changed to `40K`, and a 38 kB static buffer filled with an address-based pattern and read back twice by the CPU.

**How far it got.** Verified. Both debugger patterns read back with 0 bad bytes of 40 kB; the read at 0x2000A000 failed, as it should. The program linked with 39.6 kB of RAM in use, ran, and reported 9.7k words from 0x20000040 to 0x20009840 with 0 mismatches.

**The line.** No limit reached: the last 8 kB is ordinary RAM directly after the first 32 kB. Nothing on the chip stops a program using it; the 32K in ST's linker scripts and Zephyr's choice of region are the limit. Not tested: whether that last 8 kB behaves differently in low-power modes (retention in Standby), which may be why ST keeps it separate.

**Ask.** Does anyone know why ST's U0 example linker scripts stop at 32 kB?
