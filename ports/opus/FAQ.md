# What goes wrong when you build libopus for the LP-MSPM33C321A LaunchPad, and how do you avoid it?

Built from libopus 1.5.2 with TI Arm Clang 5.1.1.LTS and the MSPM33 SDK 1.04.00.00 on the LP-MSPM33C321A. Each answer starts with the fix. Terms: [docs/vocab.md](../../docs/vocab.md). 1 kB = 1,024 bytes. Numbers and method: [README.md](README.md).

**What caused it**, one tag per answer: Our mistake, Setup or handling, TI example default, Tool or docs, Chip limit, Third-party behavior, Unknown.

## Bit-exactness

### The encoded packets from the board differ from my Mac's at complexity 5 and above, and complexity 0 matches. What is different?

Build the Mac side with 32-bit multiply macros, or the board side with 64-bit ones. `celt/arch.h` sets `OPUS_FAST_INT64` to 1 when `__x86_64__`, `__LP64__` or `_WIN64` is defined and to 0 otherwise, and the two settings pick different multiply macros in `celt/fixed_generic.h` and `silk/macros.h`. A 64-bit Mac and the 32-bit M33 therefore run different arithmetic. The CELT pre-filter, used from complexity 5, is where the results part; SILK and complexity 0 match. With the same macros on both sides every packet and every decoded frame matched (12 of 12 runs against the Mac build with 32-bit macros; the forced-64-bit board build matched the stock Mac the same way). A command-line `-D` does not set the macro, because `arch.h` defines it with no guard and its definition wins; the port builds from a copy of the library with the one line changed (`FASTINT64=1` in `target-m33/build.sh`, `INT64=0` in `host/build.sh`). Both sets are valid Opus.

### The floating-point build's decoded samples differ from the Mac's for the same packets. Is it broken?

Not shown to be. The suspected cause is Third-party behavior. In the CELT runs 2 to 36 percent of the 10 and 20 ms frames decode to different samples with identical packets. The float code calls `exp2`, `log` and `cos` from the C math library, and the library on the board is not the Mac's. The size of the difference in samples was not measured, so it is not known whether it is one least-significant bit. Compare fixed-point builds when you need CRCs to match across machines.

### I turned on `OPUS_ARM_INLINE_EDSP` and the output changed. Why?

The library's Arm assembly multiplies are not the same arithmetic as the C macros. `-DOPUS_ARM_INLINE_EDSP` makes SILK and CELT faster by 4 to 12 percent, SILK output stays equal to the C build, and CELT output differs in 23 to 82 percent of packets. Use it where a stream only has to be valid Opus, not equal to a reference.

## Speed

### The SILK encoder takes 2.5 times the core at complexity 5. What do I set?

Use complexity 0 for 16 kHz voice on this core, or CELT. This holds for 160 MHz with two flash wait states. At 160 MHz SILK wideband encode is 56 percent of real time at complexity 0, 246 percent at 5 and 480 percent at 10, mono, with the fixed-point build. CELT at 16 kHz and 10 ms is 21 percent at complexity 0 and 43 percent at 5 (`OPUS_APPLICATION_RESTRICTED_LOWDELAY` forces CELT). The SILK decoder is 5 percent and is not the problem. Instructions on the Mac rise by 2.3 times from complexity 0 to 5 and cycles on the board by 4.1 times, encode and decode together; the reason is not isolated.

### Why is the floating-point build slower than the fixed-point one?

Part of libopus's float code is double precision. The core has a single-precision FPU, so each double operation is a library call: 1,080 call sites in the float image, none in the fixed one. Float is slower in all 12 runs (SILK encode 3.3 to 7.2 times, CELT decode 1.6 to 2.4 times). Use the fixed-point build (`FIXED_POINT`, `DISABLE_FLOAT_API`) on this part.

### Which optimization level do I use?

`-Os`. It halves the flash (164.7 kB against 331.4 kB at `-O2`) with encode within 1 percent of `-O2` and decode 6 percent faster in the CELT 48 kHz row (other rows move up to 4 percent). `-O3` is 5 percent larger than `-O2` and faster by at most 4 percent on encode and not faster on every row.

## Build

### How do I build libopus without autotools or CMake?

List the sources from the library's own `*_sources.mk` and supply a `config.h`. Take `OPUS_SOURCES`, `CELT_SOURCES` and `SILK_SOURCES`, then `SILK_SOURCES_FIXED` for fixed-point or `OPUS_SOURCES_FLOAT` and `SILK_SOURCES_FLOAT` for float (`ports/opus/sources.sh` does this). Add `-I` for `include`, `celt`, `silk`, `src` and `silk/fixed` or `silk/float`. The `config.h` needs `OPUS_BUILD`, `VAR_ARRAYS` (or `USE_ALLOCA`) and either `FIXED_POINT` or `FLOATING_POINT`; add `DISABLE_FLOAT_API` for a pure integer build. Leave out `HAVE_LRINT` so the Mac and the board round alike. `src/repacketizer.c` is needed even for one stream: constant-bit-rate padding calls it.

### How much stack does the encoder need?

Reserve 32 kB for the fixed-point build and 40 kB for the float build. The stack was painted before a run and the deepest use over the 12 runs was 27.5 kB (fixed) and 34.6 kB (float), at encoder complexity 10. libopus takes its scratch buffers from the stack (`VAR_ARRAYS`), so the figure belongs to the task that calls `opus_encode`. The codec state is separate and comes from your memory: 24.0 kB (fixed) or 42.5 kB (float) for the encoder, 17.4 or 17.8 kB for the decoder.

### How do I make sure the encoder uses SILK or CELT?

Set the application, bandwidth and signal type, then read the first byte of the packet. `OPUS_APPLICATION_VOIP` with `OPUS_BANDWIDTH_WIDEBAND` and `OPUS_SIGNAL_VOICE` at 32 kbps gave SILK-only packets (first byte 0x48 to 0x4b); `OPUS_APPLICATION_AUDIO` at 48 kHz and 96 kbps gave CELT (0xf0 for 10 ms, 0xf8 for 20 ms); `OPUS_APPLICATION_RESTRICTED_LOWDELAY` at 16 kHz gave CELT (0xb0). The top five bits of the first byte are the configuration: below 12 is SILK, 12 to 15 hybrid, 16 and above CELT.

### Opus or LC3 on this core?

LC3 costs less on every row except the SILK decoder. At 96 kbps and 48 kHz, 10 ms frames, Opus CELT needs 1.2 to 2.2 times the LC3 encode cycles and 1.6 to 2.1 times the decode cycles; at 32 kbps and 16 kHz, 1.7 to 3.5 times encode and 3.1 to 3.7 times decode, and SILK encode is 4.5 times LC3 at complexity 0 and 19.8 times at 5, with decode equal. The table is in the [README](README.md).

## Reading results

### `DSLite memory -r <addr>,<n> -s 8` returns more bytes than asked for.

Ask for the count you need and keep the first bytes. See the same entry in the [LC3 FAQ](../lc3/FAQ.md). `run_board.py` here reads 324 words of results and 9.6 kB of per-frame CRCs this way.

## Mistakes to avoid

- Do not read a CRC mismatch between a 64-bit Mac and the board as a port bug before checking `OPUS_FAST_INT64`.
- Do not size a SILK encoder budget from CELT numbers: at complexity 5 the SILK encoder was 3 to 9 times the written estimate.
- Do not use the float build on a part with a single-precision FPU unless you have counted the double-precision calls.
