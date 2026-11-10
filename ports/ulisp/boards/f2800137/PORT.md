# uLisp on the LAUNCHXL-F2800137: runs

**Short answer.** uLisp 4.9a runs on the C28x at 120 MHz from flash and gives a Lisp prompt on the probe's serial port at 115200: 19 of 19 REPL checks pass in each of 3 runs. `(tak 18 12 6)` takes 8.1 to 8.2 s, `(fib 23)` 5.3 to 5.4 s and `(q2 7 8)` 20.4 s. The sketch assumes a 32-bit `int` and 8-bit `char`, so it needs three kinds of change, all made by `build.sh` and `compat.h`. The image is 141,866 B of flash and 28,698 B of RAM with a 2,000-object workspace and a 6,144-word stack. Measured 2026-11-09.

| Item | Value |
|---|---|
| Compiler | cl2000 25.11.1.LTS, `-v28 -ml -mt --float_support=fpu32 --abi=eabi -O2`, C++, C2000Ware 26.02 |
| Flash | 70,933 words (141,866 B, 54 percent of 256 kB): `.text` and `.const` in main flash |
| RAM | 14,349 words (28,698 B): 2,000 objects at 4 words each (8,000 words), data, and a 6,144-word stack in RAMLS0 and RAMLS1 joined |
| Board layer | `board.c`: copies the `.TI.ramfunc` code to RAM, then `Device_init`; SCI-A on GPIO28 and GPIO29 at 115200 (the probe's serial port); CPU Timer 0 at 1 kHz; LED1 on GPIO20 and LED2 on GPIO22 as pins 13 and 14 |
| Build | `build.sh` (build only), then `mkraw.sh <out folder>` makes the 256 kB flash image `ulisp-raw.bin` |
| Write | `c2000-launchpad/tools/c2kflashback.js ulisp-raw.bin <readback>.bin` (about 19 s, `verifyBinaryProgram: ok`); restore with the same script and `c2000-launchpad/data/flash-as-received.bin` |

**Timing against the CC2340R5.** The CC2340R5 (Cortex-M0+, 48 MHz) takes 10.8 s for `tak` and 7.5 s for `fib 23`. Scaled by clock alone the C28x would take 4.3 s; it takes 8.1 s, so it spends 1.9 to 2.5 times the clocks per Lisp step, and finishes 1.3 times sooner in wall-clock time. The reasons are in the data: a C28x word is 16 bits, so every 32-bit object field, pointer and integer is two-word work, `long` arithmetic is several instructions, and flash costs 2 wait states at 120 MHz. The sketch is also built `long`-wide on every integer, which costs speed.

**Changes the sketch needs** (`build.sh` applies them to the build copy; the first image skipped them and passed 16 of 19 checks):
1. `char` is 16 bits. `typedef unsigned char uint8_t` in `compat.h`, `strcasecmp` in `board.c`, and a `(uint32_t)` cast on the `ch<<16`, `ch<<8` and `ch<<24` in the string builder.
2. `int` is 16 bits, so integers wrapped at 16 bits, numbers printed as garbage (`(/ 1.0 3)` printed `0.0-+/*//`), strings came back empty and the radix-40 symbol packer overflowed (`(tak 12 8 4)` returned 4, not 5, because `1-` collided with another name). Every `int` and `unsigned int` in the sketch becomes `long`, `uintptr_t` becomes `uint32_t`, and `INT_MAX` and `INT_MIN` in `compat.h` are the 32-bit limits.
3. `sizeof` counts 16-bit words, so the sketch's `sizeof(int)` (the 4 bytes of a packed word) becomes the constant 4.
4. The stack grows up: the stack check measures from `__TI_STACK_END` (untested).

**Link and boot.** The codestart branch must sit at 0x80000 (`BEGIN` in `f2800137_ulisp.cmd`), and `board.c` must copy `.TI.ramfunc` to RAM before `Device_init` (it calls `Flash_initModule` from RAM).

**Writing the image.** CCStudio's `loadProgram` of the linked `.out` timed out twice and DSLite answered -260, so the image goes in as a raw binary the way the restore does it: `hex2000 ulisp.out --ti_txt --memwidth=16 --romwidth=16`, then `mkraw.sh` fills a 256 kB image with 0xFFFF from 0x80000 (byte address 2 times the word address). Written five times (one per source revision), each `verifyBinaryProgram: ok`; logs in `c2000-launchpad/bench/ulisp/raw-write.txt`. The 256 kB image includes the flash base, so the write erases main flash first and was done only with a backup (two reads, identical) and a restore read back identical to the as-received image.

**Not done.** The stack guard with an endless recursion, an LED pin read with an instrument, `analogread` and `save-image`. Rows: `data/results/f2800137.csv`; logs: `c2000-launchpad/bench/ulisp/`.
