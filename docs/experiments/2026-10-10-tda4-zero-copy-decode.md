<!-- experiment
id: E172
board: SK-TDA4VM
date: 2026-10-10
status: works
reached: verified
line: Yes. With TI's zero-copy settings (v4l2h264dec capture-io-mode=5 feeding tiovxmemalloc), which by TI's description let the decoder write straight into buffers the C7x reads, a decode-to-detection pipeline (1280x768 H.264 into YOLOX-nano-lite 416 on the C7x) ran at 110.8 frames a second using 33 percent of one Cortex-A72. The same pipeline without those two settings, where the A72 copies every decoded frame, ran at 56.2 frames a second using 65 percent of a core. Zero copy used half the CPU time per second at twice the frame rate, about a quarter of the CPU time per frame; the copy was the largest extra cost measured, while the decoder alone does about 490 fps and the model 4.7 ms.
stop: none
-->

# Can the SK-TDA4VM's hardware video decoder feed the C7x with no Cortex-A72 copy of the pixels, and what does the zero-copy path save?

Terms used here are in [docs/vocab.md](../vocab.md) (GStreamer, NV12, zero copy, DMABUF, TIOVX, VPAC, C7x, fakesink and fpsdisplaysink, maxrss) and the board's [FAQ](../../tda4/FAQ.md). Board page: [tda4/README.md](../../tda4/README.md). Numbers: `gst_pipeline_fps` rows in `data/results/tda4.csv`. MB = 1,000,000 bytes; MiB = 1,048,576 bytes.

**Short answer.** Yes. On the SK-TDA4VM, TI's two zero-copy settings (`capture-io-mode=5` on the decoder, `tiovxmemalloc` after it) took a decode-to-detection pipeline from 56.2 to 110.8 frames a second and cut the Cortex-A72's share from 65 to 33 percent of one core. If your pipeline into TIDL runs well below the model's rate with one core nearly full, check for those two settings first.

**Question.** In a vision pipeline the decoded frames are the big data: a 1280x768 frame in NV12 is 1.47 MB, and at 100 frames a second that is 147 MB/s. If the Cortex-A72 has to copy each frame from the decoder's buffer into the accelerator's, that copy costs CPU time and caps the rate. TI's pipelines set two things that are meant to avoid the copy. Do they, and by how much?

**Vendor position.** TI, Edge AI SDK 11.00.00.08 documentation, "Data flows" and the GStreamer plugin pages (read 2026-10-09): the decode-to-inference pipelines use `v4l2h264dec capture-io-mode=5 ! tiovxmemalloc`, where mode 5 is DMABUF import and `tiovxmemalloc` allocates the shared buffers the TIOVX plugins (`tiovxmultiscaler`, `tiovxdlpreproc`, `tidlinferer`) pass between the hardware blocks. TI's `edgeai-gst-apps` prints exactly this chain when run with `-v`; the pipeline below is that print with a measuring sink in place of the display.

Expectation before the run: "Yes with `v4l2h264dec capture-io-mode=5 ! tiovxmemalloc`; TI's dataflows use exactly this", with the A72 load as the result.

**What was tried.** Board: SK-TDA4VM (J721EXSKG01EVM), TI Edge AI SDK 11.00.00.08, Linux 6.12.17-ti, no display, no network, 2026-10-10. Input: 20 copies of TI's `video0_1280_768.h264` (300 frames each) concatenated, so the pipeline runs past one clip. Model: `ONR-OD-8200-yolox-nano-lite-mmdet-coco-416x416` from the zoo on the C7x (4.66 ms per inference measured alone).

Three pipelines through `gst-launch-1.0`, 20 s each, `fpsdisplaysink` over `fakesink sync=false` so nothing paces the frames:

1. Zero copy: `filesrc ! h264parse ! v4l2h264dec capture-io-mode=5 ! tiovxmemalloc pool-size=12 ! video/x-raw,format=NV12 ! tiovxmultiscaler ! video/x-raw,width=416,height=416 ! tiovxdlpreproc model=M out-pool-size=4 ! application/x-tensor-tiovx ! tidlinferer target=1 model=M ! fpsdisplaysink ...`
2. Plain: the same with `v4l2h264dec ! video/x-raw,format=NV12` (no `capture-io-mode`, no `tiovxmemalloc`).
3. Decode only: pipeline 1 without the scaler, preprocessor and model.

CPU time from `/usr/bin/time` on the process and from `/proc/stat` for the whole system. A thermal guard waited until the hottest zone read 80 C or less before each run (the log line printed a moment later reads up to 80.5 C) and would have stopped a run at 90 C (the board reset at 92.6 C during an earlier heavy run; see the FAQ entry on heat); it never fired.

**How far it got.** Measured; every pipeline ran. Two runs of each gave the same CPU time to 1 percent.

| Pipeline | Frames a second | Process CPU, one core | Whole system, two cores | Peak memory (`maxrss`) |
|---|--:|--:|--:|--:|
| Zero copy (mode 5, tiovxmemalloc) | 110.8 | 33% (6.6 s in 20 s; 3.4 user, 3.2 system) | 19% | 364 MiB |
| Plain (no shared buffers; by TI's description the A72 copies each frame, not profiled here) | 56.2 | 65% (13.1 s in 20 s; 11.8 user, 1.4 system) | 35% | 355 MiB |
| Decode only, zero copy | about 490 by wall clock (6,000 frames in 12.3 s including start-up; `fpsdisplaysink` averaged 499.6) | 50% (1.2 user, 5.0 system) | | 109 MiB |

**The line.** No line: the zero-copy path works as TI's data flows say. The numbers say what it is worth: half the A72 time per second at twice the frames, so about a quarter of the A72 time per frame (3.0 ms against 11.7 ms). Per frame the plain path spends 11.7 ms of A72 time and the zero-copy path 3.0 ms; the 8.7 ms difference is the most the copy can cost; copying a 1.47 MB frame in 8.7 ms would be a rate of about 170 MB/s, so this is a ceiling on the copy's cost and not a measured copy speed, because the two pipelines also differ in other plugin work. That copy was the largest extra cost measured; why it caps the rate at 56 fps when the core was only 65 percent busy is **Unknown (suspected: one thread doing the copy out of buffer memory the CPU cannot cache; not profiled).** The decoder alone does about 490 frames a second and the model about 215.

The zero-copy pipeline itself is not at either hardware limit (110.8 fps against 215 for the model). Its remaining 33 percent of a core split as 3.2 s of system time and 3.4 s of user time in 20 s; suspected, not profiled: buffer handoffs between the decoder, the scaler and the DSP, and the plugins' own work. TI's data-flow documentation says the scaler and preprocessor run on the VPAC hardware, not the A72; that was not checked here.

**Ask.** If you run a camera or a file into TIDL on a TDA4 and your frame rate is lower than the model's, check for a missing `capture-io-mode=5` or `tiovxmemalloc` first; report what you found.
