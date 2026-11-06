# Petit FatFs R0.03a (ChaN)

- Origin: <https://elm-chan.org/fsw/ff/00index_p.html> (archive `pff3a.zip`, downloaded 2026-10-20).
- License: the FatFs license, a BSD-style license equal to the 1-clause BSD license (`LICENSE.txt`, unchanged).
- Kept: `pff.c`, `pff.h`, `pffconf.h`, `diskio.h`, `00readme.txt` byte for byte. The sample `diskio.c` skeleton and the documents are not kept; the disk driver is `portability/boosterpacks/sharp128/sharp_sd.c`.
- Not edited. The project configuration is `portability/boosterpacks/sharp128/pffconf.h`, staged ahead of this one.
- Why it exists here: it reads a FAT16 or FAT32 file with a few tens of bytes of RAM and no sector buffer, which is what a 1 kB part has.
- Checksums: `SHA256SUMS`.
- Used by: `portability/boosterpacks/sharp128/` (video player on the LP-MSPM0C1104).
