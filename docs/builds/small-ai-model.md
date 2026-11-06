# Time a tiny AI model that counts people in a room

Four room-occupancy networks, 636 to 33k weights, run as plain C on the LP-MSPM0G3507; every answer is checked and timed in clock cycles.

The page: [site/project-small-ai-model.html](../../site/project-small-ai-model.html). This file is its text. Terms: [vocabulary](../vocab.md).

<!-- generated from site/data/projects.json by tools/render_build_md.py; edit the data, not this file -->

## Four occupancy models in plain C on the LP-MSPM0G3507

The demonstrated build: one board on USB, no wiring, the test windows are stored in the image.

Status: Works. Boards: g3507.

Folders this version uses (in a sparse clone, fetch them first, then read `mspm0g3507/AGENTS.md`):

```bash
git sparse-checkout add mspm0g3507 ports/bitnetmcu
```

### Parts

| Part | Quantity | What it is for |
|---|---|---|
| LP-MSPM0G3507 LaunchPad development kit | 1 | the board: MSPM0G3507, 32 kB of RAM, 128 kB of flash. |
| Micro-USB data cable | 1 | power and programming |

### Before you start

- A Mac with CCStudio 21.0.1, which brings TI Arm Clang 5.1.1.LTS. Also the MSPM0 SDK 2.11.00.07, UniFlash 9.6.0, xPack OpenOCD 0.12.0 and Python 3. Checked on an Intel Mac on macOS 15.7.9 only.
- Your LaunchPad kit's XDS110 serial number. Step 2 shows where to read it.
- The code is in this repository. Clone it with `git clone --depth 1 --filter=blob:none --sparse https://github.com/Fox-Red-Labs/fox-red-labs.git` and fetch the folders listed for this version. This page calls the folder you clone it into `<repository folder>`. The four trained models, their plain-C source, prebuilt libraries and test windows are already in `mspm0g3507/mcu-bench/edgeai/uci-room-occupancy`; you do not train anything.

### Steps

1. **Mac, terminal.** Install the tools once, all free. Get CCStudio 21.0.1 from [TI's CCStudio download page](https://www.ti.com/tool/download/CCSTUDIO) (1.9 GB download, 8.1 GB installed, free myTI login). It brings TI Arm Clang 5.1.1.LTS. Get the MSPM0 SDK 2.11.00.07 from [TI's MSPM0 SDK download page](https://www.ti.com/tool/download/MSPM0-SDK) (0.4 GB download, 2.2 GB installed). Get UniFlash 9.6.0 from [TI's UniFlash page](https://www.ti.com/tool/download/UNIFLASH) (0.3 GB download, 1.9 GB installed). Get xPack OpenOCD 0.12.0 from [its GitHub release page](https://github.com/xpack-dev-tools/openocd-xpack/releases/tag/v0.12.0-7) (2 MB; the file `xpack-openocd-0.12.0-7-darwin-x64.tar.gz` on an Intel Mac, `xpack-openocd-0.12.0-7-darwin-arm64.tar.gz` on Apple Silicon), as the macOS x64 archive, and unpack it in your home folder. The generated pin and clock setup the build needs is already in the repository, so SysConfig is not needed for this page.

   You should see: `/Applications/ti/ccs2101/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/bin/tiarmclang --version` prints 5.1.1, and the folders `mspm0_sdk_2_11_00_07` and `uniflash_9.6.0` are under `/Applications/ti`.

   `build.sh` has the SDK and compiler paths written in its first lines (`SDK=` and `CC=`); edit them if yours live elsewhere.

2. **Bench, then terminal.** Plug the LaunchPad in with a micro-USB data cable and read its XDS110 serial number. No wiring is needed.

   ```text
   /Applications/ti/ccs2101/ccs/ccs_base/common/uscif/xds110/xdsdfu -e
   ```

   You should see: `Serial Num:` followed by your probe's serial number .

   If nothing is listed, the cable is charge-only; swap it. With several boards plugged in, unplug the others.

3. **Mac, terminal.** Put your serial number where the bench board's is written. Two tracked files name <probe serial>. The probe check in `sdk-common/flash.sh` refuses to run unless that probe is on the USB bus. The two `Value=` lines of the connection file `sdk-common/MSPM0G3507_<probe serial>.ccxml` select the probe by serial number. Replace `YOURSERIAL` in both commands with the number from step 2 (`<probe serial>` below is the bench board's, the text the commands search for). Do not rename the file: `flash.sh` opens it by that name.

   ```text
   sed -i '' 's/Serial Num: \*<probe serial>/Serial Num: *YOURSERIAL/' mspm0g3507/mcu-bench/sdk-common/flash.sh
   sed -i '' 's/Value="<probe serial>"/Value="YOURSERIAL"/g' mspm0g3507/mcu-bench/sdk-common/MSPM0G3507_<probe serial>.ccxml
   ```

   You should see: `grep -n YOURSERIAL` on those two files shows the changed lines, with your real number in place of the word. The filename `MSPM0G3507_<probe serial>.ccxml` is unchanged.

   If you skip this the flash stops at `Probe <probe serial> missing`; that is the check working. The error text keeps the old number (`Probe <probe serial> missing`) even after the change, so compare the probe your message should name with step 2.

4. **Mac, terminal.** Build the program that runs all four models on the board's 20 stored test windows, with the models compiled at -O3. The program is compiled by TI Arm Clang 5.1.1 at -O2. The model code is the prebuilt archives, built earlier by TI Arm Clang 4.0.4.LTS. It runs at 80 MHz, using the pin and clock setup of TI's `sysctl_mclk_syspll` example.

   ```text
   mspm0g3507/mcu-bench/edgeai-sdk/occupancy/build.sh O3
   ```

   You should see: The last lines are the linker map's FLASH and SRAM rows: 0x1dbb0 bytes of flash used, which is 121,776 or 119 kB, and 0x334c of RAM, which is 12.8 kB. `build-O3/occupancy.out` exists. Use `Os` instead of `O3` to link the smaller, slower -Os libraries, at 95.3 kB of flash.

   If a path is wrong the script stops at the first compiler call and names the missing file. The SDK's own makefiles point at folders that do not exist, which this script avoids by not using them.

5. **Mac, terminal.** Flash the image and let it run. UniFlash's default erase for this chip is "Erase MAIN memory only", so the board's boot configuration area is not touched.

   ```text
   mspm0g3507/mcu-bench/sdk-common/flash.sh mspm0g3507/mcu-bench/edgeai-sdk/occupancy/build-O3/occupancy.out
   ```

   You should see: UniFlash loads and then verifies: `Program verification successful for` the `occupancy.out` path. The program starts running.

   If the script stops at `Probe ... missing`, the serial number in step 3 does not match a probe on the USB bus; compare it with the step 2 output.

6. **Mac, terminal.** Wait for the whole run, then read the results table the program leaves in RAM. It is 35 words at `0x202020bc`, the symbol `occ` in `build-O3/occupancy.map`. The read script halts the core after the wait; connecting OpenOCD restarts the program, so the wait must cover the whole run. `OCD` is the folder you unpacked OpenOCD into; `OCD_CFG` is the tracked connection file the digit build uses for this same board; `PROBE` is your serial. This command line is assembled from the saved run's scripts. It was not copied from a log; see the recovery line.

   ```text
   export PROBE=YOURSERIAL
   export OCD=$HOME/xpack-openocd-0.12.0-7
   export OCD_CFG=$PWD/ports/bitnetmcu/openocd-mspm0.cfg
   python3 mspm0g3507/mcu-bench/sdk-common/readwords.py 0x202020bc 35 70000
   ```

   You should see: Three or four lines starting `0x202020bc:` followed by a list of 35 numbers, the first being 213909505, which is 0x0CC00001: the program finished.

   If the first number is not 213909505 the program had not finished, or OpenOCD restarted it: raise the wait. A short wait caught the restarted run in its first milliseconds on this bench. The 30k-weight model at -Os needs about 30 s, and reads with 45 and 70 s waits worked. The default connection file in `ocd.sh` is the one in the Zephyr project folder, and the saved run used it. `ports/bitnetmcu/openocd-mspm0.cfg` has the same three lines. That pairing with this read script was not run as written here.

7. **Mac, terminal.** Read the numbers. There are three header words: done flag, clock in Hz, and timer read cost in cycles. Each model then has eight words, in the order of the 636, 1,748, 8.5k and 33k-weight models. They are mismatches and windows with the right class. Then come the smallest and largest of the best-of-5 cycle counts, for the network alone, for the float-to-int8 input step, and for both together. Divide the cycles of the last pair by 80,000,000 for seconds.

   You should see: Header `213909505, 80000000, 8`. Every model reads `0, 20`: no mismatches, 20 of 20. The fastest complete answer, input step plus network, is 320k cycles or 4.00 ms, 723,842 or 9.05 ms, 2,821,806 or 35.27 ms, and 10.4M cycles or 129.40 ms. The input step alone is about 100k cycles, or 1.25 ms, for every model.

   If a model's first number is not 0 or the second is below 20, an answer differed from the stored one. Reflash, and check you built the same library flavour you are reading. The -Os build is slower: 4.68, 10.87, 39.94 and 199.45 ms.

### Expected result

All four models answer 20 of 20 stored test windows correctly on the board, in 4.00 ms for 636 weights to 129.40 ms for 33k weights per answer at -O3 and 80 MHz.

### What this shows

- A room-occupancy network runs on a Cortex-M0+ with no FPU and no AI hardware, as plain C, and gives the same class and the same four output values as the quantized model run in onnxruntime. This holds on all 20 stored windows.
- Answer time grows with model size: 11,328 to 610,560 multiply-adds took 4.00 to 129.40 ms, about 16 to 20 cycles per multiply-add for the network alone at -O3.
- All four models, with their stored test windows, fit one 118.9 kB image in 128 kB of flash and 12.8 kB of RAM.

### Limits

- Anything about live sensing. The windows are stored in the image; no light, sound, CO2 or motion sensor is read.
- That the model is accurate in your room. The models score 85.4 to 90.9% on a held-out part of one room's four days of data. Balanced accuracy is 60.5 to 74.7%. They tell empty from occupied on 98.6 to 100% of windows but struggle to tell 2 people from 3. With whole days held out, the 8.5k-weight model scored 87.7% overall but 40.2% balanced: 159 of 170 three-person windows came out as empty.
- That bigger is better: the 8.5k-weight model beat the 33k-weight one on accuracy.
- Stack use, or current. Neither was measured for these programs.
- No build steps are given here: Build steps for these models are not published, so the page lists the results only. The numbers come from runs on the boards named in the grid.
- The timings are the fastest of five runs on the best of 20 windows, the table's min. They come from one board and one run of the program.
- Quantizing the float input costs about 100k cycles (1.25 ms) every time, which is 30% of the smallest model's total; feeding int8 directly saves it.
- TI's own Neural Network Compiler produced code 6 to 14% slower than this plain C at -Os on the three smaller models and 20% faster on the largest. This page does not run it.
- This G3507 has no neural-network accelerator; the TinyEngine accelerator is on the MSPM0G5187.
- Checked on an Intel Mac only.

### Numbers and sources

| | Value | Source |
|---|---|---|
| Models | 636, 1,748, 8.5k and 33k weights and biases (after batch norm is folded in); 11,328, 33,856, 157,824 and 610,560 multiply-adds; int8 weights and activations, int32 accumulators; trained on UCI Room Occupancy Estimation (id 864, CC BY 4.0) |  |
| Accuracy on the held-out windows (int8) | 88.4%, 88.5%, 90.9%, 85.4%; empty versus occupied 100.0%, 99.8%, 100.0%, 98.6%; the C code matches the quantized ONNX model exactly on all 1,001 test windows |  |
| Cycles per answer at -O3, input step plus network (fastest) | 320,386; 723,842; 2,821,806; 10,352,016 at 80 MHz (clock read back as 80.85 to 80.90 MHz against the Mac's clock, 1.1% high from OpenOCD latency) |  |
| Cycles per answer at -Os | 374,048; 869,848; 3,195,175; 15,956,266 (4.68, 10.87, 39.94, 199.45 ms) |  |
| Model flash and RAM | -Os 1.7 kB / 3.1 kB / 10.1 kB / 34.6 kB of flash and 1.1 kB / 1.1 kB / 2.1 kB / 4.1 kB of RAM; -O3 6.3 kB / 9.5 kB / 16.6 kB / 40.7 kB of flash, same RAM |  |
| Toolchain | Harness: TI Arm Clang 5.1.1.LTS -O2, MSPM0 SDK 2.11.00.07; model libraries: TI Arm Clang 4.0.4.LTS; flash with UniFlash 9.6.0; results read with xPack OpenOCD 0.12.0; cycles counted by SysTick (the Cortex-M0+ has no cycle counter), read cost 8 cycles |  |
| Image size | -O3 image: 118.9 kB of flash, 12.8 kB of RAM; -Os image: 95.3 kB of flash |  |
