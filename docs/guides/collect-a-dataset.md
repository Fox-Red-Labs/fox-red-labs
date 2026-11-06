# How do I collect my own sensor data to train a tiny AI model for a TI MSPM0 LaunchPad development kit (LP-MSPM0G3507, LP-MSPM0G5187)?

Have the board send one sensor reading at a time to your Mac over its USB serial port, at a fixed rate, and press a key on the Mac to say what you are doing while it records; the result is a CSV file with a label on every line. Then check the file, train a small neural network on it, shrink it to 8-bit numbers with TI's tools, and test it on the board with recordings it never saw before you trust it live. This repo has each piece: `portability/dataset-stream/` (board side), `tools/dataset/capture.py` (record and label), `tools/dataset/check.py` (rate, gaps, balance, plot, TI's dataset layout) and `tools/dataset/train.py` (train, quantize, test vectors).

**What has and has not run (2026-10-08).** The Mac tools pass their host tests on a saved log, and the training script ran end to end on made-up data. **The board-side capture has not run**: the BP-BASSENSORSMKII sensor BoosterPack plug-in module has not arrived, so no number on this page comes from a real sensor. Compiling a model from this dataset with TI's Neural Network Compiler has not run either (the same compiler route has run for the four room-occupancy models of this repo), and TI Edge AI Studio was not run. **There is no board program you can flash**: the board file that connects this code to a LaunchPad is written when the sensor arrives.

Terms: [docs/vocab.md](../vocab.md) (CNN, NNC, inference, golden vectors). Also used here: **quantize**, store the model's numbers as 8-bit integers so it fits and runs fast on a small chip; **ONNX**, a standard file format for trained models, which TI's compiler reads; **logits**, the raw score the model gives each class (the highest wins); **test vectors**, saved inputs with known right answers; **BoosterPack plug-in module**, TI's name for a board that plugs on top of a LaunchPad development kit; **TI Edge AI Studio**, TI's point-and-click tool for training models; **CSV**, a text file with one row per line and commas between values; **I2C**, the two-wire bus the sensor talks on; **UART** and **baud**, the serial link to the Mac and its speed in bits a second; **virtual environment**, a private folder of Python packages for one project; **NPU**, the G5187's neural-network accelerator; **QAT** (quantization-aware training), retraining a model while simulating 8-bit numbers so it loses little accuracy when stored that way; **confusion matrix**, a table of which class the model said against the true class; **ring buffer**, a fixed-size buffer that overwrites its oldest entry. Problems carry a cause tag: **Tool or docs** (a tool or a page is missing or wrong), **TI example default** (a TI example's setting causes it), **Setup or handling** (how the board or the Mac is set up), **Chip limit** (the silicon cannot do it), **Unknown** (not found; "suspected" names the lead). Board errors and fixes: [mspm0g3507/FAQ.md](../../mspm0g3507/FAQ.md). 1 kB = 1,024 bytes; 1 GB = 1,024 MB. Machine: Intel MacBook (Core i7-8750H), macOS 15.7.9. TI pages read 2026-10-08.

Contents: [what a dataset is](#what-is-a-dataset-in-plain-words) · [what you need](#what-do-i-need) · [stream](#step-1-how-does-the-board-stream-samples) · [record and label](#step-2-how-do-i-record-and-label-on-the-mac) · [how much](#how-many-samples-per-class-and-how-do-i-know-i-have-enough) · [clean and check](#step-3-how-do-i-clean-and-check-the-data) · [train](#step-4-how-do-i-train-a-small-1-d-cnn) · [compile](#step-5-how-do-i-quantize-and-compile-it-with-tis-nnc) · [test on the board](#step-6-how-do-i-check-it-on-the-board-before-trusting-it-live) · [washer, sounds and TI's datasets](#what-about-washer-vibration-sound-clips-or-datasets-ti-already-ships) · [questions](#questions) · [next](#what-is-next)

## What is a dataset, in plain words?

| Word | Meaning here |
|---|---|
| Sample | One reading of every channel at one moment: one line of the CSV file |
| Channel | One measured quantity, such as `ax` (acceleration along x) |
| Class | One thing the model should recognize, such as `idle`, `shake` or `circle` |
| Label | The class written next to each sample, set by the key you pressed |
| Window | A fixed run of samples the model sees at once. 128 samples at 100 samples a second is 1.28 s |
| Stride | How far the next window starts after the last one. Stride 32 with window 128 means windows overlap by three quarters |
| Segment | One unbroken stretch of one label in one recording |
| Train, validation and test split | Data to learn from, data to tune on, and data kept back to grade the model. Split by whole segment, never by window: neighboring windows share most of their samples, so a test window would be a near copy of a training window and the score would look better than the model is |

Balance matters because a model learns to guess the biggest class. In the public room-occupancy dataset earlier models of this repo used, 8,228 of 10.1k rows are "empty room", so a model that always says "empty" scores 81% and is useless. Aim for about the same number of windows in each class; `check.py` warns above 1.5 to 1.

## What do I need?

| Item | Version or part | Note |
|---|---|---|
| LaunchPad development kit | LP-MSPM0G3507 or LP-MSPM0G5187 | The G5187 also has an NPU and a microphone |
| Motion sensor | BP-BASSENSORSMKII (sold at TI.com): Bosch BMI160 accelerometer and gyroscope at I2C address 0x69 | Or the BOOSTXL-EDUMKII's analog accelerometer (Kionix KXTC9-2050) |
| Recording | Python 3 (3.14.7 here), pyserial 3.5, matplotlib 3.11.2 for the optional plot | In a virtual environment, about 126 MB with both |
| Training | Python 3.12.13, torch 2.2.2, numpy 1.26.4, onnx 1.22.0, onnxruntime 1.23.2, TI `tinyml_torchmodelopt` 1.5.0 | About 1.0 GB. torch 2.2.2 is the last PyTorch build for Intel Macs |
| Compiling | TI NNC 2.1.2 and TI Arm Clang 5.1.1.LTS | On GitHub's Linux machines, an Apple Silicon Mac, Linux or Windows; not on an Intel Mac |

Set up the recording environment (any folder works; this one is in your home folder):

```bash
python3 -m venv ~/ds-venv
```

```text
~/ds-venv/bin/pip install pyserial==3.5 matplotlib==3.11.2
```

**Run every command from the top folder of this repository.** Commands that need no extra packages work with any `python3`; the ones that plot or record use `~/ds-venv/bin/python`.

## Step 1: how does the board stream samples?

The board takes one reading at a fixed rate and prints it as one line: time in milliseconds, then each channel in raw sensor counts.

```text
# dataset-stream v1 rate_hz=100 sensor=bmi160
t_ms,ax,ay,az,gx,gy,gz
0,12,-40,16390,3,-2,1
10,15,-38,16401,2,-1,0
```

The code is `portability/dataset-stream/core/` (`ds_stream.c` for timing and formatting, `bmi160.c` for the sensor), with the loop and the per-board pins in [`portability/dataset-stream/README.md`](../../portability/dataset-stream/README.md) and [`boards/README.md`](../../portability/dataset-stream/boards/README.md). Three choices in it matter for the data:

- **Fixed rate from a timer, not a delay.** `t_ms` is the time the sample was due, so if the loop runs late the skipped samples show as a gap of whole periods, and the board also prints `# overrun <n>`.
- **Raw counts.** At the BMI160's +-2 g range, 16,384 counts is 1 g. Flat on the desk, `az` reads about 16,384.
- **Room on the serial link.** At 115200 baud the link carries 11.5 kB a second. A six-channel line is at most 52 bytes, so 221 lines a second is the worst-case limit; typical lines are 23 to 40 bytes, and 100 Hz leaves room either way.

The core is set to 100 Hz and six channels (accelerometer and gyroscope); another rate, range or channel set means changing `ds_init`'s rate and the register values in `bmi160.c`. The Bosch data sheet (BST-BMI160-DS000-09, register 0x7E) says the BMI160 starts in suspend mode after power-up and measures nothing until two commands wake it. `bmi160_init` sends them. The host test checks that sequence against a fake sensor:

```bash
portability/dataset-stream/test/run.sh
```

It prints 29 `ok` lines and `OK`. Not run on a board.

## Step 2: how do I record and label on the Mac?

Plug the LaunchPad in and find its serial port. The debug probe shows two; use the one whose name ends in 1.

```bash
ls /dev/cu.usbmodem*
```

For the LP-MSPM0G3507 used here the two are `/dev/cu.usbmodemMG3500011` and `/dev/cu.usbmodemMG3500014` (recorded in [mspm0g3507/FAQ.md](../../mspm0g3507/FAQ.md)). The middle part, `MG350001`, is the probe's serial number. If nothing is listed, try another cable: some USB cables carry power only.

Record, with your probe's serial in place of `MG350001`. Each `--keys` item is one letter, `=`, and a class name.

```text
~/ds-venv/bin/python tools/dataset/capture.py --serial MG350001 --keys i=idle,s=shake,c=circle --out gestures-1.csv
```

While it records, press `i` and hold the board still, press `s` and shake it, press `c` and draw circles in the air. Space marks rows "none" (kept in the file, ignored later); `q` stops. A status line shows the label, the row count and the measured rate every second. When it stops it prints the rate from the board's clock, any gaps, and the rows per class.

This is the summary from the host test (made-up sine waves, not a sensor):

```text
wrote 3200 rows to fx.csv
board clock: 100.00 Hz (median step 10 ms), 32.02 s, largest step off the median 0 ms
gaps: 1 (3 samples missing); board reported 3 skipped; time resets: 0; unreadable lines: 2
  circle           900 rows      9.0 s
  idle             900 rows      9.0 s
  none             500 rows      5.0 s
  shake            900 rows      9.0 s
```

Record each class in at least three recordings, on different days or with the board held differently. A recording is one file. The last recording of each class becomes the test set, so the model is graded on a recording it never saw.

## How many samples per class, and how do I know I have enough?

Start with three sessions of two minutes per class. At 100 Hz with 128-sample windows and stride 32, a two-minute segment gives about 367 windows after trimming, so about 1,100 per class. The windows overlap, so the number that really counts is sessions, not windows.

Two checks tell you if you have enough:

1. `check.py` warns below 100 windows or below 3 segments per class (both can be changed with `--min-windows` and `--min-segments`).
2. Train once with two recordings per class and once with three. If the score on the held-out recording still rises when you add a recording, record more recordings. If validation is high but the held-out recording is much lower, your recordings are too alike: vary the grip, the speed and the place.

## Step 3: how do I clean and check the data?

```text
~/ds-venv/bin/python tools/dataset/check.py gestures-1.csv gestures-2.csv gestures-3.csv --plot gestures.png
```

What it does and why:

- **Drops the first second and the last half second of every segment** (`--trim-start 1.0`, `--trim-end 0.5`). Those are the moments you were pressing the key or starting to move.
- **Checks the sample rate** from the board's clock in each file, and warns if files differ by more than 2% or more than 1% of samples are missing.
- **Counts windows per class** and how they fall into train, validation and test.
- **Warns on imbalance** above 1.5 to 1, on a board reset mid-recording (time going backwards), and on values stuck at the sensor's limit (-32768 or 32767), which means the range is too small for the motion.
- **Prints a per-class summary** (mean, spread, min, max per channel). If two classes look the same here, the model will struggle too.
- **Plots** the first segment of each class with `--plot` (needs matplotlib). Look at it: shaking should look like a fast wave, circles like two slow waves a quarter turn apart, idle like flat lines.

It exits with 0 when every check passes and 1 with a `WARN` line for each problem. On the test log with the default 128-sample windows it warns that 3 windows per class is too few, which is right for 9 seconds per class.

| Warning | What to do |
|---|---|
| `fewer than 100: record more` | Record more of that class |
| `segment(s), fewer than 3` | Record that class in more separate sessions |
| `classes unbalanced` | Record more of the smallest class |
| `samples missing (more than 1%)` | Lower the rate, send fewer channels, or close other programs using the port |
| `clock went backwards` | The board reset; split the file at the reset or record again |
| `values at the sensor's limit` | Raise the range: in `bmi160_init` (`core/bmi160.c`), change the write to register 0x41 (the sensor's `ACC_RANGE` register) from 0x03 to 0x05 (+-4 g, 8,192 counts per g), record again, and pass `--scale 8192` to `train.py` |

Run the tool tests (no board):

```bash
python3 tools/dataset/test_dataset.py
```

It prints 22 `ok` lines and `OK`.

## Step 4: how do I train a small 1-D CNN?

Training needs a second Python environment, about 1.0 GB, built once by the steps in `mspm0g3507/mcu-bench/edgeai/README.md` (Python 3.12, because torch 2.2.2, the last PyTorch build for Intel Macs, does not install on 3.14). The command below runs it from there, `~/tinyml-bench/venv`. Recording uses `~/ds-venv`; training uses this one.

The recipe is the one the room-occupancy models of this repo used (`mspm0g3507/mcu-bench/edgeai/README.md`): train in PyTorch, then retrain with TI's NPU-style 8-bit quantization (`TINPUTinyMLQATFxModule` from TI's `tinyml_torchmodelopt`), export to ONNX, fold constants. That page has the environment setup, including why `tinyml_torchmodelopt` goes in with `--no-deps` on an Intel Mac and the two small fixes `train.py` carries for torch 2.2.2.

```text
~/tinyml-bench/venv/bin/python tools/dataset/train.py gestures-model gestures-1.csv gestures-2.csv gestures-3.csv --channels ax,ay,az --gen-c
```

The model (`--model tiny`, the default) is batch norm on the input (it rescales each channel to a steady range), a 1-D convolution of 8 filters 5 samples wide, a max pool, a convolution of 16 filters 3 wide, global average pooling and one fully connected layer. Input shape `(1, 3, 128, 1)`, TI's layout of channels, time, width 1. Raw counts are divided by `--scale` (16,384, so inputs are in g). It follows TI's NPU rules: channel counts multiples of 4, kernels 3 or 5, at least 16 inputs to the last layer.

What it writes to the output folder:

| File | What it is |
|---|---|
| `model_tinpu_int8.onnx` | The quantized model, the file TI's NNC and Edge AI Studio take |
| `result.json` | Float and 8-bit accuracy on the held-out segments, confusion matrix, which segments went where |
| `test_vectors.h` | Held-out windows (5 per class), their true class, and the class and logits onnxruntime gives |
| `labels.h` | Class names in output order and the input scale |
| `ds_model.c`, `.h` (with `--gen-c`) | A plain integer C version of the same model, checked against onnxruntime on every test window |

Measured on the made-up data (three sessions of 30 seconds per class, 258 test windows): the whole script took 10.9 s, the plain C gave the same logits as onnxruntime on 258 of 258 windows, and the model is 39,984 multiply-adds (one multiplication and one addition each) per inference with 795 bytes of weights. Built for a Cortex-M0+ with TI Arm Clang 4.0.4.LTS at `-Os` (the copy installed on this Mac; the compile job in step 5 uses 5.1.1.LTS) the plain C is 1.7 kB of code and 4.4 kB of RAM. Its accuracy was 100%, which says only that sine waves are easy; your real number comes from your real recordings.

## Step 5: how do I quantize and compile it with TI's NNC?

The model is already quantized by step 4. NNC turns the ONNX file into `tvmgen_default.h` and `mod.a` for the chip. TI publishes NNC 2.1.2 for Linux x86-64, Windows and Apple Silicon Macs, all on Python 3.10 (wheel index read 2026-10-08). There is no Intel Mac build. **Tool or docs.** On an Intel Mac, use one of these routes.

**Route 1: GitHub's Linux machines.** [`mspm0g3507/.github/workflows/nnc-compile.yml`](https://github.com/Fox-Red-Labs/fox-red-labs/blob/main/mspm0g3507/.github/workflows/nnc-compile.yml) installs NNC 2.1.2 and TI Arm Clang 5.1.1.LTS, checks both downloads against checksums, and compiles for the LP-MSPM0G3507 CPU and the LP-MSPM0G5187 NPU (and a C2000 target); its first run, for the four occupancy models, took 2 minutes 9 seconds. GitHub runs a workflow only from `.github/workflows/` at the top of a repository, so copy the file there in a private repository of your own. Put `train.py`'s output folder (say `gestures-model/`, which holds `model_tinpu_int8.onnx`) in the repository, set `MODELS_DIR` to the folder above it, and replace the four occupancy names in the workflow's loops with `gestures-model`. Push, then start it and download the result (`<you>/<repo>` is your repository):

```text
gh workflow run nnc-compile.yml -R <you>/<repo>
```

```text
gh run download -R <you>/<repo> --name nnc-occupancy
```

The compile line it runs for the G3507 CPU is below; for the G5187 NPU the target is `"c, ti-npu"`. `$OPTS` is the TI Arm Clang options line set in the workflow, from TI's NNC user's guide (MSPM0 section).

```text
tvmc compile --target="c, ti-npu type=soft" --target-c-mcpu=cortex-m0plus model_tinpu_int8.onnx -o mod.a --cross-compiler="tiarmclang" --cross-compiler-options="$OPTS"
```

**Route 2: an Apple Silicon Mac, Linux or Windows with Python 3.10.** Install NNC from TI's package index, then run the same `tvmc` line with TI Arm Clang on the `PATH`:

```bash
pip3 install --extra-index-url https://software-dl.ti.com/mctools/esd/tvm/mcu ti_mcu_nnc
```

**Route 3: TI Edge AI Studio, point and click.** Edge AI Studio for Microcontrollers is version 1.7.0 (released 2026-07-11); the desktop app is for Windows and Linux only, so on a Mac use the cloud version at <https://dev.ti.com/edgeaistudio/> with a myTI account. TI's MSPM0 guide ("Deploying Machine Learning Models on TI MSPM0 Microcontrollers Using CCStudio Edge AI Studio GUI Tools", SDK docs, read 2026-10-08) gives these steps: create a project with the "Generic Time Series" task, import your data on the Capture tab, pick the device (MSPM0G5187 for the NPU, or MSPM0G3507) on the Model Selection tab, train on the Train tab, compile, and download the compiled model from the Deploy tab. The download holds an `artifacts` folder and a `golden_vectors` folder with `test_vector.c` and `user_input_config.h`.

Edge AI Studio and TI's command-line flow (tinyml-tensorlab) read a fixed folder layout: `classes/<class>/*.csv` plus optional `annotations/instances_train_list.txt`, `instances_val_list.txt` and `instances_test_list.txt` (Tiny ML Tensorlab user guide 1.4.0, "Classification Dataset Format"). `check.py` writes it, split by segment like `train.py`:

```bash
python3 tools/dataset/check.py gestures-1.csv gestures-2.csv gestures-3.csv --export-ti gestures-ti
```

TI's guide says a zip must have `classes/` at its top level, with no extra folder above it:

```bash
cd gestures-ti && zip -r ../gestures-ti.zip classes annotations && cd ..
```

Each exported file starts with a `time_ms` column; TI's loader drops any column whose name contains "time".

Two things on TI's pages to know. **Tool or docs.** TI's MSPM0 guide tells you to import `examples/nortos/LP_MSPM0G5187/edgeAI/time_series_data_capture/`; in MSPM0 SDK 2.11.00.07 the folder is `timeseries_data_capture`, and it has no `artifacts` folder or `test_vector.c` to overwrite, so you add those files rather than replace them. **Unknown.** The Tiny ML Tensorlab guide's comparison table lists the GUI's task types as "Arc Fault, Motor Fault", while the MSPM0 guide shows a generic time-series project; which applies to a new classifier in version 1.7.0 was not checked.

## Step 6: how do I check it on the board before trusting it live?

This board program is not written; the steps below are the plan, and nothing in this step has run. Run the held-out test vectors first. Build a small program that feeds each window in `test_vectors.h` to the model and compares the answer with `ds_test_expected_class`. With the plain C model, the logits must match `ds_test_expected_logits` exactly. With NNC, its float outputs should equal those integers: on the occupancy models all 80 values matched on the LP-MSPM0G3507. The harness for those models is the pattern: `mspm0g3507/mcu-bench/edgeai-sdk/occupancy-nnc/main.c`.

Only when every vector matches, go live:

1. Keep the last 128 samples of `ax`, `ay`, `az` in a ring buffer.
2. Every 32 new samples, copy them out oldest first as `[channel][sample]`, divide by 16,384, and run the model.
3. Print the class name from `labels.h` over the same serial port.
4. Do each motion 20 times and count the right answers. That count, not the training score, is the number to report.

If the vectors match but live answers are wrong, compare a live window with a recorded one: the usual suspects are axes in a different order, a different scale, or the board mounted differently from when you recorded.

## What about washer vibration, sound clips, or datasets TI already ships?

They have their own page: [docs/guides/dataset-variants.md](dataset-variants.md) covers recording a washer's phases with the same tools, collecting sound clips on the LP-MSPM0G5187, the stored test data in TI's AI examples, and two public datasets with their licenses.

## Questions

### Why might my BMI160 read all zeros for motion?

Send the two wake-up commands first (register 0x7E: 0x11, then 0x15), as `bmi160_init` does. The data sheet says both sensors start in suspend mode, and TI's `data_sensor_aggregator` example reads the data registers without ever writing 0x7E. This has not been seen: the BoosterPack has not arrived.

### What do I flash on the board before recording?

A program built from `portability/dataset-stream/` plus a board file for your LaunchPad. Not written: the board file does not exist for any board in this repo; `portability/dataset-stream/boards/README.md` lists the pins each one needs.

### Why do I need three sessions when one long recording gives thousands of windows?

Because the test must be a recording the model never saw. The tools split by segment (one unbroken stretch of one label), so a class needs at least three segments, and separate sessions make them differ the way real use will. With one segment per class, `check.py` puts every window in training and `train.py` stops with "every class needs windows in train and test". With two, one is training and one is test, with nothing to tune on.

### What happens to rows labeled "none"?

They stay in the CSV file and every tool skips them. Use space to mark the moments between classes (picking the board up, resting) so they do not teach the model anything.

### Does this work on Windows or Linux?

`check.py` and `train.py` do. `capture.py` reads key presses the macOS and Linux way (`termios`), so it is not written for Windows; pass the port with `--port` (on Linux usually `/dev/ttyACM0`). Tested only on macOS 15.7.9.

### Can I run TI's NNC on an Intel Mac?

No: TI's index has NNC 2.1.2 wheels for Linux x86-64, Windows and Apple Silicon Macs only (checked 2026-10-08). Compile on GitHub's Linux machines with `nnc-compile.yml`, or use Edge AI Studio's cloud version.

### Will the model run on the LP-MSPM0L1306?

Not this one: the 3-channel, 128-sample plain-C model needs 4.4 kB of RAM and the chip has 4 kB. Stream data from the L1306 if you like, and run the model on a G3507 or G5187, or train a smaller window.

### Why is TI's "time_series_data_capture" folder missing?

In MSPM0 SDK 2.11.00.07 it is spelled `timeseries_data_capture`. TI's Edge AI Studio guide for MSPM0 uses the other spelling.

### What does "unreadable lines: 2" in the capture summary mean?

Usually nothing to fix. Expected, not seen on a board: a port opened in the middle of a line cuts that line in half. In the host test the 2 unreadable lines are a boot line and a cut-off line. `capture.py` counts any line that is not a comment, the header or a full row of whole numbers as unreadable and leaves it out of the CSV file. If the count keeps rising during a recording, check the baud rate (115200) and the cable.

### What do the "# overrun" lines mean?

The board was late and skipped that many samples. `capture.py` adds them up ("board reported 3 skipped") and leaves them out of the CSV file; `check.py` finds the same gap from the `t_ms` values and warns above 1% missing.

### capture.py says "stdin is not a terminal: no key presses, every row is labeled none; stop with --seconds". Why?

Run it in a Terminal window. It was started with its input redirected or from a script, so it cannot read key presses. Use `--seconds` to stop it when you only want to test the link.

### I pressed the wrong key mid-recording. What now?

Press the right key at once. The wrong label lasts until your next key press. Note the time; afterwards delete those rows from the CSV file or change their label column to `none`. The 1 s trim at the start of each segment hides a short slip.

### Why one Python for recording and another for training?

Recording needs only pyserial (and matplotlib for the plot), which install on any Python 3. Training needs torch 2.2.2, the last PyTorch for Intel Macs, which installs only up to Python 3.12. On an Apple Silicon Mac one Python 3.12 environment can hold both; not run.

## Limits

No LaunchPad board file exists for `portability/dataset-stream`, so no real recording, held-out score or on-board test-vector run is on this page. Edge AI Studio's cloud route and the LP-MSPM0G5187 microphone path have not been tried.
