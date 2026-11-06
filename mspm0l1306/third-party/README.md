# Open-source C libraries and benchmarks on the TI LP-MSPM0L1306: what fits and how fast

**Short answer.** 40 of 42 images ran: 14 of 19 Embench-IoT benchmarks (13 pass their own check), micro-ecc P-256 (sign in 0.50 s with its assembly), jsmn, SHA-256, two printf libraries and hardware CRC (10.9 times faster than a table). Five Embench benchmarks and heatshrink do not fit in 4 kB of RAM.

Terms: [docs/vocab.md](../../docs/vocab.md) (Embench, stack, linker). Errors and fixes: [LP-MSPM0L1306 FAQ](../FAQ.md). 1 kB = 1,024 bytes.

Contents: short version, results on the board, what changed from the G3507 suite, images in run order, what does not fit, hardware CRC, run order, commands.

The MSPM0G3507 suite, rebuilt for the LP-MSPM0L1306: Cortex-M0+, 32 MHz from SYSOSC, 64 kB flash, 4 kB SRAM, hardware CRC only. Built with TI Arm Clang 5.1.1 against SDK 2.11.00.07. `build.sh` does not touch the board; `run_bench_l1306.py` flashes each image and reads its results (below).

## Short version

- **42 images built** (all with `-O2`; Embench and printf also with `-Os`). 15 Embench benchmarks, CRC-32, jsmn, micro-ecc (two variants), and the 4 printf variants.
- **5 benchmarks do not fit, all for RAM** (9 images, because Embench was tried at two optimization levels): heatshrink (7,076 bytes (6.9 kB) of static RAM), Embench huffbench (8,752), matmult-int (8,052), qrduino (8,280), sglib-combined (8,736). The chip has 4,096. Flash was never the limit: the biggest image is 42 kB of 64 kB. Numbers are in the table below.
- **Dropped on purpose:** the hardware AES test and the whole tiny-AES-c image (no AES on this chip; the software-only half is not built either, because the point of that image was software against hardware), and the hardware TRNG test plus the `rand()` comparison that went with it.
- **Kept:** hardware CRC. The L130x CRC block has the 32-bit polynomial (`CRC_SYS_CRC32_ENABLE` is 1 in `mspm0l130x.h`), so the same IEEE CRC-32 test runs unchanged. It also has CRC-16; this suite only tests CRC-32.
- No benchmark data size was changed. No benchmark was shrunk to fit.
- **Stack:** the depth of 40 of the 42 images is unknown (not measured), so each gets all RAM left after its static data. The two micro-ecc images get 2,048 bytes (2.0 kB), because the G3507 run measured at most 1,704 bytes (1.7 kB) for them.

## Results on the board

Run with `python3 run_bench_l1306.py` on probe <probe serial>, then the five images whose serial output was unreadable rerun with `python3 run_bench_l1306.py <list> 30`.

- **40 of 42 images ran and finished.** 39 passed every check. `embench/st` failed its own check at both levels, as it did on the G3507, where it was traced to Embench's 1e-13 tolerance being finer than one step of a double; not re-traced here.
- **Embench `wikisort` does not fit, though it links.** Its `benchmark_body` reserves 4,188 bytes (4.1 kB) of stack in one instruction (`ldr r7, =0xffffefa4; add sp, r7`), more than the chip's whole RAM. The stack pointer lands at 0x1FFFFEE0, below SRAM, and the first write faults into the default handler. The linker cannot see this; only running it shows it. Counted with the four Embench benchmarks above (heatshrink is not part of Embench): **5 of 19 Embench benchmarks do not fit in 4 kB.**
- **Serial capture.** Three images (the two eyalroz printf images and, on the first pass only, two Embench `-Os` images) printed a few garbled bytes before their result line; the eyalroz ones do it every time. The result lines were intact and are used below.

### Embench-IoT, cycles for one run (CPU_MHZ=1)

The L1306 needs fewer cycles than the G3507 for the same code at -O2 (median 0.93 times): one flash wait state at 32 MHz against two at 80 MHz. The -Os column swings both ways (0.63 to 1.47); each L1306 benchmark is its own image, so code placement differs from the G3507's packed images, and -Os timing is sensitive to it.

| Benchmark | L1306 -O2 | G3507 -O2 | L1306 / G3507 | L1306 -Os | G3507 -Os | L1306 / G3507 |
|---|--:|--:|--:|--:|--:|--:|
| aha-mont64 | 10,374,714 | 10,629,850 | 0.98 | 12,061,204 | 10,827,868 | 1.11 |
| crc32 | 6,499,531 | 9,470,466 | 0.69 | 7,492,653 | 6,887,860 | 1.09 |
| cubic | 10,021,552 | 10,878,777 | 0.92 | 10,080,200 | 7,171,362 | 1.41 |
| edn | 6,862,307 | 6,947,750 | 0.99 | 7,178,547 | 5,629,854 | 1.28 |
| minver | 8,679,738 | 8,931,530 | 0.97 | 8,644,763 | 9,868,500 | 0.88 |
| nbody | 6,429,041 | 6,985,891 | 0.92 | 6,423,742 | 7,643,319 | 0.84 |
| nettle-aes | 8,572,900 | 10,016,449 | 0.86 | 7,784,014 | 9,757,578 | 0.80 |
| nettle-sha256 | 7,028,059 | 7,213,314 | 0.97 | 7,499,854 | 11,156,598 | 0.67 |
| nsichneu | 4,495,705 | 4,773,930 | 0.94 | 4,519,088 | 7,120,385 | 0.63 |
| picojpeg | 5,660,711 | 6,085,085 | 0.93 | 6,731,035 | 6,897,295 | 0.98 |
| slre | 5,801,146 | 6,789,472 | 0.85 | 6,268,883 | 4,422,191 | 1.42 |
| st | 9,529,950, check fails | 10,318,553 | 0.92 | 9,669,088, check fails | 6,572,947 | 1.47 |
| statemate | 5,448,453 | 6,419,329 | 0.85 | 5,787,999 | 7,121,891 | 0.81 |
| ud | 12,504,702 | 13,042,249 | 0.96 | 6,912,697 | 6,035,250 | 1.15 |
| wikisort | does not fit (stack) | 3,689,596 | | does not fit (stack) | 8,972,786 | |
| huffbench, matmult-int, qrduino, sglib-combined | do not link (RAM) | | | | | |

### Libraries

| Library | Job | L1306 cycles | Time at 32 MHz | G3507 cycles (80 MHz) | Notes |
|---|---|--:|--:|--:|---|
| micro-ecc, Thumb assembly | P-256 key pair / sign / verify | 15.1 M / 16.0 M / 18.3 M | 0.47 / 0.50 / 0.57 s | 15.8 M / 16.7 M / 19.1 M | Stack 1,584 to 1,608 B (1.6 kB) of 2,048. Verify rejects a flipped bit. |
| micro-ecc, plain C | same | 35.2 M / 36.2 M / 41.0 M | 1.10 / 1.13 / 1.28 s | 37.3 M / 38.4 M / 43.4 M | Stack 1,640 to 1,704 B (1.7 kB). |
| B-Con SHA-256 | hash 1 kB | 153,793 to 156,668 | 4.8 ms | 160,704 | FIPS 180-4 "abc" vector passes |
| jsmn | parse 300 bytes of JSON, 51 tokens | 14,774 | 0.46 ms | 17,292 | |
| TI libc `snprintf` | one line with ints, hex, a float | 30,098 (-O2), 30,262 (-Os) | 0.94 ms | 31,062 | |
| eyalroz/printf | same line | 37,097 (-O2), 37,157 (-Os) | 1.16 ms | 34,177 | |
| eyalroz/printf, no float | same line without the float | 9,037 (-O2), 8,734 (-Os) | 0.28 ms | 9,059 | |

### Hardware CRC against software (CRC-32, 1 kB)

| Method | Cycles | Against the table |
|---|--:|--:|
| Software, bitwise | 77,877 | 6.4 times slower |
| Software, 256-entry table in flash | 12,211 | 1 |
| Hardware, byte feeds | 4,303 | 2.8 times faster |
| Hardware, 32-bit word feeds | 1,121 | **10.9 times faster** |
| Hardware, `DL_CRC_calculateBlock32` | 1,790 | 6.8 times faster |

All five give the same CRC, and the check value for "123456789" matches. On the G3507 the word-feed hardware CRC was 13.5 times faster than the table.

## What changed from the G3507 suite

| Item | G3507 | L1306 |
|---|---|---|
| Clock | 80 MHz from SYSPLL | 32 MHz from SYSOSC, default clock tree (flash gets 1 wait state at 32 MHz, set by SYSCTL automatically) |
| UART0 | PA10 TX, PA11 RX, 40 MHz bus clock | PA8 TX, PA9 RX, 32 MHz, divisors 17 and 23 at 115200 (SysConfig output in `syscfg/`) |
| Startup file | `startup_mspm0g350x_ticlang.c` | `startup_mspm0l130x_ticlang.c` |
| SRAM | 32 kB at `0x20200000` | 4 kB at `0x20000000` |
| Harness | `common/tb.c`, `tb.h` | Same files, read from the original folder. `tb.c` is built from a `sed` copy that only changes one include path (it pointed at `bench_timer.h` by a path that only works in the old tree). `TB_MAX_RESULTS` is set per image to the number of result lines (the original reserves 40 entries, which is 1.1 kB of the 4 kB). |
| Embench | 3 images, 19 benchmarks | one image per benchmark, 19 benchmarks, 2 optimization levels |
| tiny-AES-c, hardware AES | `aes` image | dropped |
| `crc_trng` | CRC-32 plus TRNG | `crc`: same CRC-32 test, TRNG part removed (`crc/main.c`) |
| `text` | heatshrink and jsmn in one image | split into `heatshrink` and `jsmn` (`text_hs/main.c`, `text_jsmn/main.c`; the code is the original with the other half deleted) |
| micro-ecc, printf, Embench harness | | original sources and `main.c` files, by path, unedited |

The output format, the `BENCH START`, `R,...` and `BENCH DONE` lines, the RAM copy in `tb_res` with magic `0x7141E002`, and every correctness check are the same as the original.

Embench speed ratio changes at 32 MHz: `baseline_ms / (cycles / 32000 x 16)` (the G3507 README used 80000).

## Images, in run order

Sizes are from the linker output. Flash is text plus initialized data. Static RAM is `.data` plus `.bss`, not counting the stack. The image does not fit if static RAM plus a 256 byte stack is more than 4,096 bytes (4.0 kB), or flash is over the 65,528 byte linker region.

| Image | Flash (of 64 kB) | Static RAM (of 4 kB) | Stack set | Stack basis |
|---|--:|--:|--:|---|
| crc_O2 | 11,368 (17.3%) | 1,272 | 2,824 | all remaining RAM, depth unknown |
| printf_none_O2 | 2,432 (3.7%) | 48 | 4,048 | all remaining RAM, depth unknown |
| printf_ti_O2 | 13,388 (20.4%) | 52 | 4,040 | all remaining RAM, depth unknown |
| printf_eyalroz_O2 | 15,456 (23.6%) | 48 | 4,048 | all remaining RAM, depth unknown |
| printf_eyalroz_nofloat_O2 | 9,168 (14.0%) | 48 | 4,048 | all remaining RAM, depth unknown |
| printf_none_Os | 1,888 (2.9%) | 48 | 4,048 | all remaining RAM, depth unknown |
| printf_ti_Os | 9,836 (15.0%) | 52 | 4,040 | all remaining RAM, depth unknown |
| printf_eyalroz_Os | 9,992 (15.2%) | 48 | 4,048 | all remaining RAM, depth unknown |
| printf_eyalroz_nofloat_Os | 4,744 (7.2%) | 48 | 4,048 | all remaining RAM, depth unknown |
| jsmn_O2 | 4,640 (7.1%) | 1,072 | 3,024 | all remaining RAM, depth unknown |
| uecc_asm_O2 | 33,400 (51.0%) | 1,248 | 2,048 | measured on G3507 (1,704 max) plus margin |
| uecc_c_O2 | 42,136 (64.3%) | 1,248 | 2,048 | measured on G3507 (1,704 max) plus margin |
| embench_aha_mont64_O2 | 6,344 (9.7%) | 65 | 4,024 | all remaining RAM, depth unknown |
| embench_crc32_O2 | 4,340 (6.6%) | 52 | 4,040 | all remaining RAM, depth unknown |
| embench_cubic_O2 | 14,572 (22.2%) | 92 | 4,000 | all remaining RAM, depth unknown |
| embench_edn_O2 | 7,080 (10.8%) | 1,658 | 2,432 | all remaining RAM, depth unknown |
| embench_minver_O2 | 9,464 (14.4%) | 160 | 3,936 | all remaining RAM, depth unknown |
| embench_nbody_O2 | 8,380 (12.8%) | 372 | 3,720 | all remaining RAM, depth unknown |
| embench_nettle_aes_O2 | 17,312 (26.4%) | 1,592 | 2,504 | all remaining RAM, depth unknown |
| embench_nettle_sha256_O2 | 8,000 (12.2%) | 168 | 3,928 | all remaining RAM, depth unknown |
| embench_nsichneu_O2 | 13,644 (20.8%) | 116 | 3,976 | all remaining RAM, depth unknown |
| embench_picojpeg_O2 | 27,012 (41.2%) | 2,445 | 1,648 | all remaining RAM, depth unknown |
| embench_slre_O2 | 7,904 (12.1%) | 112 | 3,984 | all remaining RAM, depth unknown |
| embench_st_O2 | 8,532 (13.0%) | 1,680 | 2,416 | all remaining RAM, depth unknown |
| embench_statemate_O2 | 8,128 (12.4%) | 275 | 3,816 | all remaining RAM, depth unknown |
| embench_ud_O2 | 7,544 (11.5%) | 1,812 | 2,280 | all remaining RAM, depth unknown |
| embench_wikisort_O2 | 10,864 (16.6%) | 3,256 | 840 | all remaining RAM, depth unknown |
| embench_aha_mont64_Os | 3,848 (5.9%) | 65 | 4,024 | all remaining RAM, depth unknown |
| embench_crc32_Os | 3,452 (5.3%) | 52 | 4,040 | all remaining RAM, depth unknown |
| embench_cubic_Os | 13,668 (20.9%) | 92 | 4,000 | all remaining RAM, depth unknown |
| embench_edn_Os | 4,752 (7.3%) | 1,658 | 2,432 | all remaining RAM, depth unknown |
| embench_minver_Os | 4,864 (7.4%) | 160 | 3,936 | all remaining RAM, depth unknown |
| embench_nbody_Os | 5,612 (8.6%) | 372 | 3,720 | all remaining RAM, depth unknown |
| embench_nettle_aes_Os | 15,112 (23.1%) | 1,592 | 2,504 | all remaining RAM, depth unknown |
| embench_nettle_sha256_Os | 6,848 (10.4%) | 168 | 3,928 | all remaining RAM, depth unknown |
| embench_nsichneu_Os | 13,028 (19.9%) | 116 | 3,976 | all remaining RAM, depth unknown |
| embench_picojpeg_Os | 10,676 (16.3%) | 2,445 | 1,648 | all remaining RAM, depth unknown |
| embench_slre_Os | 5,064 (7.7%) | 112 | 3,984 | all remaining RAM, depth unknown |
| embench_st_Os | 4,844 (7.4%) | 1,680 | 2,416 | all remaining RAM, depth unknown |
| embench_statemate_Os | 6,544 (10.0%) | 275 | 3,816 | all remaining RAM, depth unknown |
| embench_ud_Os | 2,784 (4.2%) | 1,812 | 2,280 | all remaining RAM, depth unknown |
| embench_wikisort_Os | 11,824 (18.0%) | 3,256 | 840 | all remaining RAM, depth unknown |

Where the stack basis says "all remaining RAM", the depth was not measured, so the stack was set to everything the static data leaves free (rounded down to 8 bytes, so a few bytes of slack show). If an image is wrong or hard-faults on the board, suspect stack depth first. `tb_stack_paint()` and `tb_stack_used()` are in the harness if you want to measure. Two with the least stack are `embench_wikisort` (840 bytes) and `embench_picojpeg` (1,648 bytes (1.6 kB)). A finished image has `tb_res.magic == 0x7141E002`.

## Do not fit

Linker numbers, from a relink into an oversized memory map. All are RAM overruns. The static data is arrays inside the benchmark itself, so they cannot shrink without changing the benchmark.

| Image | Flash | Static RAM | Why |
|---|--:|--:|---|
| heatshrink_O2 | 9,136 | 7,076 | RAM over by 3,236 bytes (3.2 kB) with a 256 byte stack (2,980 over with no stack at all). Flash is fine. |
| embench_huffbench_O2 | 6,596 | 8,752 | RAM over by 4,912 bytes (4.8 kB) with a 256 byte stack (4,656 over with no stack at all). Flash is fine. |
| embench_matmult_int_O2 | 6,080 | 8,052 | RAM over by 4,212 bytes (4.1 kB) with a 256 byte stack (3,956 over with no stack at all). Flash is fine. |
| embench_qrduino_O2 | 16,244 | 8,280 | RAM over by 4,440 bytes (4.3 kB) with a 256 byte stack (4,184 over with no stack at all). Flash is fine. |
| embench_sglib_combined_O2 | 8,964 | 8,736 | RAM over by 4,896 bytes (4.8 kB) with a 256 byte stack (4,640 over with no stack at all). Flash is fine. |
| embench_huffbench_Os | 4,036 | 8,752 | RAM over by 4,912 bytes (4.8 kB) with a 256 byte stack (4,656 over with no stack at all). Flash is fine. |
| embench_matmult_int_Os | 4,120 | 8,052 | RAM over by 4,212 bytes (4.1 kB) with a 256 byte stack (3,956 over with no stack at all). Flash is fine. |
| embench_qrduino_Os | 10,532 | 8,280 | RAM over by 4,440 bytes (4.3 kB) with a 256 byte stack (4,184 over with no stack at all). Flash is fine. |
| embench_sglib_combined_Os | 5,420 | 8,736 | RAM over by 4,896 bytes (4.8 kB) with a 256 byte stack (4,640 over with no stack at all). Flash is fine. |

heatshrink keeps a 2,048 byte output buffer (`decomp`) and a 3,080 byte compressed buffer (`comp`) in RAM, plus encoder and decoder state; the 2 kB text input is in flash. A rewrite that streams and compares instead of buffering would fit, but that is a different test, so it was not done. Embench has no smaller documented size setting.

## Hardware CRC on the L130x

`DL_CRC_init` takes `DL_CRC_16_POLYNOMIAL` or `DL_CRC_32_POLYNOMIAL` (DriverLib `dl_crc.h`; the 32-bit one exists because `CRC_SYS_CRC32_ENABLE` is 1). The polynomial is fixed in silicon (CRC-16 0x1021, CRC-32 0x04C11DB7), so there is no custom polynomial. The test uses CRC-32 with bit reversal on, seed `0xFFFFFFFF` and a software final XOR, the same as on the G3507. If the check value `0xCBF43926` fails, `hw_check` tries the eight combinations and names the one that works. The L130x DriverLib has the same CRC calls (`feedData8`, `feedData32`, `calculateBlock32`) as the G3507.

## Run order

`build.sh` writes `images.txt`, the 42 `.out` files with absolute paths, in this order: `crc`, the 8 printf images, `jsmn`, `uecc_asm`, `uecc_c`, then the 15 Embench benchmarks at `-O2`, then at `-Os`. Shortest first. Expect about 5 to 10 seconds per image for flash and run, and `uecc_c` is the slowest (about 40 s at 32 MHz). A run of all 42 should take under 10 minutes.

## Commands

    cd mspm0l1306/third-party
    ./build.sh                       # all images, about 10 minutes (the original suite's vendor/ is reused by path)
    ./build.sh crc embench_st        # only images whose name contains a word
    python3 run_bench_l1306.py       # flashes and collects: needs the board, probe <probe serial>, tty /dev/cu.usbmodemML1300011

`run_bench_l1306.py [images.txt] [timeout_s]` is a copy of the original runner. It overrides the probe (`ML130001`), the serial port, the OpenOCD config (`openocd_l1306.cfg`, same lines as `../tools/ocd.sh`) and the safe memory map (flash `0x0` to `0x10000`, SRAM `0x20000000` to `0x20001000`; NONMAIN is refused). It writes `results/<image>.txt` and `results/runs.jsonl`.

Files: `build.sh`, `syscfg/` (SysConfig output and `bench32.syscfg`), `crc/`, `text_hs/`, `text_jsmn/` (copied mains), `out/` (images, maps, `sizes.txt`, `nofit.txt`).
