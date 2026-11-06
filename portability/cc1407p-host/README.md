# CC1407P host driver on a bare microcontroller

A small microcontroller with no RTOS can drive the BP-CC1407P-9-FE sub-1 GHz radio with about 10.2 kB of flash and 5.1 kB of RAM (2.1 kB with 4 buffers). TI's host driver is plain C and needs seven things from the host: SPI, a chip-select pin, a read of the MISO line, the reset pin, an interrupt pin, a delay and a timer. This folder holds the part written in this repo, and the script that measures what the whole thing costs in flash and RAM. Built 2026-10-09. **Nothing here has run on a radio**: the build proves the size, not the link.

Terms: [docs/vocab.md](../../docs/vocab.md).

| File | What it is |
|---|---|
| `../../third-party/ti-cc140xp-host/` | TI's driver, unmodified (`SOURCE.md` has the tag and license) |
| `hal_bare.c`, `hal_bare.h` | This repo's replacement for TI's two Zephyr-only files (`platform_spi.c`, `platform_util.c`): same state bits, same wait for the radio to pull MISO low after chip select, same 30 microsecond ready poll, no kernel, no heap, no 256-byte dummy buffer. It calls seven `hb_*` functions that a board supplies |
| `hooks_stub.c` | Empty `hb_*` functions so the image links for measuring. A real board replaces them with a few DriverLib calls each |
| `app_tx.c` | The job of TI's `ti_cc140xp_tx` sample without an RTOS: open, device config, front-end and Wi-SUN radio settings, one transmit command, one packet |
| `shim/` | Empty Zephyr log macros and a one-line stand-in for TI's wrapper header, so TI's files compile untouched |
| `build_fit.sh` | `build_fit.sh <cortex-m0plus or cortex-m33> <-Os> <out dir> [buffers]` builds with TI Arm Clang 5.1.1.LTS and prints flash and RAM |
| `link.cmd` | One flash region and one RAM region, so the sizes are the image's own |

## Result

| Piece | Cortex-M0+ | Cortex-M33 |
|---|---|---|
| Code | 7.5 kB | 7.5 kB |
| Constants, of which the Wi-SUN radio settings are 2.0 kB | 2.7 kB | 2.7 kB |
| Initialized data (copied from flash at start) | 48 B | 48 B |
| **Flash** | **10.4 kB** | **10.5 kB** |
| Driver state `trxHostObject` (fixed, set by TI's limits of 6 commands, 15 configs, 15 streams) | 1.3 kB | 1.3 kB |
| Buffer pool, TI default of 24 buffers of 156 B | 3.7 kB | 3.7 kB |
| This repo's HAL and app (HAL object 32 B, packet 80 B, command 48 B, flag 4 B) | 164 B | 164 B |
| Initialized data (counted in RAM below) | 48 B | 48 B |
| **RAM, default pool of 24** | **5.1 kB** | **5.1 kB** |
| RAM with 4 buffers | 2.1 kB | 2.1 kB |
| RAM with 2 buffers | 1.8 kB | 1.8 kB |

Stack is not counted (the driver calls nest a few levels; a measurement needs a run). The radio settings are stored in the radio's own non-volatile memory by `persistConfig`, so a host that is only provisioned once needs them in flash once, not on every boot.

What follows from it, against the real chips:

- **MSPM0C1104** (16 kB flash, 1 kB RAM): flash fits with 5.9 kB to spare; RAM does not fit even with 2 buffers (1.8 kB). The fixed 1.3 kB of driver state alone exceeds 1 kB. A patch to TI's limits is the only way, not run.
- **MSPM0L1306** (64 kB, 4 kB): fits with 4 buffers (2.1 kB, 1.9 kB left); the default 24 do not (5.1 kB).
- **Everything with 32 kB of RAM or more:** fits as shipped.
- **F2800137 (C28x):** builds and runs with the shim in `c28x/` (all files compile, 8.4 kB of flash and 8.7 kB of RAM), but sends different bytes than an Arm core, so it cannot talk to a radio. The driver and its wire format are written for 8-bit bytes; the C28x has none. Needs a field-by-field message packer.

The buffer count is hard-coded in TI's `buffer_manager.h` (24). The 4 and 2 buffer builds use a copy of the driver with only that number changed; whether the driver still works with so few is **unknown** until it runs (the host code reserves half the pool, at most 12, for outgoing blocks).
