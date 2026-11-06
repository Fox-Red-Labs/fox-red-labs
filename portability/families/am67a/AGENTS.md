# AM67A family (BeagleY-AI): porting notes for an AI coding assistant

That Linux code from the AM62L moves to the BeagleY-AI with no edits, that the accelerator and the video codec belong behind two glue calls, and that the hardware video decoder is not reliable today. Each line below comes from a run or a file in the Fox Red Labs repo.

Terms: [docs/vocab.md](../../../docs/vocab.md) and the [family FAQ](FAQ.md). **C7x/MMA** is the DSP core and accelerator; **TIDL** is TI's runtime that offloads a network to it; **Wave5** is the video codec block. 1 kB = 1,024 bytes. Checked 2026-11-03 with BeagleBoard Edge AI image 11.00.00.08 (Linux 6.12.33) on one BeagleY-AI.

**How to use it.** Copy this file into the top folder of your own project as `AGENTS.md`; for an assistant that reads `CLAUDE.md`, copy it under that name or add the line `@AGENTS.md`. Paths in backticks are in the Fox Red Labs repo.

## Rules, each from a run

- Reuse `portability/three-ways/linux/glue_linux.c` as it is and supply a config header with `-DCONFIG_AM62L_H -include <header>` (`portability/matrix.csv` X56).
- The board has no ADC: expect `adc=65535` from the portable app (`beagley-ai/bench/portability/three-ways-run.log`).
- Build with the board's own `gcc` (13.3.0); there is no `libc.a`, so no `-static` (`beagley-ai/bench/portability/three-ways-run.log`).
- The A53 has no LSE atomics: preload `am62l-evm/steam/lse_trap/` builds unchanged (`beagley-ai/bench/portability/lse-trap-run.log`).
- Put the accelerator behind `classifier()` and the codec behind `frames()` as in `beagley-ai/bench/seam/glue.py`; one model file, a per-chip artifacts folder (`beagley-ai/bench/seam/README.md`).
- Probe for `TIDLExecutionProvider` plus the artifacts folder and for a Wave5 device; allow `FRL_INFER` and `FRL_VIDEO` to force either (`beagley-ai/bench/seam/glue.py`).
- Decode with GStreamer; `ffmpeg -c:v h264_v4l2m2m` stopped after 3 frames when scaled and piped (`beagley-ai/bench/seam/ffmpeg-vs-gst-2026-11-03.log`).
- Use software decode for frames a program reads until the Wave5 stall is understood (`beagley-ai/bench/seam/v4l2-after-reboot.log`).
- Expect the classifier 11.6 times faster on the C7x and the pipeline 3 times (`data/results/beagley-ai.csv`, X58).
- A KVM guest, Redroid, Jailhouse and the AM62L's core-1 Zephyr route do not apply to the stock image (`beagley-ai/bench/portability/os-survey.log`).
- Python GPIO needs a board description: `gpiod` imports, `gpiozero` and `lgpio` are not installed, the chips are `gpiochip1` to `gpiochip3` (`beagley-ai/bench/portability/os-survey.log`).
- Containers move as they are: Mosquitto, Home Assistant, Pi-hole (BIND mode) (`beagley-ai/bench/docker/`).
