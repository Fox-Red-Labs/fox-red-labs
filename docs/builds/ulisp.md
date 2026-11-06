# A Lisp prompt on a LaunchPad

Type a Lisp line on the LP-MSPM0G5187's USB serial port and see the answer. uLisp 4.9a runs unmodified through a 160-line Arduino shim.

The page: [site/project-ulisp.html](../../site/project-ulisp.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## uLisp on the LP-MSPM0G5187

Build with an Arm GCC, flash and check the prompt, then open the serial port.

Status: Works. Boards: g5187.

Folders this version uses (in a sparse clone, fetch them first, then read `ports/ulisp/AGENTS.md`):

```bash
git sparse-checkout add ports/ulisp third-party mspm0g5187 tools mac-setup
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM0G5187 LaunchPad development kit | 1 | runs the Lisp interpreter; the prompt is on its USB serial port |

### Before you start

- A Mac with an `arm-none-eabi-g++` 12 or newer (GNU Arm Embedded 14.3.1 was used) and the MSPM0 SDK 2.11.00.07 (`mac-setup/README.md`).
- The code: `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git`, then `git sparse-checkout add ports/ulisp third-party mspm0g5187 tools mac-setup`. Every command on this page runs from the repository folder.
- The board ships with a demo in its flash. To keep it, back it up first with the command in `mspm0g5187/AGENTS.md`.

### Steps

1. **Mac, terminal.** Put an Arm GCC on your path and build. It takes about 20 seconds.

   ```text
   ports/ulisp/build.sh
   ```

   You should see: `ports/ulisp/build-g5187/ulisp.elf` exists.

   If the compiler is not found, put its `bin` folder first on your `PATH`; any `arm-none-eabi-g++` 12 or newer works.

2. **Mac, terminal.** Flash the image and read three seconds of the console. The check passes when `uLisp` appears.

   ```text
   python3 tools/bench/run.py --board mspm0g5187 --image ports/ulisp/build-g5187/ulisp.elf --secs 3 --expect "uLisp"
   ```

   You should see: The tool prints a pass.

   If the tool cannot find the board, replug the USB cable and run it again.

3. **Mac, serial terminal.** Open the board's serial port at 115200 baud. The port name carries your probe serial. Press `~` to stop a running program.

   ```text
   screen /dev/cu.usbmodem<your probe serial>1 115200
   ```

   You should see: `uLisp 4.9a`, then a prompt showing the free workspace, such as `2800>`. `(+ 1 2)` answers `3`.

   If nothing appears, press Enter once.

### Expected result

A Lisp prompt on the serial port that evaluates what you type.

### What this shows

- An unmodified Lisp interpreter runs on a Cortex-M0+ with 32 kB of RAM, with 2,800 objects of workspace.
- `(tak 18 12 6)` takes 7.5 s at 80 MHz; the Arduino Zero (48 MHz) takes 11.4 s and the Raspberry Pi Pico (200 MHz) 2.6 s, from uLisp's published tables.

### Limits

- Analog read, sound, SPI and I2C are stubs. The LED path was driven by hand, and the repository's LED check read the pin toggling but printed FAIL for the run because it reads at the instant of reset.
- Ran on one board. The LP-MSPM0G3507 and LP-MSPM0G3519 images build and were not flashed.
- The workspace is 2,800 objects on the 32 kB parts, 9,000 on the G3519.

### Numbers and sources

| | Value | Source |
|---|---|---|
| tak 18 12 6 | 7.5 s at 80 MHz | `ports/ulisp/RESULTS-ROWS-wave6.csv` |
| fib 23 | 5.2 s | `ports/ulisp/RESULTS-ROWS-wave6.csv` |
| Full garbage collection | 1.07 ms | `ports/ulisp/README.md` |
