# wasm3: run WebAssembly on the MSPM0G3519, MSPM0G3507 and MSPM33C321A

wasm3 v0.9.0 (MIT), a WebAssembly interpreter in C, builds for the Arm Cortex-M0+ MSPM0G3519 and MSPM0G3507 and the Cortex-M33 MSPM33C321A. One 346-byte `.wasm` file says hello over a host function, blinks an LED through an imported function and runs a Fibonacci and an integer-loop benchmark. The firmware is 55.6 kB of flash on the G3519 and 76.4 kB on the M33; a tiny configuration runs the same file in 11.1 kB of RAM, so it also fits the G3507's 32 kB. **Status: Works on the LP-MSPM0G3519 (PASS, fib 43.5 times and the loop 28.9 times slower than C; 14 passes in 60 s with the heap flat). The G3507 and M33 images are built and checked in an emulator and on the Mac, not flashed.**

Terms: [vocab](../../docs/vocab.md). 1 kB = 1,024 bytes. Checked 2026-10-21 on an Intel Mac, macOS 15.7, GNU Arm Embedded 14.3.1, MSPM0 SDK 2.11.00.07, MSPM33 SDK 1.04.00.00, wasm3 v0.9.0 (commit `0cd38327`), wabt 1.0.39 (npm) to turn text into Wasm.

## What is here

| Part | File |
|---|---|
| wasm3 v0.9.0, unmodified | `third-party/wasm3/` |
| The test app in WebAssembly text and binary | `app/app.wat`, `app/app.wasm`, `app/app_wasm.h` (made by `app/build_app.js`) |
| Runner: loads the module, links four host functions, checks results against C | `common/frl_wasm.c`, `common/native.c` |
| Mac build | `host/build.sh` |
| LP-MSPM0G3519 and LP-MSPM0G3507 | `boards/g3519/` (`BOARD=g3507` for the small chip) |
| LP-MSPM33C321A | `boards/m33c321a/` |
| Three patches (see Limits) | `patches/000*.patch`, applied to a build copy |
| Emulator check | `tests/emu_check.py` |

The app imports `env.putc`, `env.led`, `env.delay_ms` and `env.print_i32`, and exports `run(blinks)`, `fib(n)` and `bench(n)`. It declares no linear memory (a Wasm memory page is 64 kB), which is why it fits small parts. The runner compares both benchmark results with the same algorithms written in C and fails on a mismatch (fib(20) = 6765, bench(20000) = 3486216529).

## Build and run

```bash
export PATH=/opt/ST/STM32CubeCLT_1.22.0/GNU-tools-for-STM32/bin:$PATH    # any arm-none-eabi-gcc 12 or newer
ports/wasm3/host/build.sh                       # Mac: builds and runs, prints the report
ports/wasm3/build_all.sh                        # host + G3519 + G3507 (two configs) + M33, emulator checks
bash ports/wasm3/tests/run_security_tests.sh      # call_ref stack-overflow test, patched against unpatched
```

`build_all.sh` needs `pip install unicorn` for the emulator step (set `EMU_PY` to that Python). To rebuild `app.wasm` from `app.wat`: `npm install wabt` somewhere, then `NODE_PATH=<there>/node_modules node app/build_app.js`.

## Size

| Image | Flash | Static RAM (data+bss, incl. 16 kB heap) | C stack | RAM total | Fits |
|---|---|---|---|---|---|
| G3519, standard | 55.6 kB of 512 kB | 16.2 kB | 8 kB | 24.2 kB of 128 kB | yes |
| G3507, standard | 55.4 kB of 128 kB | 16.1 kB | 8 kB | 24.1 kB of 32 kB | yes |
| G3507, tiny (6 kB heap, 1 kB Wasm stack) | 55.4 kB of 128 kB | 6.1 kB | 5 kB | 11.1 kB of 32 kB | yes |
| M33C321A | 76.4 kB of 1 MB | 20.5 kB | 4 kB | 24.5 kB of 256 kB | yes |

Source: the linker symbols and the size tool. wasm3 runs from a fixed heap (`d_m3FixedHeap`), so no `malloc` is linked. The app used 5.9 kB of the heap on the M0+ and 8.9 kB on the M33 (`heap_used` in the emulator reports), which is why a 6 kB heap works. The wasm3 README gives a minimum of about 64 kB of code and 10 kB of RAM; v0.5.0 built to 43 kB of flash on the M0+; v0.9.0 adds a validator and builds to 55 kB, so the code is above that figure here, and the tiny RAM figure is 11.1 kB including the C stack. Build options that mattered: `d_m3CodePageAlignSize=1024` (the default code-page size of 4 kB ran a 16 kB heap out at link time), no floating point on the M0+ (`d_m3HasFloat=0`), `-Os`.

The C stack is large because wasm3 nests a native frame per Wasm call: fib(20) needs about 4 kB. A 3.5 kB native budget trapped fib(20) on the M0+ (`[trap] stack overflow`); 4 kB passes.

## Speed

An interpreter such as this costs 10 to 40 times the same loop in C. Measured on the Mac (clang -O2, fib(30) and 2,000,000 loop passes): Wasm 59 ms against native 2.7 ms for fib (21.8 times) and 39 ms against 2.1 ms for the loop (18.9 times).

On the chips, an ESTIMATE from the emulator. It counts instructions in the Wasm run and in the same C algorithm, in one run each (fib(20), 20,000 passes). The ratio of the two counts predicts the slowdown if both run at similar cycles per instruction; flash wait states and branches make that only roughly true.

| Chip | fib(20), Wasm / native instructions | loop, Wasm / native | Estimated slowdown |
|---|---|---|---|
| Cortex-M0+ (G3519, G3507) | 5.68 M / 0.153 M | 5.22 M / 0.280 M | 37 times (fib), 19 times (loop) |
| Cortex-M33 (C321A) | 4.23 M / 0.153 M | 3.78 M / 0.180 M | 28 times (fib), 21 times (loop) |

At one cycle per instruction, fib(20) in Wasm would take at least 71 ms at 80 MHz (G3519) and 132 ms at 32 MHz (M33 as built, 32 MHz oscillator). Measured times on the G3519 are in "On the board" below; the image prints `fib_wasm_us` and `fib_native_us` itself.

## What the emulator shows, and does not

The emulator (Unicorn) runs the real firmware instructions on a Cortex-M0+ or Cortex-M33 model. Peripherals are stubs, SysTick is a counter advanced by a hook, and interrupts do not run, so every microsecond figure the image prints under emulation is meaningless. It shows the code starts, the module loads, imports link, the LED function is called six times (on and off, three blinks), the results match C, and the instruction tallies above. It does not show the UART, the LED pin, the clock or real speed. A pass here is not a pass on a chip.

## Limits

- Run on the LP-MSPM0G3519 only (section "On the board"). The G3507 and M33 images are not flashed.
- The M33 image stays on the 32 MHz oscillator it starts on (no SysConfig output is used); the G3519 and G3507 image sets the PLL to 80 MHz with the same errata check as `ports/micropython/mphalport.c`. Estimates for 160 MHz would be a scaling, not a result.
- The M33 image has no console: the Education BoosterPack covers the serial pins, so text goes to the RAM buffer `frl_log` and results to `frl_result`, read over the probe. The G3519 prints on UART0 (PA10) at 115200 and also fills `frl_result`.
- G3519 LED is PB27 (green); M33 LED is PA2 (blue, active high as in the SDK example). The G3507 image uses PB27 too and has not been checked against that board's LED wiring.
- Three patches, applied to a build copy by `patches/apply.sh` (`third-party/wasm3/` is not changed). (1) `op_CallRef` lacks the native-stack check its sibling call ops have (`source/m3_exec.h`). Call_ref is off by default (`d_m3HasTypedRefs=0`), so no image here contains it, but `d_m3MaxNativeStack` is set on every MCU build (stack minus 512 bytes; the default budget is about 8 MB and never trips on a part with a 4 to 8 kB stack). (2) `memory.grow` adds page counts in 32 bits, which wraps with one-byte pages; the patch adds in 64 bits and returns -1 above the maximum. (3) A constant expression runs with an empty memory header instead of NULL; v0.9.0 already rejects other opcodes there. `tests/run_security_tests.sh` builds each test twice with AddressSanitizer and UBSan: call_ref recursion unpatched aborts, patched traps with `[trap] stack overflow`; the memory.grow test leaves the size at 0x3FFFFFFF instead of 0xC0000000 unpatched and returns -1 patched; the init-expression test is rejected both ways. The upstream reports are drafted and not filed.
- The WASI, libc and tracer API files are left out of the build.
- The app has no linear memory. A module with memory needs 64 kB for one page, so it does not fit the G3507.

## Flash and read the results

Take the probe lock first (`tools/bench/README.md`). LP-MSPM0G3519, from the repo root; send the binary as written in `ports/micropython/README.md`, "First boot":

```bash
DSL=/Applications/ti/ccs2101/ccs/ccs_base/DebugServer/bin/DSLite
$DSL flash -c mspm0g3519/MSPM0G3519_MG000001.ccxml -e -f -v ports/wasm3/build-g3519/wasm3.bin,0x0
# UART0 at 115200 (back-channel) prints the report; the green LED blinks 3 times every 3 s. Or read the results block:
ADDR=0x$(arm-none-eabi-nm ports/wasm3/build-g3519/wasm3.elf | awk '/ frl_result$/{print $1}')
$DSL memory -c mspm0g3519/MSPM0G3519_MG000001.ccxml -r $ADDR,72 -s 8 -o /tmp/r.bin && python3 ports/wasm3/tools/decode_result.py /tmp/r.bin
```

LP-MSPM33C321A. Back up its flash first. The map shows nothing in BCR_CONFIG or BSL_CONFIG; the image has only the `.intvecs`, `.text`, `.rodata` and `.data` sections at 0x10000000 and 0x30000000.

```bash
DSL=/Applications/ti/ccs2101/ccs/ccs_base/DebugServer/bin/DSLite
$DSL flash -c mspm33c321a/MSPM33C321A_M3010001.ccxml -e -f -v ports/wasm3/build-m33/wasm3.out
sleep 8
ADDR=0x$(arm-none-eabi-nm ports/wasm3/build-m33/wasm3.elf | awk '/ frl_result$/{print $1}')
$DSL memory -c mspm33c321a/MSPM33C321A_M3010001.ccxml -r $ADDR,72 -s 8 -o /tmp/r.bin && python3 ports/wasm3/tools/decode_result.py /tmp/r.bin
```

A pass reads `status 0`, `fib_val 6765`, `bench_val 3486216529`. The blue LED blinks 3 times every 3 s. For the LP-MSPM0G3507, `BOARD=g3507` images are `build-g3507-std/` and `build-g3507-tiny/`; flash the same way with that board's `.ccxml`. Only the G3519 run is measured.

## On the board (LP-MSPM0G3519, 2026-10-21)

The G3519 image (16-byte padded binary, DSLite) runs at 80 MHz and prints PASS on the backchannel UART at 115200:

| Measure | WebAssembly | Native C | Ratio |
|---|---|---|---|
| fib(20) = 6765 | 167.0 ms | 3.84 ms | 43.5x |
| Loop, 20,000 passes | 173.9 ms | 6.01 ms | 28.9x |
| Module load (346 bytes) | 1.03 ms | | |

The board ratios are a little worse than the emulator estimate (37x and 19x). The first image leaked about 6.9 kB of the 16 kB heap per pass of the app loop (`heap_used` 6.9 kB, then 13.8 kB). Fixed 2026-10-21: each pass frees the runtime and environment and resets the fixed heap. On the board the fixed image ran 14 passes in 60 s, every one PASS with `heap_used` at 6.9 kB.
