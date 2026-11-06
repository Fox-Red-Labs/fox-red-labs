# Porting code to the AM67A family (BeagleY-AI): the AM62L's Linux code moves as it is, and accelerators and the video codec get one seam

The AM67A is a Linux-class part with the same Cortex-A53 cores as the AM62L, so the AM62L's Linux code moves across with no edits: the portable app's glue file built on the BeagleY-AI byte-identical (66 lines, 0 edited) and needed a 2-line config header for the two file paths that differ (matrix id X56), and the AM62L's atomics shim and moonlight-embedded build rebuilt unchanged (X57). What the AM67A adds is a seam below the application: a C7x accelerator that runs a network in 8.14 ms against 94.82 ms on the Arm cores, and a Wave5 video codec. One small app picks both at run time through two glue calls, and the pipeline ran at 30.1 frames per second with the accelerator against 10.1 without (X58). With the frames scaled before the Arm cores convert them, hardware decode feeds the same app at 61.3 frames per second. Gaps: no display, camera or serial adapter has been attached; the stock kernel has no KVM; the board has no ADC and no power rails to read; the portable app runs on the MCU R5F core (X64, below) once three board quirks are handled in its glue.

Terms: [docs/vocab.md](../../../docs/vocab.md) and this family's [FAQ](FAQ.md). **C7x/MMA** is the DSP core and matrix accelerator; **TIDL** is TI's runtime that offloads a network to it; **Wave5** is the H.264 and H.265 codec block; **V4L2** is Linux's video device interface; **seam** is the line in an application where code that never changes meets code that changes per board; **glue** is the code on the board's side of that line. **LSE** is the ARMv8.1 atomic instruction set. 1 kB = 1,024 bytes. The generated record: [EVIDENCE.md](EVIDENCE.md). The assistant's copy: [AGENTS.md](AGENTS.md). Sibling guide: [AM62L](../am62l/README.md).

Contents: the part, the answers to four porting questions (what moves from the AM62L, where the accelerator seam goes, what links down to a microcontroller), a ten-minute path, what was measured, errors, sources.

## The board (one part held)

| Part | Kit | Core and clock | Memory | On the board | Source |
|---|---|---|---|---|---|
| AM67A (J722S) | BeagleY-AI 4 GB | four Cortex-A53 at 1.4 GHz, two C7x with MMA, R5F cores | 4 GB LPDDR4 (2,529 MB visible to Linux), microSD | Gigabit Ethernet, Wi-Fi and Bluetooth LE, micro-HDMI, Wave5 codec, 40-pin header, LEDs `ACT` and `PWR`; USB-C is power only on the Edge AI image | `beagley-ai/FIRST-BOOT.log`, `beagley-ai/bench/portability/os-survey.log` |

Board folder: `beagley-ai/`.

### Part-to-part map: the AM67A against the other Linux parts held

A dash means the files here do not record it.

| | AM67A (BeagleY-AI) | AM62L32 (TMDS62LEVM) | TDA4VM (SK-TDA4VM, off the bench) |
|---|---|---|---|
| Arm cores | 4 x Cortex-A53, 1.4 GHz | 2 x Cortex-A53, 1.25 GHz | 2 x Cortex-A72, 2.0 GHz |
| Accelerator | C7x/MMA, 4 TOPS | none | C7x/MMA, 8 TOPS |
| Video codec | Wave5, hardware: 379.8 fps decode at 1280x768 H.264 | none | hardware codec: 452 fps decode at 1280x768 H.264 |
| Pose model (YOLOX-S 640) on the accelerator | 43.13 ms | - | 13.76 ms |
| The same model on the Arm cores | 4,475.57 ms | - | 1,766.95 ms |
| RegNetX-200MF on the accelerator | 5.95 ms | - | 2.89 ms |
| Compiler on the board | GCC 13.3.0 | none; build in a Debian trixie arm64 container | GCC |
| ADC, power-rail sensors | none | IIO ADC; eight INA228 rails | - |
| KVM in the stock kernel | no | no (a custom kernel was built) | - |
| Atomics | ARMv8.0, no LSE | ARMv8.0, no LSE | - |

Sources: `data/results/beagley-ai.csv`, `data/results/tda4.csv` (rows `video_decode_fps`, `dl_inference_ms`), `portability/families/am62l/README.md`, `beagley-ai/bench/portability/os-survey.log`.

## What moves from the AM62L as it is, what needs glue

Every row ran on the BeagleY-AI on 2026-11-03 unless the last column says otherwise.

| AM62L item | Moves? | What differs | Evidence |
|---|---|---|---|
| The portable app, Linux glue (`portability/three-ways/linux/glue_linux.c`) | Unchanged | 0 glue lines edited; a 2-line config header gives the LED path (`/sys/class/leds/ACT`) and an ADC path that does not exist, so every line reads `adc=65535`; built with the board's own gcc (2.3 s, 66.1 kB) | X56, `beagley-ai/bench/portability/three-ways-run.log` |
| LSE atomics trap library (`am62l-evm/steam/lse_trap/`) | Unchanged | none; both self-tests print PASS; the same `Illegal instruction` without it | X57, `beagley-ai/bench/portability/lse-trap-run.log` |
| moonlight-embedded build (`am62l-evm/moonlight/build.sh`) | Unchanged (builds) | none; 157.8 kB, as on the AM62L; make 76 s, 469 s with pull, apt and clone; not run | X57, `beagley-ai/bench/portability/moonlight-build.log` |
| Home Assistant, Pi-hole, Mosquitto containers | Unchanged | Pi-hole needs `FTLCONF_dns_listeningMode=BIND`; Mosquitto pulled in 5.5 s and answered in 3.4 s | `beagley-ai/bench/docker/`, `data/results/beagley-ai.csv` |
| Node-RED container | Not run | the image is arm64; no reason to differ | - |
| Python GPIO (gpiozero, Blinka), `gpio_claim.sh` | Needs glue | `gpiod` imports; gpiozero and lgpio are not installed; the chip names (`gpiochip1` 4201000.gpio, `gpiochip2` 600000.gpio, `gpiochip3` 601000.gpio) and the header pin map are this board's; the AM62L script unbinds drivers named for its LEDs | `beagley-ai/bench/portability/os-survey.log` |
| LVGL, Slint, the libretro front end (`am62l-evm/hmi/`, `lean/`) | Needs an HDMI display | no `/dev/fb0` until a display is attached (framebuffer emulation is built in); libdrm and ALSA headers are on the image | `beagley-ai/bench/portability/os-survey.log` |
| Showcase (Xorg, Doom on the framebuffer, SuperTuxKart, RetroArch), Steam client | Needs an HDMI display | no Xorg or Xvfb on the host; the AM62L route is a container | - |
| KVM guests, Redroid | Needs a rebuilt kernel | `# CONFIG_KVM is not set`, binder off, `binfmt_misc` off; the AM62L used its own `-ti-kvm` kernel | `beagley-ai/bench/portability/os-survey.log` |
| Jailhouse, Linux-to-Zephyr on core 1, PSCI stub | Does not apply | AM62L U-Boot and two-core specifics; this part has four A53 cores and R5F and C7x cores loaded through `remoteproc` (R5F 78400000 and both C7x attached, R5F 79000000 offline) | `beagley-ai/bench/portability/os-survey.log` |
| Eight-rail power numbers | Does not apply | no INA228 sensors on this board; a USB-C inline meter is the only route | `beagley-ai/bench/portability/os-survey.log` |

## Where an application cuts its seam so one program runs on three boards

The program is `beagley-ai/bench/seam/` (a 23-line app and a 59-line glue file, README). The cut, in order:

1. **The application core** holds the loop, the post-processing and the output. It sees frames and class ids and no provider, device or element name. This is the same idea as `common/app.c`, which sees five calls (`board_init`, `adc_read`, `uart_write`, `led_toggle`, `sleep_ms`) and no register.
2. **An inference call** (`classifier()`): the glue holds the session. For onnxruntime that is the provider list (`TIDLExecutionProvider` then `CPUExecutionProvider`, or the CPU one alone), the compiled-artifacts folder for this chip, the thread count, and the input layout. The model file is one file for both providers. On the SK-TDA4VM the same call applies with a different artifacts folder (the TDA4VM and the AM67A are different compile targets); on the AM62L the probe finds no `TIDLExecutionProvider` and takes the CPU provider. The AM62L run of this app was not made.
3. **A video call** (`frames()`): the glue holds the decoder element and the scale step. The accelerated choice is `v4l2h264dec`; the fallback is `avdec_h264`. The scale step comes before the colour conversion: the decoder's buffers are slow for the Arm cores to read, so converting the full frame first ran at 6.5 fps. FFmpeg's `h264_v4l2m2m` is not a safe second choice here (it delivered 3 frames and stopped).
4. **A probe and two overrides**: with no setting, the glue uses the accelerator if the TIDL provider and the artifacts folder exist, and the codec if a Wave5 device exists. `FRL_INFER` and `FRL_VIDEO` force either path, which is how both ways were timed on one board.

What the glue file contains per board:

| Board | Inference | Video | Config values |
|---|---|---|---|
| AM62L | CPU provider | software decode | model path, thread count 2 |
| BeagleY-AI | TIDL provider, then CPU | Wave5 `v4l2h264dec`, scaled before conversion, else software | artifacts folder compiled for the AM67A |
| SK-TDA4VM | TIDL provider, then CPU | `v4l2h264dec`, the same element name (452 fps decode in `data/results/tda4.csv`) | artifacts folder compiled for the TDA4VM |

The same app on the same clip gave: accelerator and software decode 30.1 frames per second, CPU and software decode 10.1; the classifier alone 8.14 ms and 94.82 ms. The pipeline gains 3 times where the classifier gains 11.6 because decode and scale stay on the Arm cores and the frames cross to Python; a zero-copy pipeline (decoder to accelerator without a copy, the GStreamer element set in TI's `edgeai-gst-apps`) is the next cut: it ran on the SK-TDA4VM at 17.8 fps for pose plus encode (`data/results/tda4.csv`, `file-pose-encode`) and has not been run on the BeagleY-AI.

## What links down to a microcontroller

The boundary that exists is the five-call contract in `portability/three-ways/common/app.h`. `app.c` above it is byte-identical on 11 boards (X53), including this one (X56): on MSPM0 the glue below it is DriverLib (39 lines), on the AM62L and BeagleY-AI it is Linux sysfs and a tty (66 lines). The only protocol between them is the app's output line, `t=<n> adc=<raw> avg=<avg>`, which one checker (`portability/three-ways/serial_check.py`) validates on every board. Nothing above the five calls, such as inference or video, has a microcontroller counterpart in the files. The AM67A has R5F cores that Linux loads through `remoteproc` (two cores listed, one attached and one offline) and these are MCU-class. The upstream Zephyr tree already has the board `beagley_ai/j722s/mcu_r5f0_0`, and its memory map (code and resource table in the 15 MB carveout at `a1100000`) matches the Linux device tree, so the build needed no new board: an overlay moves the console to a RAM buffer (the last 64 KB of the carveout) that the resource table declares as a remoteproc trace buffer, and one new glue file (`zephyr/src/glue_zephyr_r5f.c`, `adc_read` returns 65535, no LED) sits under the unchanged `app.c` (X64). The image is 21,372 B of loadable segments. Linux loaded it into the core (`remote processor 79000000.r5f is now up`) and `trace0` showed the Zephyr banner and 16 lines `t=0 adc=65535 avg=65535` to `t=15`, 1.00 s apart; `echo stop` then returned in 0.029 s. The first try stalled after `t=0` and could not be stopped; three board facts caused that, all fixed in the glue file with `app.c` untouched. (1) The MCU timer at `04800000` is powered and counts at 25 MHz, but Zephyr's `ti_dmtimer` driver writes `TCLR` several times in a row and the write-posted timer drops the compare-enable bit, so no tick interrupt came; the glue sets it again after `TWPS` clears. (2) The mailbox page at `29010000` is cacheable under the default MPU map, so register reads were stale and writes stayed in the cache; the glue maps that page as device memory (MPU region 11). (3) `echo stop` makes the kernel send `RP_MBOX_SHUTDOWN` (0xFFFFFF14) and wait for 0xFFFFFF15 and for the core to sit in WFI; the glue answers on the mailbox and sleeps. Limits: the core cannot be started again after a stop until the board reboots (`module-reset deassert failed, ret = -19`); `adc_read` returns 65535 and the LED is not driven; remoteproc numbers change per boot, so select the core by its name `79000000.r5f`. Log: `beagley-ai/bench/portability/r5f-app-2026-11-04.log`.

## Ten-minute path

Run the seam app on a BeagleY-AI that already boots the Edge AI image (steps to boot it: `beagley-ai/README.md`). The commands are from `beagley-ai/bench/seam/README.md`.

1. Mac, terminal. Copy the app and the clip's folder is already on the board:

   ```bash
   scp beagley-ai/bench/seam/app.py beagley-ai/bench/seam/glue.py root@<board address>:/root/seam/
   ```
2. Board shell. Run both ways:

   ```bash
   cd /root/seam
   FRL_INFER=tidl FRL_VIDEO=sw python3 app.py /opt/edgeai-test-data/videos/video0_1280_768.h264 100
   FRL_INFER=cpu  FRL_VIDEO=sw python3 app.py /opt/edgeai-test-data/videos/video0_1280_768.h264 100
   ```

   You should see `fps=30` and `infer_median_ms=8.1` on the first, `fps=10.1` and `infer_median_ms=94.8` on the second (2026-11-03).
3. Board shell. Run the portable app unchanged, from `portability/three-ways`:

   ```bash
   gcc -std=c99 -D_POSIX_C_SOURCE=200809L -O2 -Icommon -Ilinux -DCONFIG_AM62L_H -include linux/boards/beagley-ai/config_beagley_ai.h common/app.c linux/glue_linux.c -o three_ways && timeout 14 ./three_ways
   ```

   You should see 14 lines `t=<n> adc=65535 avg=65535`, one a second.

## What was measured

| What | Number | Conditions | Level | Source |
|---|---|---|---|---|
| Portable app glue, edited lines | 0 of 66 | AM62L glue file reused | measured | `portability/matrix.csv` X56 |
| Portable app config header | 2 lines (AM62L: 5) | `config_beagley_ai.h` | measured | `data/results/beagley-ai.csv` `app_config_lines` |
| Classifier on the C7x, inside the app | 8.14 ms | RegNetX-200MF, 100 frames, median of 3 | measured | `data/results/beagley-ai.csv` `dl_inference_ms` `seam-app-regnetx-200mf-224-tidl-c7x` |
| The same on the Arm cores | 94.82 ms | median of 3 | measured | `seam-app-regnetx-200mf-224-a53` |
| Whole pipeline, accelerator | 30.1 fps | software decode, median of 3 | measured | `gst_pipeline_fps` `gst-avdec-regnetx-tidl-python-pipe` |
| Whole pipeline, Arm cores only | 10.1 fps | software decode, median of 3 | measured | `gst-avdec-regnetx-a53-python-pipe` |
| Hardware decode into the app | 61.3 fps | scale to 224x224 before `videoconvert`, median of 3; converting first gave 6.4 fps | measured | `gst-v4l2dec-scalefirst-regnetx-tidl-python-pipe` |
| Software decode into the app, scale first | 51.6 fps | median of 3 | measured | `gst-avdec-scalefirst-regnetx-tidl-python-pipe` |
| LSE trap self-tests | 2 of 2 PASS | rebuilt on the board | measured | `port_example_behavior` `lse-trap-tests-armv8-1-code-on-a53` |
| moonlight-embedded build | 157.8 kB, 469 s | not run | measured | `port_flash_bytes` `moonlight-embedded-f32e415` |

## If you see this

- If the portable app prints `adc=65535` on every line, the board has no ADC: the config header names a file that does not exist and the glue returns 65535 for it. Point `ADC_RAW` at another sysfs file or accept the constant.
- If a program dies with `Illegal instruction`, preload the trap library from `am62l-evm/steam/lse_trap/`; it builds here unchanged.
- If the hardware-decode choice prints no result, use `FRL_VIDEO=sw` (FAQ).

The rest, with causes: [FAQ.md](FAQ.md).

## Where these facts come from

BeagleBoard Edge AI image 11.00.00.08 (Linux 6.12.33), GCC 13.3.0 and 14.2.0 (container), onnxruntime 1.15.0, GStreamer and FFmpeg as shipped, moonlight-embedded f32e415, runs of 2026-11-03 on one BeagleY-AI 4 GB over Ethernet. Data: `portability/matrix.csv` (X56 to X58), `data/results/beagley-ai.csv`, `data/results/tda4.csv`, `data/results/am62l-evm.csv`, the logs under `beagley-ai/bench/portability/` and `beagley-ai/bench/seam/`. The generated record: `portability/families/am67a/EVIDENCE.md`. Checked 2026-11-03.
