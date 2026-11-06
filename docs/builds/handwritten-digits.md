# Draw a digit. Watch the microcontroller recognize it

Draw one digit on the touch screen and see the board's prediction after you lift your finger. The model runs locally on the microcontroller.

The page: [site/project-handwritten-digits.html](../../site/project-handwritten-digits.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Draw on a touch screen

The demonstrated build: screen, 11 wires, one board.

Status: Works. Boards: g3507.

Folders this version uses (in a sparse clone, fetch them first, then read `mspm0g3507/AGENTS.md`):

```bash
git sparse-checkout add mspm0g3507 third-party/bitnetmcu ports/bitnetmcu
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM0G3507 LaunchPad development kit | 1 | the board |
| Waveshare 3.5 inch capacitive touch LCD (ST7796S screen, FT6336U touch) | 1 | the screen and touch panel |
| ELEGOO jumper wires (120-piece pack) | 12 female-to-female as tested (11 are required); 13 with the optional MISO wire | the wires from the screen to the LaunchPad header |
| Micro-USB data cable | 1 | power and programming |

### Before you start

- A Mac with CCStudio 21.0.1, the MSPM0 SDK 2.11.00.07 and xPack OpenOCD 0.12.0, installed in step 1. CCStudio brings TI Arm Clang 5.1.1. Checked on an Intel Mac on macOS 15.7 only.
- A micro-USB data cable, and your LaunchPad kit's XDS110 serial number. Step 5 shows where to read it. The flash script refuses to run without it, on purpose.
- About 12 GB free for the tools. No Python and no AI tool are needed for this version; the scripts use bash, sed and grep, which macOS has. The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`.

### Wiring

The 40-pin edge is four 10-pin headers. Hold the board with the USB connector at the top: J1 (pins 1 to 10) and J3 (21 to 30) are the two left rows, J4 (31 to 40) and J2 (11 to 20) the two right rows. Pin 1 of J1 and pin 21 of J3 are the top pins of the left rows, nearest the USB connector; J4 and J2 count the other way, so pin 40 and pin 20 are at the top. A diagram drawn on TI's board drawing, checked against the SysConfig pin output the demo is built with (`mspm0g3507/display-test/gen80/ti_msp_dl_config.h`). Signal and supply are both 3.3 V: the module's level translator takes the LaunchPad kit's 3V3, and nothing is driven at 5 V. No jumper on the LaunchPad moves.

| Module pin | Board pin | What it is | Needed |
|---|---|---|---|
| VCC pin (not the 3V3 pin) | J1 pin 1 (3V3) | power, 3.3 V only | required |
| GND | J2 pin 20 (GND) | ground | required |
| SCLK | J1 pin 7 (PB9) | screen clock, SPI1 | required |
| MOSI | J2 pin 15 (PB8) | screen data in, SPI1 | required |
| LCD_CS | J2 pin 13 (PB6) | screen chip select | required |
| LCD_DC | J4 pin 39 (PB1) | command or data | required |
| LCD_RST | J4 pin 40 (PB4) | screen reset | required |
| LCD_BL | J4 pin 38 (PA28) | backlight on | required |
| TP_SDA | J1 pin 10 (PB3) | touch data, software I2C | required |
| TP_SCL | J1 pin 9 (PB2) | touch clock, software I2C | required |
| TP_INT | J2 pin 11 (PB16) | touch interrupt. The program reads it once at start for a diagnostic value and finds touches by polling the touch chip, so the demo does not need it. Wired in the tested setup. | optional |
| TP_RST | J2 pin 12 (PB0) | touch reset | required |
| MISO | J2 pin 14 (PB7) | screen read-back; only to diagnose a dark screen | optional |
| 3V3 pin and SD_CS | not connected | the module's own 3V3 pin, which stays empty, and its SD card select | leave off |

### Steps

1. **Mac, terminal.** Install the tools once, all free. CCStudio 21.0.1 comes from [TI's CCStudio download page](https://www.ti.com/tool/download/CCSTUDIO) (1.9 GB download, 8.1 GB installed, free myTI login); it brings TI Arm Clang 5.1.1. The MSPM0 SDK 2.11.00.07 comes from [TI's MSPM0 SDK download page](https://www.ti.com/tool/download/MSPM0-SDK) (0.4 GB download, 2.2 GB installed). xPack OpenOCD 0.12.0 comes from [its GitHub release page](https://github.com/xpack-dev-tools/openocd-xpack/releases/tag/v0.12.0-7) (2 MB; the file `xpack-openocd-0.12.0-7-darwin-x64.tar.gz` on an Intel Mac, `xpack-openocd-0.12.0-7-darwin-arm64.tar.gz` on Apple Silicon); get the macOS x64 archive and unpack it in your home folder. The OpenOCD connection file is in this repository, so nothing else is needed. Checked on an Intel Mac; on Apple Silicon CCStudio runs under Rosetta 2 by TI's own notes. Then clone this repository into `<repository folder>`. To fetch only the board folder, use the sparse-checkout line in its README. The model and upstream code are already in the tree under `third-party/bitnetmcu`: GPL-3.0, commit 0715bfc, unchanged.

   You should see: `/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin/tiarmclang --version` prints 5.1.1, and `ls ports/bitnetmcu/touch` shows `digit_touch.c`, `build.sh`, `flash.sh`, `lcd_touch.h`, `prepare.h` and `font5x7.h`.

   If `third-party/bitnetmcu` is empty, the clone did not bring the third-party folder; it is part of this repository and is not a submodule, so re-clone. If your tools live somewhere else, step 4 and step 6 show how to point the scripts at them.

2. **Bench.** With the LaunchPad unplugged, run the wires in the table above from the display to the header, power from the 3V3 pin only. Leave the display's own 3V3 and SD_CS pins empty. The 13th wire, MISO, is only for diagnosing a dark screen.

   You should see: Every wire sits on the header row the picture shows; nothing is plugged into the display's 3V3 or SD_CS.

   If you are not sure a wire is on the right pin, count from the header's pin 1 and check it against the table before plugging in. Pin 1 is the corner nearest the USB connector, J1.

3. **Mac, terminal.** Tell the build where your SDK and compiler are, check them, and build. The two paths below are the installer defaults on this Mac; change them to where yours are. Keep the quotes: they let a path with spaces work. The build uses the tracked SysConfig output for an 80 MHz CPU and 20 MHz SPI.

   ```text
   # replace <repository folder> with the folder you cloned this repository into
   export SDK="/Applications/ti/mspm0_sdk_2_11_00_07"
   export CC="/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS"
   cd <repository folder>/ports/bitnetmcu/touch
   ./build.sh --check
   ./build.sh
   ```

   You should see: `check OK: every tool and setting this script needs was found`, then two lines, FLASH and SRAM, then `built .../out/digit_touch.out`. On 2026-10-10 the image was 42.3 kB of flash and 5.0 kB of RAM. The linker map says 42.3 kB and 5 kB, and 4 kB of that RAM is the stack setting.

   If the check prints `Not ready` it names each missing item and how to fix it; set `SDK` or `CC` to the folder it says and run it again. Downloads: [MSPM0 SDK](https://www.ti.com/tool/download/MSPM0-SDK), [CCStudio](https://www.ti.com/tool/download/CCSTUDIO).

4. **Bench, then terminal.** Plug the LaunchPad in with a micro-USB data cable and list its serial ports. Your probe id is the text after `usbmodem` without the last digit: `usbmodemAB1234561` and `usbmodemAB1234564` are the two ports of probe `AB123456`. Flashing needs this id and will not guess it, so it can never flash a board that is not yours.

   ```text
   ls /dev/cu.usbmodem*
   ```

   You should see: Two `cu.usbmodem` lines per board, with the same text and last digits 1 and 4. The backlight may come on but the screen stays dark until step 5.

   If nothing appears, the cable is charge-only; swap it for a data cable. If several boards are plugged in, unplug the others, or run `system_profiler SPUSBDataType | grep -A6 XDS110` and read the `Serial Number` line of the board you mean.

5. **Mac, terminal.** Flash and start it. Set `OCD` to the folder you unpacked OpenOCD into, the one that holds `bin/openocd`, and `PROBE` to your id from step 4; `<probe serial>` stands for your id, so replace it, brackets included. The check runs first and never opens the probe. To find yours, run `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e` if you have CCStudio (it prints `Serial Num:`), else `system_profiler SPUSBDataType | grep -A3 XDS110`.

   ```text
   export OCD="$HOME/xpack-openocd-0.12.0-7"
   export PROBE=<probe serial>
   cd <repository folder>/ports/bitnetmcu/touch
   ./flash.sh --check
   ./flash.sh
   ```

   You should see: `check OK`, then `Verified OK` and `running: the screen should say DRAW A DIGIT`. The script retries up to three times, because on this board OpenOCD's verify fails on about every other load and passes on the next. **Unknown (suspected: Tool or docs).** The retry handles it.

   If `PROBE: not set` appears, step 4 shows how to read it. If all three tries fail with `Verify Failed`, see [the OpenOCD CRC mismatch entry](../../site/board-mspm0g3507.html#openocd-reports-a-crc-mismatch-after-program-file-verify-but); if it says no device, replug and check the probe id against `ls /dev/cu.usbmodem*`.

6. **Bench.** Look at the screen.

   You should see: "DRAW A DIGIT", a black box and a red CLEAR button, in portrait.

   If the screen is dark or shows static with every wire right, see [the screen stays dark entry](../../site/board-mspm0g3507.html#the-screen-stays-dark-or-shows-static-even-though-every-wire): the chip-select gap or a wire is the usual cause. If the backlight is off, LCD_BL on J4 pin 38 is the first wire to check.

7. **Bench.** Draw one digit, 0 to 9, inside the box with a finger, then lift the finger.

   You should see: About half a second after you lift, the answer and the time it took appear under the box. The inference takes 33 or 34 ms at 80 MHz.

   If nothing follows the finger, touch is not answering: [the touch chip at address 0x38 entry](../../site/board-mspm0g3507.html#the-touch-chip-does-not-answer-on-i2c1-at-address-0x38-is-it) has the two checks: the software I2C pins and TP_RST.

8. **Bench.** Touch the box again or tap CLEAR to try another digit.

   You should see: The box clears and the next digit is read the same way. It reads one digit at a time: "10" came out as 6 and "12" as 4 in the video, because the model knows single digits only.

   If a digit is read wrong, draw it larger and centered; the drawing is scaled and centered like the MNIST training digits before it is read.

### Expected result

A digit drawn with a finger is answered on screen about half a second after you lift the finger. The inference time, 34 ms at 80 MHz, appears beside it. The demo waits half a second on purpose before it reads the drawing. The 560 ms from lift to answer is computed from the wait, the 25 ms of preparation and the inference. It was not measured end to end.

### What this shows

- The 64-wide BitNetMCU model runs on the LP-MSPM0G3507 and reads the ten digits that ship with it, 10 of 10, through the same drawing path a finger uses. The self-test does this at power-up. The author reports 99.55% on MNIST for this model.
- Finger-drawn digits were read on camera: counted from the video, seven of the nine drawings were single digits and all seven were read right. No larger count exists.
- One inference takes 2.71M cycles on the demo's path, 34 ms at 80 MHz; turning the drawing into the model's 16 x 16 input takes another 25 ms.

### Limits

- How often it reads a stranger's handwriting right. The 10 stored digits are a port check and not an accuracy measurement. The only count of finger drawings is the seven in the video.
- The 99.55% is the author's figure on the MNIST test set. It is not measured here, and it is not on finger input.
- Pen-to-answer time. The demo waits half a second after the finger lifts, then spends 25 ms preparing and 34 ms inferring, so the answer appears about 560 ms after the lift. That figure is computed and was not measured end to end. The 34 ms is the model alone.
- Letters or two-digit numbers: the model was trained on single digits.
- That 20 MHz SPI is safe everywhere. The data sheet rates SPI1's standard-drive pins to 16 Mbit/s. The panel's allows about 15 MHz. The display test read back only 64 pixels of one row at 20 and 40 MHz. It worked here on 10 to 20 cm wires.
- Touch runs over software I2C on PB3 and PB2 because the chip's I2C block did not get an answer from the touch controller through the module. The translator is suspected and was not checked.
- Chip select needs a 10 microsecond gap between commands or the screen drops the second one; the driver has it, a rewritten driver must keep it.
- SPI runs at 20 MHz, over the 16 Mbit/s the data sheet gives these pins and the 15 MHz the panel allows. If you see wrong colors or noise, change the divider in `digit_touch.c` (`DL_SPI_setBitRateSerialClockDivider(LCD_SPI_INST, 1)`) from 1 to 2 for 13.3 MHz.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Model | BitNetMCU 64-wide CNN, 11.0 kB of weights, commit 0715bfc, unchanged | `ports/bitnetmcu/README.md` |
| Inference | 2.71M cycles on the demo's drawing path (34 ms at 80 MHz); the separate benchmark image measures 2,750,868 at 80 MHz with two flash wait states | `ports/bitnetmcu/README.md` |
| Preprocessing | 1.97M cycles, 25 ms at 80 MHz, in software floating point: the digit's larger side scaled to 20 of 28 pixels, center of mass centered, shrunk to 16 x 16, normalized like the training data | `ports/bitnetmcu/touch/prepare.h` |
| Clock and SPI | 80 MHz CPU from the SysPLL (display80.syscfg), SPI1 at 20 MHz, 16-bit frames | `mspm0g3507/display-test/README.md` |
| Compiler and SDK | TI Arm Clang 5.1.1.LTS at -O3, MSPM0 SDK 2.11.00.07, SysConfig 1.26.3, xPack OpenOCD 0.12.0 | `docs/experiments/2026-10-07-mspm0-bitnetmcu.md` |
| Screen driver | ST7796S over SPI and FT6336U over bit-banged I2C, copied from the display test with the I2C delay scaled to the clock | `mspm0g3507/display-test/README.md` |
| Image | 43,304 bytes of flash (42.3 kB) and 5,128 bytes of RAM (5.0 kB, of which 4,096 is the stack setting), from the linker map of the 2026-10-10 build |  |

## Run stored test digits without a screen

The port check: the ten digits that ship with the model, read on the board and compared with the Mac. No wiring, no touch input.

Status: Works. Boards: g3507, l1306, cc1354p10, f2800137, c1104.

Folders this version uses (in a sparse clone, fetch them first, then read `mspm0g3507/AGENTS.md`):

```bash
git sparse-checkout add mspm0g3507 ports/bitnetmcu mspm0c1104
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM0G3507 LaunchPad development kit | 1 | the board these steps use |
| Micro-USB data cable | 1 | power and programming |

### Before you start

- A Mac with CCStudio 21.0.1, the MSPM0 SDK 2.11.00.07 and xPack OpenOCD 0.12.0, installed in step 1. CCStudio brings TI Arm Clang 5.1.1, and its `tiarmnm` is the symbol tool the run script uses. Intel Mac, macOS 15.7 only. Python 3 reads the answers back. macOS offers it with the command line tools.
- A LaunchPad, either the LP-MSPM0G3507 or, for steps 5 and 6, the LP-MSPM0L1306. You also need a micro-USB data cable and that board's XDS110 serial number. Step 3 shows where to read it. The run script refuses to start without it, on purpose.
- The other boards use their own folders and tools: `ports/bitnetmcu/c2000/` with TI C2000 compiler 25.11.1, and `mspm0c1104/`. Each README has its commands. The CC1354P10 row is a stored result. The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`.

### Steps

1. **Mac, terminal.** Install the tools once, all free. CCStudio 21.0.1 comes from [TI's CCStudio download page](https://www.ti.com/tool/download/CCSTUDIO) (1.9 GB download, 8.1 GB installed, free myTI login); it brings TI Arm Clang 5.1.1. The MSPM0 SDK 2.11.00.07 comes from [TI's MSPM0 SDK download page](https://www.ti.com/tool/download/MSPM0-SDK) (0.4 GB download, 2.2 GB installed). xPack OpenOCD 0.12.0 comes from [its GitHub release page](https://github.com/xpack-dev-tools/openocd-xpack/releases/tag/v0.12.0-7) (2 MB; the file `xpack-openocd-0.12.0-7-darwin-x64.tar.gz` on an Intel Mac, `xpack-openocd-0.12.0-7-darwin-arm64.tar.gz` on Apple Silicon); get the macOS x64 archive and unpack it in your home folder. The OpenOCD connection file is in this repository, so nothing else is needed. Checked on an Intel Mac; on Apple Silicon CCStudio runs under Rosetta 2 by TI's own notes. Then clone this repository into `<repository folder>`. To fetch only the board folder, use the sparse-checkout line in its README.

   You should see: `/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin/tiarmclang --version` prints 5.1.1, and `ls ports/bitnetmcu/mspm0` shows `build.sh`, `run.sh` and `main.c`.

   If your copies live elsewhere, steps 2 to 6 set `SDK`, `CC` and `OCD` to your paths; each script prints what it could not find.

2. **Mac, terminal.** Build the 64-wide model for the LP-MSPM0G3507 at 80 MHz. The two paths are the installer defaults on this Mac; change them to where yours are. Keep the quotes: they let a path with spaces work. The check runs first and builds nothing.

   ```text
   # replace <repository folder> with the folder you cloned this repository into
   export SDK="/Applications/ti/mspm0_sdk_2_11_00_07"
   export CC="/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS"
   cd <repository folder>/ports/bitnetmcu/mspm0
   ./build.sh --check g3507 80
   ./build.sh g3507 cnn_64 O3 80
   ```

   You should see: `check OK`, then the FLASH and SRAM lines of the linker map and a new `build-g3507-cnn_64-O3-80` folder. The lines read 23.0 kB of flash and 3.3 kB of RAM.

   If the check prints `Not ready` it names each missing item and how to fix it. Downloads: [MSPM0 SDK](https://www.ti.com/tool/download/MSPM0-SDK), [CCStudio](https://www.ti.com/tool/download/CCSTUDIO). Clock 80 exists for the G3507 only; the L1306 uses 32.

3. **Bench, then terminal.** Plug the LaunchPad in with a micro-USB data cable and list its serial ports. Your probe id is the text after `usbmodem` without the last digit: `usbmodemAB1234561` and `usbmodemAB1234564` are the two ports of probe `AB123456`. Flashing needs this id and will not guess it, so it can never flash a board that is not yours. No wiring is needed: the test digits are in the image.

   ```text
   ls /dev/cu.usbmodem*
   ```

   You should see: Two `cu.usbmodem` lines per board, with the same text and last digits 1 and 4.

   If nothing appears, the cable is charge-only; swap it for a data cable. If several boards are plugged in, unplug the others, or run `system_profiler SPUSBDataType | grep -A6 XDS110` and read the `Serial Number` line of the board you mean.

4. **Mac, terminal.** Run it on the LP-MSPM0G3507. Set `OCD` to the folder you unpacked OpenOCD into, the one that holds `bin/openocd`, and `PROBE` to your id from step 3; `<probe serial>` stands for your id, so replace it, brackets included. The script flashes, fills RAM with a pattern, lets the board classify the ten digits for 20 seconds, then reads the answers, cycles and stack depth back over the debugger. To find yours, run `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e` if you have CCStudio (it prints `Serial Num:`), else `system_profiler SPUSBDataType | grep -A3 XDS110`.

   ```text
   export CC="/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS"   # run.sh finds its symbol tool, tiarmnm, inside this folder
   export OCD="$HOME/xpack-openocd-0.12.0-7"
   export PROBE=<probe serial>
   cd <repository folder>/ports/bitnetmcu/mspm0
   ./run.sh --check build-g3507-cnn_64-O3-80
   ./run.sh build-g3507-cnn_64-O3-80
   ```

   You should see: `check OK`, `magic 0xb17e0001 (done), clock 80000000 Hz, right 10 of 10`, `answers 3209069277`, `cycles 2748503 to 2754797 (mean 2,750,868; 34.39 ms)` and `stack: 3072 bytes reserved, deepest use 2288 bytes`.

   If `right` is under 10 or the magic says NOT DONE, the image did not run to the end: reflash, and check you built clock 80 for this board. `run.sh` already retries OpenOCD's verify three times; for the known verify failure see [the OpenOCD CRC mismatch entry](../../site/board-mspm0g3507.html#openocd-reports-a-crc-mismatch-after-program-file-verify-but). If it prints `PROBE: not set`, step 3 shows how to read it.

5. **Mac, terminal.** Only with an LP-MSPM0L1306: build the same model for it at 32 MHz. `STACK=3600` reserves 3.5 kB of the board's 4 kB of RAM for the stack, as the saved result did. The model's deepest use is 2.2 kB. The default 3,072 also links, so the number only sets the headroom shown in step 6.

   ```text
   # replace <repository folder> with the folder you cloned this repository into
   export SDK="/Applications/ti/mspm0_sdk_2_11_00_07"
   export CC="/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS"
   cd <repository folder>/ports/bitnetmcu/mspm0
   ./build.sh --check l1306 32
   STACK=3600 ./build.sh l1306 cnn_64 O3 32
   ```

   You should see: `check OK`, then the FLASH and SRAM lines and a new `build-l1306-cnn_64-O3-32` folder. The lines read 21.4 kB of flash and 3.6 kB of the board's 4 kB of RAM, which leaves 396 bytes.

   If the link fails with a stack or RAM error, the model is too big for 4 kB: use `cnn_64`, or a smaller model (`cnn_16small`, `12k`, `1k`). `STACK` is the stack size in bytes.

6. **Mac, terminal.** Run it on the LP-MSPM0L1306. Use the id of this board, read as in step 3; `<probe serial>` stands for your id, so replace it, brackets included. Before OpenOCD starts the script asks TI's `xdsdfu` tool, which is inside CCStudio, whether that probe is plugged in; set `XDSDFU` to its path if yours is elsewhere. To find yours, run `/Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e` if you have CCStudio (it prints `Serial Num:`), else `system_profiler SPUSBDataType | grep -A3 XDS110`.

   ```text
   export CC="/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS"   # run.sh finds its symbol tool, tiarmnm, inside this folder
   export OCD="$HOME/xpack-openocd-0.12.0-7"
   export PROBE=<probe serial>
   cd <repository folder>/ports/bitnetmcu/mspm0
   ./run.sh --check build-l1306-cnn_64-O3-32
   ./run.sh build-l1306-cnn_64-O3-32
   ```

   You should see: `check OK`, `magic 0xb17e0001 (done), clock 32000000 Hz, right 10 of 10`, `answers 3209069277`, `cycles 2646151 to 2653623 (mean 2,649,196; 82.79 ms)` and `stack: 3600 bytes reserved, deepest use 2288 bytes`.

   If it prints `Probe ... missing`, that id is not on the USB bus: replug, and compare with `ls /dev/cu.usbmodem*`. If `right` is under 10 or the magic says NOT DONE, reflash and check you built clock 32 with `STACK=3600`. Otherwise the stack line reads 3 kB reserved.

### Expected result

`right 10 of 10` and the answers `3209069277`, the same ten the Mac gives from `host/host_main.c`.

### What this shows

- Every board that ran it gave the same ten answers as the Mac for the model it ran, and the board table says which model. So the port is correct.
- Cycle counts per digit are comparable with the author's CH32V002 figure: fewer at 32 MHz on the MSPM0 boards, 0.5% more at 80 MHz with two flash wait states.

### Limits

- Nothing about handwriting: no touch input, no finger. Passing ten stored vectors is a functional check of the port and is no accuracy estimate.
- A passing stored run on a board does not mean the touch build works there; that needs the screen driver on that board.
- On the LP-MSPM0L1306 only the 64-wide model was run, with a 3.5 kB stack leaving 396 bytes of RAM; the SRAM-loop variant does not fit.
- The author's cycle counts were not rerun on his chip; the comparison is chip plus compiler.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Cycles per digit, 64-wide model | G3507 2,640,383 at 32 MHz and 2,750,868 at 80 MHz; L1306 2,649,196 at 32 MHz; CC1354P10 about 1.13M at 48 MHz; F2800137 2,551,750 from RAM and 2,897,585 from flash at 120 MHz; author's CH32V002 2,736,250 | `ports/bitnetmcu/README.md` |
| Stack | 2.2 kB deepest use on the MSPM0 boards (RAM painted before the run) |  |
| Models | 64-wide CNN 11.0 kB (99.55%), 16-wide 3.2 kB (98.92%), 12k fully connected 12.3 kB (99.02%), 1k 1.1 kB (94.22%); accuracies are the author's on MNIST | `ports/bitnetmcu/README.md` |
| C28x patches | `1L << shift` in ReLUNorm, signed test-data bytes, a 2-word alignment on one shared buffer; the arithmetic is unchanged |  |
