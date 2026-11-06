# What goes wrong when you build the Plaits synthesis code for the LP-MSPM33C321A LaunchPad, and how do you avoid it?

Built from the Plaits DSP (pichenettes/eurorack commit f4cfa76, stmlib e3bd7c9) with TI Arm Clang 5.1.1.LTS and the MSPM33 SDK 1.04.00.00 on the LP-MSPM33C321A. Each answer starts with the fix. Terms: [docs/vocab.md](../../docs/vocab.md). 1 kB = 1,024 bytes. Numbers and method: [README.md](README.md).

**What caused it**, one tag per answer: Our mistake, Setup or handling, TI example default, Tool or docs, Chip limit, Third-party behavior, Unknown.

## Build

### Can TI Arm Clang build C++ code like Plaits, and which flags does it need?

Yes: compile with `-std=c++11 -fno-exceptions -fno-rtti` and link `libc++.a`, `libc++abi.a` and `libc.a` from the compiler's `lib` folder. All 40 source files (Plaits and stmlib) compile with no source change and the image links with no heap. The engines are classes with virtual functions, so the constructors run through the C++ start-up, which the `--rom_model` link handles. `ports/plaits/target-m33/build.sh` has the whole line.

### The compile stops with `expected member name or ';' after declaration specifiers` inside `hw_memcfg.h`. What is wrong?

Define `TEST` only for the Plaits files, not for the files that include the SDK. Plaits builds off the module with `-DTEST`, which turns off the STM32 flash-writing code in `user_data.h`. The SDK header `hw_memcfg.h` has a register field named `TEST`, so `-DTEST` on `ti_msp_dl_config.h` breaks it. `build.sh` passes `-DTEST` to the Plaits files and to `plaitsrun.cc`, and leaves the board `main.cc` and the SDK sources without it.

### Does it need the FPU, and which optimization level and how much flash?

Yes, keep the FPU on; `-O2` is a good default. With the FPU off every engine costs 11 to 39 times the cycles (149% to 1,140% of real time at 160 MHz). `-O3` is 3% faster for 211.7 kB of flash; `-Os` is 164.2 kB but costs 1.4 times on additive and 1.6 times on modal. `-O2` is 202.2 kB. Table in [README.md](README.md).

### Linking a soft-float image fails with `Tag_ABI_VFP_args ... combining incompatible files`. How do you link it?

Build the four driverlib sources the clock setup needs with the soft-float flags instead of linking `driverlib.a`. The SDK archive for this chip is hard-float. `build.sh` does this for the `soft` option; the same fix is in `ports/lc3/FAQ.md`.

## Rendering

### Every engine I select produces the same sound. Why?

Set `Patch::engine` to the engine number as an integer and leave `Modulations::engine` at 0. `Voice::Render` adds `patch.engine` to the engine CV as the integer base of a hysteresis quantizer; the CV is the fraction from 0 to 1. Passing `engine / 23.0f` as `patch.engine` rounds to 0 and always selects the first engine (24 engines, CRC `909d5311` for all 24). The engine order in this version is 0 VA with filter, 1 phase distortion, 2 to 4 six-operator FM, 5 wave terrain, 6 string machine, 7 chiptune, 8 to 15 the original eight, 16 to 23 the second eight; `pl_names` in `common/plaitsrun.cc` lists them.

### The first block of an engine takes much longer than the rest. Is that a fault?

No. `Voice::Render` resets the engine and loads its data when the engine number changes: 41.7k cycles for the six-operator FM banks (104% of a 12-sample block at 160 MHz), 39.5k for the string machine, 112.0k for particle and 107.4k for string. Switch engines while the output is muted, or crossfade two voices. Steady blocks average 4.3k to 13.8k cycles, the worst 14.5k.

### The speech engine misses real time on some blocks. What happens?

Buffer about 7 blocks of output, or keep the harmonics control steady. In the test the three blocks where harmonics jumps from 0.3 to 0.5, 0.7 and 0.9 cost 66.5k, 105.3k and 248.3k cycles (166%, 263% and 621% of real time). Every other speech block costs 3.9k to 8.0k. The cost is in the engine's own code; which part was not isolated. 84 samples of buffer (1.75 ms) cover the worst one.

### How much RAM does a voice need?

8.7 kB for the `Voice` object and a 16 kB buffer shared by all engines; the stack peaked at 0.8 kB in the test. `Voice::Init` calls `allocator->Free()` before each engine's `Init`, so the engines overlap in the same 16 kB. A second voice needs its own 16 kB buffer and its own 8.7 kB object.

## Bit-exactness

### The board's samples differ from my Mac's. What is different?

Build both sides with `-ffp-contract=off`, no `-ffast-math`, and the FPU on. With those choices all 24 engines match the Mac in every block at `-O2`, `-O3` and `-Os`. With the FPU off, 20 engines differ in a few blocks each (373 of 6.1k blocks), the cause was not isolated (candidates are float-to-integer conversion and the library `sinf`, `cosf` and `tanf`). The TI compiler does not fuse multiply-adds in this code at its default (`-ffp-contract=on`), so that flag changes nothing here; keep it off anyway so a compiler that does fuse cannot move the result.
