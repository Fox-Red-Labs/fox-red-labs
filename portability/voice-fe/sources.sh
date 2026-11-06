# Sourced by the build scripts: FE_SRCS (the front end's C files) and FE_INC (include flags). Needs REPO set.
_V="$REPO/portability/voice-fe"; _S="$REPO/third-party/speexdsp"
FE_SRCS="$_S/libspeexdsp/mdf.c $_S/libspeexdsp/preprocess.c $_S/libspeexdsp/fftwrap.c $_S/libspeexdsp/kiss_fft.c $_S/libspeexdsp/kiss_fftr.c $_S/libspeexdsp/filterbank.c $_V/beamformer.c $_V/fe_arena.c $_V/fe_run.c"
FE_INC="-DHAVE_CONFIG_H -I$_V/speex_cfg -I$_V -I$_S/include -I$_S/libspeexdsp"
unset _V _S
