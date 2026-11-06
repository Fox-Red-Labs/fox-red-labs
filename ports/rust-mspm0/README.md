# Rust on the LP-MSPM0L1306

Install Rust with rustup, add the `thumbv6m-none-eabi` target, and build the two programs in `l1306/` with `cargo build --release`. The blinky is 2.6 kB of flash and 112 bytes of static RAM; the serial hello is 3.7 kB and 148 bytes. Both ran on the board on 2026-10-20 (checked on an Intel Mac, rustc 1.99.1, embassy commit `7915b85a`, one LP-MSPM0L1306).

Terms: [vocab](../../docs/vocab.md), [LP-MSPM0L1306 FAQ](../../mspm0l1306/FAQ.md). **Embassy** is an open-source Rust framework for small chips, and **embassy-mspm0** is its hardware layer for MSPM0 chips. It is a community project (embassy-rs, MIT OR Apache-2.0), not a TI product, and TI does not support it. **UART** is the serial port. 1 kB = 1,024 bytes.

## Ten-minute path

From nothing to a blinking LED takes about 2.7 minutes of machine time on an Intel Mac (rustup 50 s, first build with the downloads 107 s, flash 2.6 s). Disk: about 590 MB for Rust itself, 12 MB for the cargo tools, 121 MB of downloaded crates and 110 MB for one build folder.

| Step | Where | Command |
|---|---|---|
| 1. Check disk, then install Rust | terminal | `curl -fsSL https://sh.rustup.rs -o rustup.sh && sh rustup.sh -y --profile minimal -t thumbv6m-none-eabi --no-modify-path`, then `export PATH=$HOME/.cargo/bin:$PATH` |
| 2. Build both programs | terminal | `cd ports/rust-mspm0/l1306 && cargo build --release` |
| 3. Flash the blinky | terminal | `python3 tools/bench/run.py --board mspm0l1306 --image ports/rust-mspm0/l1306/target/thumbv6m-none-eabi/release/blinky --secs 1` |
| 4. Flash the serial hello and read it | terminal | the same command with `uart_hello`, `--secs 4 --expect "Hello from Rust on MSPM0L1306"` |
| 5. Check the LED pin | terminal | `python3 tools/check/blink.py --board mspm0l1306 --probe <serial>` |

LED1 (red, PA0) blinks once a second. The serial port is the XDS110 back-channel at 115200 baud with jumpers J16 and J17 on XDS, as shipped.

The tool that flashed it is the repository's OpenOCD path, which loads the program file (ELF) directly. probe-rs, the usual Rust flasher, was not installed and not needed. The Embassy examples use defmt over RTT (debug text read through the probe), which this path cannot show, so these programs print on the UART instead.

## What was measured

| Program | Flash | Static RAM | C equivalent in this repo |
|---|--:|--:|---|
| Rust blinky (`blinky`) | 2.6 kB | 112 B | TI blink 440 B, 256 B (all stack) |
| Rust serial hello (`uart_hello`) | 3.7 kB | 148 B | message-hello 1.9 kB, 322 B |

Flash is code plus initial data from the linker size report (TI Arm Clang `tiarmsize`); static RAM is data plus bss. The stack is the rest of the 4 kB; the C numbers count a 256 B stack in bss, the Rust numbers do not, so compare with that in mind. Build time: 107 s with every download and compile from nothing, 3 s after a one-line change. Flash and verify: 2.6 to 2.9 s. The size is small because Rust's size-optimized build with link-time optimization drops everything unused.

Pin check on 2026-10-20: PA0 enabled as an output, 12 samples over 3 s, 5 changes, PASS. The pin check is the evidence for the LED; no photo of the LED was taken.

## Limits

Only GPIO, the timer driver and the blocking UART were run. The ADC, I2C and the async UART were not. The crate on crates.io under the name `embassy-mspm0` is an empty reserved 0.0.0, so the build takes the HAL from a pinned git commit. Results rows: `RESULTS-ROWS-wave3.csv`. The lessons are below.

## Porting lessons

- The crates.io name is empty: depend on the embassy git repository at one commit, and commit `Cargo.lock`.
- The chip feature is the part name without temperature or packaging letters, plus the package: `mspm0l1306rhb`.
- Link `interrupt_group.x` from `build.rs` when the `rt` feature is on, or the link fails on missing interrupt symbols.
- LED1 turns on when PA0 is low; `set_inversion(true)` on the pin makes "high" mean lit.
- Do not wait on RTT output when the flash path is OpenOCD through the lock; print on the UART.
