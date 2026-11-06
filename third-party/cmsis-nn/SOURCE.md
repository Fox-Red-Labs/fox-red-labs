# cmsis-nn

- Upstream: CMSIS-NN https://github.com/ARM-software/CMSIS-NN at commit `18bb0fd4ecea4b17a5a3c444c8d3a8cf28b1e68f`, and CMSIS_6 https://github.com/ARM-software/CMSIS_6 at commit `5782d6f8057906d360f4b95ec08a2354afe5c9b9` (core headers only). Both are the pins in tflite-micro's `MODULE.bazel` at `../tflite-micro`'s commit.
- How it was made: `create_tflm_tree.py` with `--makefile_options="TARGET=cortex_m_generic TARGET_ARCH=cortex-m33 OPTIMIZED_KERNEL_DIR=cmsis_nn"` (needs `wget`, here a curl shim). From the output: the 14 `tensorflow/lite/micro/kernels/cmsis_nn/*.cc` TFLM kernel wrappers, `third_party/cmsis_nn` (headers and the 56 source files the microWakeWord image links; the other 57 were deleted), and `third_party/cmsis/CMSIS/Core/Include` (CMSIS-Core headers).
- Licence: Apache License 2.0 (`third_party/cmsis_nn/LICENSE`, `third_party/cmsis/LICENSE`). The TFLM kernel wrappers are Apache 2.0, Copyright The TensorFlow Authors.
- Used by: `ports/microwakeword/m33/` (`make CMSIS=1`). Optional: the reference kernels in `../tflite-micro` are enough to run the models.
