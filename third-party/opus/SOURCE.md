# Opus

- Upstream: https://github.com/xiph/opus, tag `v1.5.2`, commit `ddbe48383984d56acd9e1ab6a090c54ca6b735a6`. The Opus audio codec (RFC 6716) with its SILK and CELT layers, in C.
- Copied unmodified: `include/`, `celt/`, `silk/`, `src/`, `COPYING`, `AUTHORS`, `README`, `LICENSE_PLEASE_READ.txt` and the three `*_sources.mk` file lists. Left out: the `dnn/` folder (deep packet-loss concealment, DRED and the neural post-filters; none is built), `tests/`, `doc/`, `training/`, build systems, the x86 and MIPS code, the demo and compare programs, and the unit-test sources. Hashes are in `SHA256SUMS`.
- Licence: BSD-3-Clause (`COPYING`). The patent licences named in `COPYING` and `LICENSE_PLEASE_READ.txt` apply.
- Arithmetic: `FIXED_POINT` (integer, 16 and 32 bit) or `FLOATING_POINT` (single precision, with double precision in parts of SILK); one is chosen on the compiler command line. The port sets `HAVE_CONFIG_H` and supplies its own `config.h`.
- Used by: `ports/opus/`. The port compiles the files as they are and sets its options on the compiler command line.
