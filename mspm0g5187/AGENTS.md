# AI assistant notes for the TI LP-MSPM0G5187 LaunchPad development kit

Its neural processing unit (NPU) ran the room-occupancy model of this repo 16.7 times faster than the CPU, that TI's wake-word example does not use the board's own microphone, and that several TI examples hang or read nonsense on this board. This file is the short list, each item from runs on one board unless it says otherwise.

Terms: [docs/vocab.md](../docs/vocab.md) and the [LP-MSPM0G5187 FAQ](FAQ.md) in the Fox Red Labs repo. In short: **NPU** is the chip's neural processing unit; **NNC** is TI's neural network compiler; **SysConfig** is TI's tool that writes a chip's pin and clock setup code; **DriverLib** is TI's C library for the chip's parts; **DSLite** is the command-line flasher inside UniFlash; **backchannel UART** is the serial port that comes through the debugger's USB cable. 1 kB = 1,024 bytes. Checked 2026-10-08 to 2026-10-10 on an Intel Mac, macOS 15.7, with MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, UniFlash 9.6.0 and xPack OpenOCD 0.12.0-7, on one LP-MSPM0G5187 Rev. A (probe firmware 3.0.0.38).

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths below in backticks starting `mspm0g5187/` or `data/` are in the Fox Red Labs repo; they are not in your copy, and links in this file only work inside that repo.

## The board

| Part | Fact |
|---|---|
| Chip | MSPM0G5187: Arm Cortex-M0+ at 80 MHz, 128 kB flash, 32 kB RAM, TinyEngine NPU (`mspm0g5187/README.md`) |
| LEDs | Red, green and blue; the blink output read with the debugger is PA17 (`data/results/mspm0g5187.csv`) |
| Serial | UART0 PA10 and PA11, loop-tested at header J4 pins 34 and 33 with the J101 RXD and TXD shunts off (`data/results/mspm0g5187.csv`) |
| Audio | Onboard microphone into a PCM1809 audio ADC, then I2S on PB10, PB11 and PB12; jumper J6 powers both (`mspm0g5187/FAQ.md`) |
| Headers | Pins fitted on J1 and J3 only; J2 and J4 are bare, so BoosterPack SPI and pins above 30 are not reachable (`mspm0g5187/FAQ.md`) |
| SPI | The 2x4 header J20 by the microSD slot carries one SPI port on PB6 to PB9 (`mspm0g5187/radio/README.md`) |
| I2C | Header pins 9 (PA9, clock) and 10 (PA8, data) (`mspm0g5187/FAQ.md`) |
| Debugger | XDS110 with EnergyTrace on the board, shipped on firmware 3.0.0.38 (`mspm0g5187/FAQ.md`) |

## Start here

| Step | This board |
|---|---|
| Tools | MSPM0 SDK 2.11.00.07, TI Arm Clang 5.1.1.LTS, SysConfig 1.26.3, UniFlash 9.6.0, xPack OpenOCD 0.12.0-7, probe firmware 3.0.0.38 (`mspm0g5187/README.md`) |
| Probe serial | `xdsdfu -e`, from CCStudio's `ccs_base/common/uscif/xds110/` folder |
| Back up | No `backup.py` recipe. Do it before the first write: TI ships no source for the 424 B factory program, so the backup is the only copy. Plan used on this board: DSLite `memory` reads of main flash (`-r 0x0,0x8000` words, 128 kB) and data flash (0x41D00000, 8 kB), each read twice and compared; copies are in `mspm0g5187/data/` |
| Build TI's blink | `cp -R <SDK>/examples/nortos/LP_MSPM0G5187/driverlib/gpio_toggle_output .` then `../mspm0g3507/mcu-bench/sdk-common/sdkmake.sh gpio_toggle_output/ticlang` (3.2 to 5.8 s) |
| Flash | `<UniFlash>/dslite.sh --mode flash -c MSPM0G5187_<serial>.ccxml -e -f -v -u gpio_toggle_output/ticlang/gpio_toggle_output.out` (4.7 to 6.4 s; the `.ccxml` is `mspm0g5187/MSPM0G5187_MG020001.ccxml` with your serial) |
| Check | `python3 tools/check/blink.py --board mspm0g5187 --probe <serial>` (PASS on TI's factory image, PA24 toggled) |
| Restore | `<UniFlash>/dslite.sh --mode flash -c <ccxml> -e -f -v backup-flash.bin,0x0` or OpenOCD `program backup-flash.bin 0x0 verify`; read back and compare |

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| "OK Kilby" never lights the LED | `wakeword_detection_ai_npu` reads an analog microphone on PB22, not the onboard one | Stack a BOOSTXL-EDUMKII, or use the I2S port in `mspm0g5187/wakeword/` (its microphone run needs the radio wires off header J20) | LED lights |
| The program waits forever on `tvmgen_default_finished` | A debugger `reset halt` then `resume` during an NPU run leaves the NPU stuck (5 of 6 times) | Flash with DSLite so the image starts itself, then halt and read without a reset | All 20 vectors match |
| `adc12_monitor_supply` reads codes of 540 to 674 that drift | Channel 15 is header pin PA15 here, not the supply divider | Do not use this example to read the supply on this board; the G3507 reads a steady 1,796 to 1,797 | Code is steady |
| Debugger shows `SYSCFG_DL_SYSCTL_CLK_init` and nothing happens after a good flash | Seven SDK examples wait for a low-frequency oscillator bit (0x800 at 0x400B0204) that never sets: `sysctl_hfxt_run`, `sysctl_mclk_syspll`, `ecg_anomaly_detection_ai` and `_npu`, three `motor_bearing_fault` | Skip those examples | Others run |
| `GCONV layer NOT offloaded to TI-NPU` | That layer stays on the CPU, so the 30k model gains only 2.3 times against NNC's CPU build | Accept it | Speedup near 2.3 |
| `scan=none` from the Sensors BoosterPack | The stack is not seated | Unplug USB, press the BoosterPack straight down so its pin 1 (3V3) sits in J1 pin 1, replug | `scan=40,44` |
| `tmp117=none` with the ribbon cable in | The TMP117 sits on the break-away strip, joined only by the flat cable; cause not found | Try a second cable or another LaunchPad | `0x48` answers |
| I2C scan shows every address from 08 to 77 after plugging the flat cable | The cable is in end for end, which connects the clock line to ground | Unplug USB, turn the cable around, push both ends in and press the latches flat | Scan returns `40,44` |
| `bmi=none` while humidity and light read | Cause not found; the BMI160 should answer at 0x69 | None found | Not fixed |
| CC1407P replies are all `04 02 f0 00` | The PICO wire (PB8) sat one socket off, in radio pin 16 instead of 15 | Move the wire to radio pin 15 | Replies are valid |
| Transmit ends with status `0x8A` after a change of protocol | The radio keeps the image of its first transmit since its last reset | Close and reopen TI's host driver (pulses RESET_N, about 50 ms) between protocols | Transmit goes |
| 200-byte packets return `TRX_Host_No_Resources`, 20-byte ones go | A stored but unpersisted config keeps holding buffer space (suspected) | Persist each config right after storing it | 200-byte packets go |
| A retargeted G3519 example fails with `undeclared identifier 'UART0'` or `'SPI1'` | The example folder holds TI's generated `ti_msp_dl_config.c` and `.h` for the G3519, which the source includes before SysConfig's own | Delete both from the example folder (not `ticlang/`) and build again | 79 of 122 `driverlib` examples build |
| Retargeted DMA example waits forever, `gDstData` correct | SysConfig moved the channel to 1; the handler tests channel 0 | Keep the `DMA_CH0` assignment in the `.syscfg` | `gChannel0InterruptTaken` 1 |
| DSLite "Connection to MSPM0 core failed" after a retargeted TI Drivers example | Its linker file is the G3519 map, so the stack top is outside the RAM | Wait-for-debug hold, then program the backup | Main and data flash equal the backup |
| MLPerf Tiny keyword-spotting model scores 3 of 20 on the board | The model takes float input (the pad comes before the scaling, so `skip_normalize` cannot drop it) and was fed int8 | Pass a float array; `output_int=true` still gives int8 logits | 18 of 20 as on the host |
| Backchannel serial shows nothing and the receive buffer holds the board's own start-up text | A loop wire joins PA10 and PA11 (J4 pins 34 and 33) | Take the wire off and fit the J101 shunts, or read results through the debugger | Host sees `m-init-done` |
| MicroPython pystone stops with `maximum recursion depth exceeded` on the minimal image | The minimal build's 1.5 kB stack minus a 512 B guard cannot hold the benchmark's calls | `make BOARD=g5187 VARIANT=min STACK_LEN=0x1000` (`mspm0g5187/bench/micropython/pystone-min-first-attempts.txt`) | Three pystone runs print |
| Arduino layer on this board has no heap and a 256 B stack | SysConfig writes `--stack_size=256` for this chip, not 512, so the build's linker-file edit missed it | `portability/arduino/build.sh` now matches any stack size (RAM of Blink 6,348 B once the reserve is in) | `bss` near 6.3 kB in `tiarmsize` |

Traps shared with other boards: TRAPS.md. Also: PB10 and PB11 carry both the radio's reset and interrupt and the audio converter, so do not run the microphone examples while the radio is wired (`mspm0g5187/radio/README.md`); `adc12_max_freq_dma` gives 0.547 Msps against the data sheet's 1.6, cause unknown (`mspm0g5187/FAQ.md`).

## What fits and what does not

| Program | Result | Source |
|---|---|---|
| TI blink | 528 B flash, 256 B RAM, all stack | `data/results/mspm0g5187.csv` |
| Four room-occupancy models, 500 to 30k parameters | all pass 20 of 20 vectors on CPU and NPU | `data/results/mspm0g5187.csv` |
| Radio sender image (CC1407P over SPI) | 20.4 kB flash, 5.4 kB RAM | `mspm0g5187/radio/README.md` |
| OpenThread router host (CC1354P10 build) | does not fit: 456.7 kB flash and 125.9 kB RAM needed, against 128 kB and 32 kB | `data/results/mspm0g5187.csv` |
| Zephyr | no board file in 4.4.2 | `data/results/mspm0g5187.csv` |

MLPerf Tiny keyword spotting (DS-CNN) runs on the NPU in 18.70 ms at 91.0 percent on the 4,890-clip test split (`mspm0g5187/mlperf-tiny/`); its CPU build and ResNet-8 do not fit the RAM. TI's quantization-aware training (two runs, 3 epochs each) did not beat its post-training quantization on this model: 88.9 against 89.7 percent on the first 1,000 clips on the board, so use `ptq` first (`docs/experiments/2026-11-03-mspm0g5187-mlperf-tiny-kws.md`).

NPU against CPU at 80 MHz: 2.12 ms against 35.27 ms on the 8k model (16.7 times), 3.0 times on the 500-parameter model, 1.9 times on the 30k model (`docs/experiments/2026-10-09-mspm0g5187-npu-vs-cpu.md`).

## What ran here

- One-command blink: `FRL_SYSCONFIG=<SysConfig 1.28.0 folder> python3 tools/frl/frl.py run --board mspm0g5187 --probe <serial> --pattern 150,150,150,900` builds, flashes and checks PA17 in 9.4 to 10.5 s warm (536 B image); `FRL-E021` means frl refuses a SysConfig newer than 1.26.3, and naming the folder with `FRL_SYSCONFIG` runs it unvalidated (`data/results/mspm0g5187.csv`, `time_to_blink` cli).
- Not run on this board: uLisp `analogread` and `save-image` (the port stubs the ADC and has no flash storage code; `data/results/mspm0g5187.csv`).
- Changed blink, proved by pin reads: half-period 272 ms at DELAY 8,000,000 and 135 ms at 4,000,000 (`data/results/mspm0g5187.csv`).
- CoreMark 182.79 per second at -O3 and 80 MHz (2.285 per MHz) (`mspm0g5187/README.md`).
- Interrupt latency 36 to 40 cycles, 7 more than the G3507's best (`data/results/mspm0g5187.csv`).
- Hardware AES-128 45.5 times and CRC-32 8.1 times faster than software on 1 kB (`mspm0g5187/RESULTS-mspm0g5187.md`).
- MicroPython 1.29.0 minimal image (72.9 kB flash) boots with `ports/micropython` `BOARD=g5187`; 680 to 794 pystones per second at 80 MHz (`data/results/mspm0g5187.csv`, `micropython_pystone`). Arduino layer: Blink and `boardcheck` run through `portability/arduino` `BOARD=mspm0g5187`, PA17 half periods 1,000 and 500 ms (`led_toggle_blink_check`).
- Example sweep: 174 of 203 build; 120 ran, 109 clean (`mspm0g5187/RESULTS-mspm0g5187.md`).
- CC1407P radio: 21,658 of 21,658 packets received over about 6 h 15 min (`mspm0g5187/radio/README.md`).
- Not run: power baseline and a spoken wake word heard through the onboard microphone (`data/results/mspm0g5187.csv`).

## Check a blink without looking

After a flash, prove the LED pin is toggling through the debugger: `python3 tools/check/blink.py --board mspm0g5187 --probe <your probe serial>` from the repository root (xPack OpenOCD; it reads the GPIO registers for 3 s and never writes). PASS means an LED pin enabled as an output changed state; it passed on this board on 2026-10-13 against TI's factory image. This is the check to paste into a report.

<!-- ports:start (tools/render_ports.py) -->
## Ports that ran here

Code written for other chips and tools, brought to this board. The files to copy, the command and what you see for each are in `docs/ports.md`; the method and a prompt to port your own are there too. Works ran and did what it should; Could work ran in part.

| Port | Status | Result | Evidence |
|---|---|---|---|
| Arduino display libraries on the LP-MSPM0G3507, and the core layer on the LP-MSPM0G5187 | Works | Arduino_GFX's graphics test draws on an ST7796S screen on the LP-MSPM0G3507, through a 649-line Arduino API layer under the unmodified libraries. | `portability/arduino/README.md` |
| Fixed-weight DSP (FIR, filter bank, DFT) on the LP-MSPM0G5187 | Works | A 64-tap FIR filter runs on the LP-MSPM0G5187's NPU (its neural-network accelerator), compiled with NNC, TI's neural-network compiler, 29.7 times faster than the same graph on the CPU, with byte-for-byte equal output. | `mspm0g5187/npu-dsp/README.md` |
| MicroPython 1.29.0 on the LP-MSPM0G3519, LP-MSPM0G3507 and LP-MSPM0G5187 | Works | A new MicroPython port boots on the LP-MSPM0G3519 with the MicroPython banner and drives a 3.5 inch touch screen from Python. | `ports/micropython/README.md` |
| Peanut-GB Game Boy emulator on MSPM0 and MSPM33 | Works | Peanut-GB plays Tobu Tobu Girl on the LP-MSPM33C321A with the Educational BoosterPack screen at the console's own speed. | `ports/peanut-gb/README.md` |
| Post-quantum key exchange and signatures on seven boards: LP-CC1354P10-1, LP-EM-CC2340R5, LP-AM243, LP-MSPM0G3519, LP-MSPM33C321A, LP-EM-CC2755P10 and LP-MSPM0G5187 | Works | ML-KEM-768 key exchange and ML-DSA-44 signatures pass NIST's known-answer vectors on seven boards, from a 36 kB Cortex-M0+ to an 800 MHz Cortex-R5F; the LP-CC1354P10-1 also agrees a key with a Mac. | `cc1354p10/pqc-join/README.md` |
| STM32 UART DMA receive with idle-line interrupt (MaJerle stm32-usart-uart-dma-rx-tx) on TI boards | Works | MaJerle's STM32 UART DMA example answers every test packet at 115200 baud on the LP-MSPM0G3519, G5187, L1306, C1104 and MSPM33C321A. | `docs/experiments/2026-10-19-stm32-usart-dma-port-matrix.md` |
| Sub-1 GHz CC1407P radio on an LP-MSPM0G5187, heard by an LP-CC1354P10-1 | Works | TI's CC1407P sub-1 GHz radio driver runs unchanged on the LP-MSPM0G5187, and an LP-CC1354P10-1 receives its packets: 21,658 of 21,658. | `mspm0g5187/radio/README.md` |
| uLisp Lisp interpreter on the LP-MSPM0G5187, LP-MSPM0G3507, LP-MSPM0G3519, LP-EM-CC2340R5, LP-EM-CC2755P10, LP-MSPM33C321A, LP-AM243 and LAUNCHXL-F2800137 | Works | uLisp, an Arduino sketch, gives a Lisp prompt on the serial port of the LP-MSPM0G5187, LP-EM-CC2340R5 and LP-EM-CC2755P10: 19 of 19 REPL (type-and-run loop) checks pass on each. | `ports/ulisp/README.md` |
| Arduboy2-compatible layer and three open-source games on MSPM0 and MSPM33 | Could work | Snake, TileWorld and Crate Confusion build unchanged against an Arduboy2-compatible layer for four kits, and Snake draws on the LP-MSPM33C321A's Educational BoosterPack screen. | `ports/arduboy/FIT.md` |
| BoosterPack drivers (Educational MKII, Sharp128, Audio, Sensors MKII, NPU DSP) on LaunchPad kits of other families | Could work | Portable C cores for BoosterPacks ran: the Educational pack on the LP-MSPM0L1306 and LP-MSPM33C321A, the Sharp128 panel on the LP-MSPM0C1104, the Audio pack on the LP-AM13E230 and the Sensors pack on the LP-MSPM0G5187. The Educational, Sharp128 and Audio builds also build for the LP-MSPM0G3519, and the Educational and Sharp128 builds for the LP-MSPM0G3507. | `portability/boosterpacks/edumkii/RUN-LOG.md` |
<!-- ports:end -->

## Where the evidence lives

Start at `mspm0g5187/README.md`, then `mspm0g5187/FAQ.md` for each problem, `mspm0g5187/RESULTS-mspm0g5187.md` for numbers, and `data/results/mspm0g5187.csv` for every row. If this file gets something wrong on your board, correct the FAQ first.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-24. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

How it locks up:

- A retargeted example whose linker file is the G3519 map puts the stack top outside RAM; the chip faults in a reset loop (DSLite: Connection to MSPM0 core failed) (`mspm0g5187/FAQ.md`, 2026-10-24)

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| sweep/results-g3519-port/tools/recover.sh with recover_halt_g5187.tcl (wait-for-debug hold), then program the backup (twice; main and data flash read back equal to the backup) | remote | proven | `mspm0g5187/sweep/results-g3519-port/tools/recover.sh`, `mspm0g5187/FAQ.md`, 2026-10-24 |

Restore: OpenOCD prog.sh: program backup-flash.bin 0x0 verify. Regions: main and data flash. (`mspm0g5187/FAQ.md`)

<!-- recovery:end -->
