# FatFs R0.16 (ChaN)

- Origin: <https://elm-chan.org/fsw/ff/> (archive `ff16.zip`, downloaded 2026-10-20).
- License: the FatFs license, a BSD-style license equal to the 1-clause BSD license (`LICENSE.txt`, unchanged; the same text heads every source file).
- Kept: the `source/` folder of the archive (`ff.c`, `ff.h`, `ffconf.h`, `ffsystem.c`, `ffunicode.c`, `diskio.c`, `diskio.h`, `00history.txt`, `00readme.txt`) byte for byte. The documents folder is not kept.
- Not edited. A project's own configuration is its own `ffconf.h`, put ahead of this one when staging the sources: `portability/boosterpacks/sharp128/ffconf.h`.
- Checksums: `SHA256SUMS`.
- Used by: `portability/boosterpacks/sharp128/` (microSD on the BOOSTXL-SHARP128).
