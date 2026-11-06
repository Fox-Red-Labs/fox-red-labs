# tflite-micro

- Upstream: https://github.com/tensorflow/tflite-micro, commit `4bb5a53e417b597ce5c2af2079aa8c42eb5fe9da` (2026-10-29). TensorFlow Lite for Microcontrollers, C++ runtime for quantised models on small CPUs.
- How it was made: `python3 tensorflow/lite/micro/tools/project_generation/create_tflm_tree.py <dir>` (upstream's own source-tree generator, reference kernels, host target). It downloads and patches flatbuffers v25.9.23 and kissfft v130 (`tensorflow/lite/micro/tools/make/*.patch`) and copies only the sources and headers the library needs. Files here are that output, then pruned: every `.cc` and `.c` file the microWakeWord build does not compile was deleted (the list is `ports/microwakeword/host/sources.mk`). Headers are all kept. `signal/` holds headers only (`kernels/micro_ops.h` includes them).
- Not edited by hand. The patched copies of `third_party/kissfft/` and `third_party/flatbuffers/include/` come from the generator.
- Licences: TFLite Micro and flatbuffers are Apache License 2.0 (`LICENSE`, `third_party/flatbuffers/LICENSE`); kissfft is BSD-3-Clause, Copyright (c) 2003-2010 Mark Borgerding (`third_party/kissfft/COPYING`).
- Used by: `ports/microwakeword/`. To regenerate: clone upstream at the commit above and run the generator; then run `make` in `ports/microwakeword/host`.
