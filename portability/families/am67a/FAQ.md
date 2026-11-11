# AM67A family (BeagleY-AI): porting errors, causes and fixes

Problems hit moving code onto the BeagleY-AI on 2026-11-03. Board-bring-up errors are in [`beagley-ai/FAQ.md`](../../../beagley-ai/FAQ.md). Terms: [docs/vocab.md](../../../docs/vocab.md). Cause tags: Setup or handling; Tool or docs; Third-party behavior; Chip limit; Unknown.

### The portable app prints `adc=65535` on every line

Fix: nothing is wrong with the build. The BeagleY-AI has no ADC (no `/sys/bus/iio` device), so the glue's read returns 0xFFFF. Point `ADC_RAW` in your config header at another numeric sysfs file, or read the header's ADC through an external converter. Source: `beagley-ai/bench/portability/three-ways-run.log`.

### `glue_linux.c` includes `config_am62l.h` and the board needs its own paths

Pass `-DCONFIG_AM62L_H -include <your header>.h`: the AM62L header's include guard is then already defined, the glue file stays byte-identical and your header supplies `ADC_RAW` and `LED_DIR`. Source: `portability/three-ways/linux/boards/beagley-ai/config_beagley_ai.h`.

### `cannot find -lc` with `-static`

The image has no `libc.a`; build dynamically. Source: `beagley-ai/bench/portability/three-ways-run.log`.

### Python picks no accelerator and runs the model on the Arm cores

`onnxruntime.get_available_providers()` must list `TIDLExecutionProvider` and the model's compiled `artifacts` folder must exist; the glue's probe uses the CPU provider when either is missing. Source: `beagley-ai/bench/seam/glue.py`.

### The hardware-decode choice runs for 90 s and prints nothing

Use software decode (`FRL_VIDEO=sw`). The Wave5 decoder into a pipe completed 4 times and failed 7; the cause is not found. Source: `beagley-ai/bench/seam/v4l2-after-reboot.log`.

### A KVM guest or Redroid will not start: `/dev/kvm` is missing

The stock kernel is built without KVM, binder and `binfmt_misc`. Rebuild the kernel with them, as the AM62L runs did. Source: `beagley-ai/bench/portability/os-survey.log`.
