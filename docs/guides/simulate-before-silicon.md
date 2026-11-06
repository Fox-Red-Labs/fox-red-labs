# How do you simulate the TMDS62LEVM (AM62L) before the board arrives? Renode

Renode boots the AM62L's Zephyr image on a Mac with no board and found one real defect (a translation-table count that was one short). It was evaluated once on 2026-10-18. This page covers what Renode is, how to use it, what broke, the helper files and a log of its predictions to score on the board.

Terms: [vocab](../vocab.md). 1 KB = 1,024 bytes.

## What Renode is, in plain words

Renode is an open-source machine simulator from Antmicro (MIT license, public since 2017, on GitHub). You assemble a machine from parts: a CPU model (its Cortex-A53 and Cortex-M cores run real instructions, built on a translator derived from QEMU), an interrupt controller, timers, UARTs, memories, and whatever peripheral models exist. A platform file (`.repl`, plain text) says which part sits at which address. Then you load your real firmware binary and run it, unmodified. It is the open-source cousin of the commercial virtual development kits (Synopsys Virtualizer, Arm Fixed Virtual Platforms) that let software start before silicon. It is used as the test machine behind Zephyr's board dashboard (about 300 boards), for OpenTitan, and by several space programs.

Two properties decide what it is good for:

- **Functional, not cycle-accurate.** It tells you whether the code is right (boots, maps memory, takes interrupts, prints), not how fast it runs or how much power it takes. CoreMark in Renode is a meaningless number.
- **Only modeled parts behave.** A register with no model is a "tag": reads return 0, writes are logged. Code that only needs generic Arm parts runs fine; code that waits on a vendor clock controller stalls or sees zeros.

## What is modeled and what is not

A simulator like this covers a chip only as far as someone has written models for it. Renode has models for the generic Arm parts (Cortex-A and Cortex-M cores, GIC, generic timer, 16550 and PL011 UARTs) and for the vendor blocks that its maintainers, a vendor or a user have contributed. For the AM62L the generic parts are modeled and the chip-specific blocks (pin controller, clock and power control, the firmware mailbox, the flash controller, the display block) are not, in Renode or in QEMU or gem5. A Zephyr image that touches only the generic parts runs today; one that waits on a chip-specific block stalls or reads zeros.

A missing model is ordinary work. A peripheral model is a C# class with a register map written from the chip's public technical reference manual, the same source a driver is written from; Antmicro's documentation shows the pattern, and a model good enough to share goes upstream under Renode's license. Three things supplement the TRM when writing one: forum posts for reset defaults and behavior the manual leaves implicit, measurements from a board of the same family already on the bench (the TDA4 is a K3 part and shares the pin-controller and UART register layouts, so a K3 model can be checked against it before the AM62L arrives), and the firmware's own expectations read from its driver source. The rules here are the repo's usual ones: public documentation only, no vendor mark on the model.

## How it was used here

1. Build the Zephyr image for the board as usual. Zephyr writes the flattened device tree it built against (`build/zephyr/zephyr.dts`).
2. `dts2repl build/zephyr/zephyr.dts -o platform.repl` (Antmicro's converter; `pip install git+https://github.com/antmicro/dts2repl.git`). It emits a platform with every node it can map and a tag for every node it cannot. Check the generic timer frequency; the tool guesses 62.5 MHz.
3. `renode --console --disable-xwt -e "include @run.resc"` with a script that loads the platform, loads the ELF, sends UART0 to a file and runs a few simulated seconds.
4. When it stops, `cpu0 LogFunctionNames true` gives the function trace that says where.

On the AM62L this took one afternoon from nothing installed and found one defect before any hardware: the kernel's translation-table budget was one short for the pin-control driver. The run and its numbers: [AM62L Renode experiment](../experiments/2026-10-18-am62l-renode-presilicon.md).

## Helper files

| File | What it is |
|---|---|
| `am62l-evm/zephyr/renode/am62l-sram.repl` | The AM62L as Renode sees it, from dts2repl, with the timer frequency corrected and a header saying what is a tag |
| `am62l-evm/zephyr/renode/run.resc` | Renode script: platform, ELF, UART0 to a file, run 3 s, quit |
| `am62l-evm/zephyr/renode/run.sh` | One command: runs the script and prints the UART capture |
| `am62l-evm/zephyr/setup.sh`, `rebuild_sram.sh` | The image builds the platform was made from |

## Lessons

- **Install.** On a Mac, `brew trust renode/tap` before `brew install renode/tap/renode`, or the install refuses the tap and stops; it looks like it continued. Renode 1.17.0 runs natively on Apple silicon. dts2repl goes into the Zephyr Python environment.
- **dts2repl writes nothing to standard output.** Use `-o`.
- **Fix the timer.** The generated platform's generic timer runs at 62.5 MHz; the AM62L's runs at 200 MHz. Wrong is harmless for a hello world and wrong for anything timed.
- **A silent UART does not mean a dead UART.** The first run printed nothing because the kernel panicked before the console driver initialized. The function trace, not the UART, said why.
- **Renode scripts are not shell.** `echo` cannot run `cat`; capture the UART to a file and read it from the runner script.
- **No `timeout` on macOS.** Put the run length in the script (`emulation RunFor "3"`) and `quit`.

## The prediction log

Every claim the simulator made about the AM62L, to be scored on the board. A prediction the board confirms raises the value of the next simulation; one it contradicts marks a missing model.

| # | Renode said (2026-10-18, image `build-sram-all`, 124 KB) | Board said | Verdict |
|---|---|---|---|
| P1 | The image enters at EL3, drops to EL2 then EL1, and the MMU comes up with 8 translation tables | | |
| P2 | 6 translation tables are not enough: the kernel panics when the pin-control driver maps 0x4084000 | | |
| P3 | The GICv3 at 0x01800000 and 0x01840000 initializes and the timer interrupt arrives | | |
| P4 | The 16550 at 0x02800000 with the K3 4-byte register stride takes the Zephyr driver's writes and the hello line comes out | | |
| P5 | Nothing: UART0's clock and power state without TIFS. Renode has no clock tree, so a silent board here is not a simulator failure | | |
| P6 | Nothing: whether the boot ROM loads the image as its phase-1 payload | | |
| P7 | Nothing: boot time, CoreMark, power | | |

Fill the board column when the board arrives; a row that disagrees becomes an entry under Lessons and, if a model would have caught it, a candidate model to write.

## Where it fits beside the other tools

Renode does not replace the bench. It is the step between "the image links" and "the board is here". On the works-with grid it ran on the AM62L only. Next candidates are the Cortex-M boards Zephyr's dashboard already runs in Renode (dts2repl covers them) and any new part whose reference manual is public before its board ships.
