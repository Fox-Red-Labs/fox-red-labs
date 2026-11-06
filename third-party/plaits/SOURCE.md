# Plaits (Mutable Instruments) DSP, unmodified

Upstream: https://github.com/pichenettes/eurorack, `plaits/` at commit `f4cfa7697095a28d292bff758beb51deb49b5a86` (2026-10-12), with its `stmlib` submodule (https://github.com/pichenettes/stmlib) at commit `e3bd7c9cc00e4364166f9905c0509b6ffd0535ec`.
License: MIT, Copyright Emilie Gillet (`LICENSE-plaits.txt`, `stmlib/LICENSE`). The upstream README says "Code (STM32F projects): MIT license." and asks that the name "Mutable Instruments" is not used on derivative works.

Copied: `plaits/dsp/` (all engines, speech, FM, physical modelling, fx, oscillators, the voice), `plaits/resources.cc`, `plaits/resources.h`, `plaits/user_data.h`, `stmlib/stmlib.h`, `stmlib/dsp/` (without the `.py` generators) and `stmlib/utils/` (without `murmurhash3.h` and `crc32.h`, which no Plaits file includes and whose headers name no licence).
Left out: the module firmware (`plaits.cc`, UI, drivers, bootloader, hardware design), `plaits/resources/` (the Python generators and `waves.bin`; `resources.cc` holds the tables), the user-data receiver and the other eurorack modules.
Files are byte-identical to upstream; `SHA256SUMS` lists them. Build with `-DTEST` as upstream's own test makefile does (it turns off the STM32 flash-writing code in `user_data.h`).
