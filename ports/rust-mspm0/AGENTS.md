# Building Rust for the LP-MSPM0L1306: notes for an AI coding assistant

That the HAL is the community project embassy-mspm0, pulled from a pinned git commit because its crates.io name is an empty placeholder, and that the programs print on the UART because RTT output does not reach the repository's flash path. Terms: [vocab](../../docs/vocab.md). Each line below was run on one board on 2026-10-20 (Intel Mac, rustc 1.99.1, embassy commit `7915b85a`).

| Item | Fact | Evidence |
|---|---|---|
| Target | `thumbv6m-none-eabi` (Cortex-M0+), `opt-level = "s"`, `lto = true`, `panic = "abort"` | `ports/rust-mspm0/l1306/Cargo.toml` |
| Chip feature | `mspm0l1306rhb` on `embassy-mspm0` with `rt` and `time-driver-any` | same file |
| Memory | `memory.x` says 64 kB flash at 0, 4 kB RAM at 0x20000000 | `l1306/memory.x` |
| Linker flags | `-Tlink.x` and `-Tinterrupt_group.x` from `build.rs` | `l1306/build.rs` |
| UART | UART0, TX PA8, RX PA9, 115200, XDS110 back-channel with J16 and J17 on XDS | `l1306/src/bin/uart_hello.rs`, serial output read on the board |
| LED | LED1 PA0, lit when low: `set_inversion(true)` | `l1306/src/bin/blinky.rs`, pin check PASS |
| Build | `cargo build --release` in `l1306/`: 107 s cold, 3 s incremental | `RESULTS-ROWS-wave3.csv` in this folder |
| Flash | `python3 tools/bench/run.py --board mspm0l1306 --image <elf> --secs 4` takes the probe lock and loads the ELF | flash output on the board |
| Size | blinky 2.6 kB flash and 112 B RAM; serial hello 3.7 kB and 148 B | `RESULTS-ROWS-wave3.csv` in this folder |

Rules: do not add defmt or RTT to a program that the repository flashes through OpenOCD, since nothing reads it; export `PATH=$HOME/.cargo/bin:$PATH` first because the install did not edit the shell profile; use the `rust-mspm0/l1306` folder as the model for another MSPM0 chip by changing the chip feature, `memory.x` and the pins. Do not drive BoosterPack pins when one is stacked.
