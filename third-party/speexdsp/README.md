# SpeexDSP 1.2.1

SpeexDSP is Xiph.Org's speech processing library: an acoustic echo canceller (`speex_echo`, the MDF algorithm), a preprocessor with noise suppression, a resampler and a jitter buffer. This folder is the unmodified 1.2.1 release, used by the voice front end in `portability/voice-fe/`.

| Item | Value |
|---|---|
| Version | 1.2.1 |
| URL | https://downloads.xiph.org/releases/speex/speexdsp-1.2.1.tar.gz (project page https://www.speex.org, source https://gitlab.xiph.org/xiph/speexdsp) |
| SHA-256 of the tarball | `8c777343e4a6399569c72abc38a95b24db56882c83dbdb6c6424a5f4aeb54d3d`, equal to Xiph's `SHA256SUMS.txt` (read 2026-10-26) |
| License | BSD-3-Clause style, in `COPYING` |
| Copied | `COPYING`, `AUTHORS`, `README`, `include/speex/`, and `libspeexdsp/` without the makefiles, the `test*.c` programs and `echo_diagnostic.m` |
| Changes | None. The build supplies `config.h`, `speexdsp_config_types.h` and `os_support_custom.h` from `portability/voice-fe/` |
| Checksums | `SHA256SUMS`, one line per copied file |

Build settings used here: `FIXED_POINT`, `USE_KISS_FFT`, `OS_SUPPORT_CUSTOM` (a static arena allocator that records the heap high-water mark), `EXPORT=` empty.
