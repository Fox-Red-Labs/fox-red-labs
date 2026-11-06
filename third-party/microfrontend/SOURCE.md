# microfrontend

- Upstream: https://github.com/tensorflow/tensorflow, `tensorflow/lite/experimental/microfrontend/lib/`, tree at commit `f54938edc091eef286861f9114e9f2129edc97c6` (master when copied, 2026-10-30). The TFLite Micro audio front end: window, 512-point fixed-point FFT, 40-channel mel filterbank, noise reduction, PCAN gain control, log scale. It turns 16 kHz 16-bit audio into 40 features every 10 ms. microWakeWord's models are trained on this exact front end.
- Copied unmodified: the `.c`, `.cc` and `.h` files of the library. Left out: the `*_io` file writers, `frontend_main.c`, the memmap generator, `BUILD`, `README.md`.
- Why here and not in `third-party/tflite-micro/`: current tflite-micro no longer carries this front end (it moved to the `signal/` ops); the library still lives in the TensorFlow repository.
- Licence: Apache License 2.0, Copyright The TensorFlow Authors (headers in each file; text in `../tflite-micro/LICENSE`). `fft.cc` needs kissfft, which is in `../tflite-micro/third_party/kissfft/`.
- Used by: `ports/microwakeword/`.
