# What goes wrong when you build liblc3 for the LP-MSPM33C321A LaunchPad, and how do you avoid it?

Built from liblc3 v1.1.3 with TI Arm Clang 5.1.1.LTS and the MSPM33 SDK 1.04.00.00 on the LP-MSPM33C321A, with arm-none-eabi-gcc 14.3.1 for the Cortex-M0+ model. Each answer starts with the fix. Terms: [docs/vocab.md](../../docs/vocab.md). 1 kB = 1,024 bytes. Numbers and method: [README.md](README.md).

**What caused it**, one tag per answer: Our mistake, Setup or handling, TI example default, Tool or docs, Chip limit, Third-party behavior, Unknown.

## Speed

### How do you turn on liblc3's Arm DSP paths with TI Arm Clang? Encode is 11 to 16 percent faster with them.

Compile the library files with `-U__GNUC__ -D__GNUC__=10`. `src/ltpf_arm.h` and the saturating store in `src/common.h` are guarded by `__GNUC__ >= 10`, and Clang reports `__GNUC__` as 4, so the Arm DSP paths stay off. With the override the encoder takes 166.6k, 215.3k and 343.3k cycles per 10 ms frame instead of 197.9k, 249.3k and 387.7k, decode is unchanged, and the output is bit-exact against the Mac. The override is a compile flag; the library files are not edited.

### Does `-ffast-math` make liblc3 faster on the Cortex-M33?

No. Upstream builds with `-ffast-math`. On this core encode took 197.4k, 249.4k and 388.0k cycles with it and 197.9k, 249.3k and 387.7k without. Use the strict build when you need to compare bytes with another machine.

### Which optimization level is fastest?

`-O2` for encode, `-O3` for decode at 16 and 24 kHz. `-O3` and `-Os` both cost 12 to 24 percent more encode cycles than `-O2`; `-O3` saves 10 percent of decode cycles at 16 and 24 kHz. `-Os` is the smallest image (117.8 kB against 157.8 kB).

## Bit-exactness

### The board's encoded bytes do not match my Mac's. What is different?

Build both sides the same way: `-ffp-contract=off` and no `-ffast-math`. With those two choices the board matches the Mac in every frame of all three LE Audio configurations. With `-ffast-math` on both sides the encoded bytes also match each other, but they are not the strict build's bytes. Both are valid LC3; they are not interchangeable bit for bit.

### The decoded samples differ by one in a few places between two builds that decode the same bytes. Is something broken?

No. The decoder is float code. A different float library (the soft-float one) or fast-math reorders the arithmetic and moves a few samples by one least-significant bit: 23 of 48k samples with the FPU off, 21 of 48k with fast-math, largest difference 1. Compare CRCs between strict builds, and use a sample-by-sample difference when the arithmetic is allowed to differ.

## Build

### Linking a soft-float image fails with `Tag_ABI_VFP_args attribute value of "1" that is different than one previously seen ("0"); combining incompatible files`. How do you link it?

Compile the SDK sources the clock setup needs instead of linking the SDK archive. `driverlib.a` for the MSPM33C321A is built hard-float, and the linker will not mix it with objects built `-mfloat-abi=soft`. `dl_common.c`, `dl_timer.c`, `dl_gpio.c` and `m33/sysctl/dl_sysctl_mspm33c321x.c` from `source/ti/driverlib/` build with the soft-float flags and are enough for the SysConfig clock setup. `ports/lc3/target-m33/build.sh` does this for the `soft` option.

### How do you make liblc3 smaller for an LE Audio product?

Build with `-DLC3_PLUS=0 -DLC3_PLUS_HR=0`. That leaves out the LC3plus code and the high-resolution tables: 157.8 kB becomes 123.4 kB at `-O2` and 83.2 kB at `-Os`, the peak stack falls from 5.5 kB to 3.6 kB, and the three configurations still decode to the same CRCs as the full build. `-DLC3_PLUS=0` alone gives 139.9 kB. The LE Audio frame lengths of 7.5 and 10 ms stay; 2.5 and 5 ms need LC3plus.

### How much stack does liblc3 need?

Reserve 8 kB for the 48 kHz configuration. The stack was painted before a run: 5.5 kB was used at `-O2` with LC3plus, 3.6 kB without it. The codec state (2.5 to 8.9 kB per channel and direction) lives in memory you pass in, not on the stack.

## Reading results

### `DSLite memory -r <addr>,<n> -s 8` returns more bytes than asked for, and a large read fails with "Memory map prevented reading". Why?

Ask for a quarter of the bytes. With `-s 8` the output file was four times the requested count (16 gave 64 bytes, 4096 gave 16,384), so a 96k request ran past the end of the 256 kB RAM and failed. `ports/lc3/run_board.py` asks for 24,000 and keeps the first 96k bytes. The first bytes of each read were the right ones in every comparison made here. Seen with DSLite from UniFlash 9.6.0; the cause is not known.

## Frame lengths, loss and channels

### `lc3_frame_bytes` returns -1 and `lc3_setup_encoder` returns NULL for a 2.5 or 5 ms frame. What is wrong?

Build with LC3plus on, which is the default: leave out `-DLC3_PLUS=0`. With `-DLC3_PLUS=0` the library accepts only 7.5 and 10 ms, and says so by returning -1 for `lc3_frame_bytes(2500, ...)` and `lc3_frame_bytes(5000, ...)` and NULL from the setup call. The default build runs all four frame lengths.

### I asked for 32 kbps at 2.5 ms and the frame is 20 bytes, so the stream is 64 kbps. Why?

20 bytes is the smallest frame the library makes. `LC3_MIN_FRAME_BYTES` is 20, and `lc3_frame_bytes` raises a smaller request to it: 2.5 ms at 20 bytes is 64 kbps, 5 ms at 20 bytes is 32 kbps. Ask for 64 kbps at 2.5 ms at 16 and 24 kHz, and read the rate from `nbytes * 8 / frame length`.

### How do you decode a lost packet?

Call `lc3_decode(dec, NULL, 0, ...)` for the missing frame. The decoder returns 1 and writes concealed samples. On the Cortex-M33 a concealed 10 ms frame cost 31.6k, 50.4k and 97.9k cycles at 16, 24 and 48 kHz, which is 43, 44 and 58 percent of a good frame. Call it for every missing frame, in order, so the decoder state keeps its place.

### How do you encode two channels?

Use one encoder and one decoder per channel, and pass the interleaved buffer with a stride of 2: `lc3_encode(enc[c], LC3_PCM_FORMAT_S16, pcm + c, 2, nbytes, out)`. LC3 has no joint stereo, so the cost is two times mono (2.00 against mono for encode and 2.00 to 2.01 for decode on this core) and the state is two copies (2.5 to 5.3 kB per encoder, 3.1 to 8.9 kB per decoder at 10 ms).

## Mistakes to avoid

- Do not compare a fast-math board build with a strict Mac build and read the CRC mismatch as a port bug.
- Do not trust a codec cost guess from a different core and library: the written estimate was 2 to 4 times too high.
- Do not call a soft-float M33 run an M0+ run. It is a measurement of the M33 core without its FPU; the Cortex-M0+ figures are a model from an emulator.
