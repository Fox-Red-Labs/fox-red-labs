# SK-TDA4VM starter kit: notes for an AI coding assistant

The board may have no network, no display and a blank card, so everything goes through one serial console, and heavy runs can reset it without a log. This file is the short list. Each item comes from runs on one board unless it says otherwise.

Terms: [docs/vocab.md](../docs/vocab.md) and the [SK-TDA4VM FAQ](FAQ.md) in the Fox Red Labs repo. In short: the **A72** cores run Linux; the **C7x** is the chip's DSP with its matrix unit, where **TIDL** (TI's deep-learning runtime) runs models; the **R5F** cores are small real-time cores; **U-Boot** is the bootloader; **UART boot** is the chip's way of taking its bootloader over the serial port. 1 kB = 1,024 bytes. Checked 2026-10-10 on an Intel Mac, macOS 15.7.9, with lrzsz 0.13.1 and pyserial 3.5, on TI Edge AI SDK 11.00.00.08, Linux 6.12.17-ti, gcc 13.3, on one SK-TDA4VM (`tda4/README.md`).

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`. Most coding assistants read that name; for one that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md` to your `CLAUDE.md`. Paths below in backticks starting `tda4/` are in the Fox Red Labs repo, where the evidence for each line lives; they are not in your copy, and links in this file only work inside that repo.

## The board

| Part | Fact |
|---|---|
| Kit | SK-TDA4VM, orderable J721EXSKG01EVM, board revision PROC112A1 (`tda4/README.md`) |
| Chip | J721E: two Cortex-A72 at 1.998 GHz measured with perf, a C7x DSP with matrix unit, three free R5F cores (`tda4/README.md`) |
| Memory | 4 GiB, but Linux sees 2,170 MiB; the rest is reserved for firmware, codecs and DMA before Linux starts (`tda4/FAQ.md`) |
| Boot switch | SW1 all OFF boots from the micro-SD card; 1 ON, 2 OFF, 3 ON, 4 OFF is UART boot (`tda4/README.md`) |
| Console | J4 micro-B, a CP2108 with four ports; 115200 8N1, login `root`, no password (`tda4/README.md`) |
| Image | TI Edge AI Linux (Processor SDK Linux Edge AI 11.00.00.08), file `tisdk-edgeai-image-j721e-evm.wic.xz` (`tda4/README.md`) |

## Start here

This is a Linux starter kit: nothing is built or flashed to a chip. The first result is a boot from the micro-SD card to a `root` login on the serial console, then a model run on the C7x. No LED or camera step was run on this kit, so there is no blink to offer; a camera needs hardware the bench lacked (`tda4/README.md`, "Limits").

Built with TI Edge AI SDK 11.00.00.08 (Linux 6.12.17-ti, gcc 13.3), the CP210x VCP driver, `brew install lrzsz` (0.13.1) and pyserial 3.5 on the Mac, `xz` 5.8.4 to unpack the image.

| Step | Command or fact |
|---|---|
| Backup | The SD card is the board's state and the image is its backup: `tisdk-edgeai-image-j721e-evm.wic.xz` (1.12 GB, SHA-256 `7bebcff62930c93bc0d10462385a936305ffdfd2457c4bbd85c2b2f216076da0`, from the SK-TDA4VM software page, no login). The box card was never read back, so there is no tested backup of its contents. Check a download with `xz -t` |
| Write the card | With a reader: `tda4/tools/write-card.sh diskN path/to/image.wic.xz` (refuses the internal disk, destroys the card). With none: the "No-reader path" in `tda4/README.md` (`tda4/tools/uart_boot.py`, then `python tools/write_from_stick.py unzip`) |
| Boot | SW1 all OFF, open the console first, power on. Login `root`, no password, 115200 8N1 |
| Check | The `login:` prompt. First boot 27.7 s, bootloader to login. `python3 tda4/tools/warm_boot.py <n>` times warm boots |
| First model | Models are in `/opt/model_zoo`, one folder each with `artifacts` and `param.yaml`. Push `tda4/bench/zoo/tvm_bench.py` with `tda4/tools/push_serial.py` and run it; the runtime log lists every node on the C7x (274 of 274 for the pose model) |
| Restore | Write the image to the card again. An interrupted write is safe to rerun: chunks go to fixed block addresses |

## If you see this

| What you see | Cause | Do this | Check |
|---|---|---|---|
| All four ports silent, a reset changes nothing | SW1 not all OFF, wrong port, or a blank card (inferred, not tested) | Check SW1 and the port, then write the image | A boot log appears |
| Console silent after the USB cable moved | The console can come back under another port name | Probe all four ports (`tda4/tools/conlog.py`) or set `TDA4_PORT` | Text on one port |
| Shell stuck at a `>` prompt after a file push | A 107 kB single line overran the line buffer | Wrap base64 at 76 columns; use `tda4/tools/push_serial.py` | The hash matches |
| `spl: ymodem err - Timed out` during UART boot | An old script missed U-Boot's retry signal (C bytes, then NAKs) | Use the current `tda4/tools/uart_boot.py` | The U-Boot `=>` prompt appears |
| `Failed to start Start psplash boot splash screen` and `Direct firmware load for j7-mcu-r5f0_1-fw failed with error -2` | No display, and TI ships no firmware for three of the six R5F cores | Ignore both | The prompt appears |
| `[ERROR] Error pulling frame from GST Pipeline` | The sample clip ended; `loop: True` did not loop a raw `.h264` file | Concatenate the clip to itself (20 copies) | The run lasts the full time |
| `ALG_PERR: ... e_afalg.c(463): Failed to bind socket : No such file or directory` | The kernel has `CRYPTO_USER_API_SKCIPHER` off | Use the A72 crypto instructions (AES-128-CBC 1.49 GB/s) or `/dev/crypto` | openssl prints a speed |
| `'FLAGS_STR' undeclared` (CoreMark) | Missing flag string and posix overrides | Add `-DFLAGS_STR='"-O3"'` and EEMBC's `core_portme_posix_overrides.h` | Score validates |
| `conflicting types for 'times'` (Dhrystone) | `-DTIMES` clashes with glibc | Build with `-DTIME`; take the score from `perf stat` cycles | A DMIPS figure prints |
| `v4l2h264dec: Failed to allocate required memory` and `Buffer pool activation failed` at 1080p | The zero-copy pipeline fails at pool sizes 4, 8 and 12 (cause unknown) | Drop `capture-io-mode=5` and `tiovxmemalloc` at 1080p | The pipeline runs |
| `Broadcast address not provided in DataPlane initialization` (sensor-bridge emulator) | The interface has no broadcast address | `ip addr add <address>/24 brd + dev hsb0` | The emulator starts |
| `failed to initialize legacy DRM` (`kmscube`) | No display attached | Expected; the GPU rows need a display | No display on this bench |

Traps shared with other boards: `TRAPS.md` at the top of the repository.

## How to reach it

Install the CP210x VCP driver and click Allow in System Settings, Privacy and Security. Apple's own driver covers only the single-port CP2102, so no port appears without it (`tda4/FAQ.md`). The ports are then `/dev/cu.SLAB_USBtoUART`, `...1`, `...2` and `...4`. On the Mac used here the Linux console was `/dev/cu.SLAB_USBtoUART1` and the chip's UART-boot port was `...2`; after the cable moved to another USB port the console became `...2`. If in doubt, log all four with `tda4/tools/conlog.py`.

Open the console before powering the board. Connecting after boot can show nothing; press SW2 (reset) with the console open (`tda4/FAQ.md`).

If the card in the box boots nothing (the card here did not; it is inferred to have been blank), write TI's image with a micro-SD reader (`tda4/tools/write-card.sh`). With no reader, the card was written from the board itself: UART boot loads U-Boot, then `tda4/tools/write_from_stick.py` copies gzip chunks from a USB stick to the card. The full steps are in `tda4/README.md`, "No-reader path". Measured: 11 minutes hands-on and 51 minutes waiting, 62 counted minutes from the first console check to the Linux login.

Start anything long on the board like this, because the console helper sends Ctrl-C each time it connects (`tda4/FAQ.md`):

```text
setsid nohup <command> > log 2>&1 &
```

## What ran here

- First boot, bootloader to login: 27.7 s. Warm boot: 24.0 to 24.5 s (`tda4/README.md`, row `linux_boot_time`).
- YOLOX-s pose model 640x640: 13.8 ms on the C7x, 1,767 ms on the two A72s in fp32 (`tda4/README.md`, row `dl_inference_ms`).
- 11 TI model zoo models on the C7x: 2.2 to 13.8 ms each (`tda4/README.md`).
- CoreMark: 9.8k iterations per second on one A72 thread, 19.6k on two. Dhrystone 4.83 DMIPS per MHz (`tda4/README.md`).
- Memory bandwidth on one A72: copy 5.85 GB/s, triad 4.79 GB/s (`tda4/README.md`, row `mem_bandwidth`).
- Hardware H.264 decode, 1280x768 clip: about 450 frames per second with start-up inside the time (`tda4/README.md`).
- 9 of TI's 15 demo configs ran headless with a file in place of the display: the 8 file-only configs for 30 s, and the HTTP-source config once the board had Ethernet, at only 1.78 frames a second. The rest need a camera or hardware the bench lacks (`tda4/RESULTS-tda4.md`).
- Not run: R5F CoreMark (no J721E RTOS SDK installed), power, camera, display and GPU rows (`tda4/README.md`, "Limits").

## Traps

1. **Zero-copy decode.** `capture-io-mode=5` on `v4l2h264dec` and `tiovxmemalloc pool-size=12` after it: 110.8 fps at 33 percent of one A72, against 56.2 fps at 65 percent without (`tda4/FAQ.md`).
2. **Thermal.** The board idles at 72 to 76 C with its passive heatsink. A 5 minute pose run reached 91.5 C with no reset. Under a heavier load (2-thread CoreMark plus a TIDL loop) it reset about 60 s in, 4 s after a 92.6 C reading, cause not found. It also reset once at idle. Wait for every zone under 80 C between heavy runs (`tda4/FAQ.md`).
3. **U-Boot `ums` does not work** on this kit: both USB controllers come up host-only and rebinding one crashed U-Boot. Use a stick (`tda4/README.md`).
4. **Port names differ from TI's table.** TI's guide lists WKUP_UART0, MCU_UART0, UART0 (console), UART1; the Mac numbers them differently (`tda4/FAQ.md`).
5. **The emulator's CMake wants CUDA;** build with `tda4/hsb/Makefile` (`tda4/FAQ.md`).

<!-- ports:start (tools/render_ports.py) -->
## Ports that ran here

Code written for other chips and tools, brought to this board. The files to copy, the command and what you see for each are in `docs/ports.md`; the method and a prompt to port your own are there too. Works ran and did what it should; Could work ran in part.

| Port | Status | Result | Evidence |
|---|---|---|---|
| A radar frame parser on the LP-AM13E230 and SK-TDA4VM | Works | A radar frame parser in 882 bytes of C gives the same output on the LP-AM13E230 and SK-TDA4VM as the Mac parser. | `docs/experiments/2026-10-09-am13e230-radar-host-parser.md` |
| An MSPM0 FFT example on the SK-TDA4VM | Works | TI's MSPM0 FFT example runs unedited on the SK-TDA4VM and prints the same peak as on an R5F core and on a Mac. | `portability/everywhere/host-fft/README.md` |
| NVIDIA Holoscan's camera graph on the SK-TDA4VM | Could work | NVIDIA Holoscan needs CUDA, so its camera graph is rebuilt from TI parts on the SK-TDA4VM: YOLOX-s pose takes 13.8 ms on the C7x accelerator. | `docs/experiments/2026-10-09-tda4-holoscan-graph.md` |
<!-- ports:end -->

## Where the evidence lives

`tda4/README.md` for the bring-up and numbers, `tda4/FAQ.md` for each problem, `tda4/RESULTS-tda4.md` for how every number was made, `data/results/tda4.csv` for the rows, `tda4/hsb/` for the sensor-bridge build. If you find something this file gets wrong on your board, the FAQ is where it should be corrected first.

<!-- recovery:start -->
## When the board stops answering

Checked 2026-10-24. Before flashing a risky image, run `python3 tools/preflight_image.py <board id> <image.out>`. Narrative and commands: `docs/remote-board-recovery.md`.

Try in this order. Remote means the Mac does it with the board in place; hands means a person unplugs, presses or moves something.

| Method | Remote or hands | Status | Evidence |
|---|---|---|---|
| No card reader: UART boot (SW1 1 ON, 2 OFF, 3 ON), tools/uart_boot.py, then write the image from a USB stick; SW1 all OFF and power cycle (switch settings are hands; the transfer is remote) | hands | proven | `tda4/FAQ.md`, date not recorded |

<!-- recovery:end -->
