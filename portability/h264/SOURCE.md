# H.264 decode harness: sources and patch

Where the H.264 decode harness comes from, how it is patched and how the test streams are made. Terms: [docs/vocab.md](../../docs/vocab.md).

- Decoder: h264bsd, <https://github.com/oneam/h264bsd>, commit `42bcb5d753ad86d84903354bf3c68423c28adb7b` (2022-04-26). Not copied here: `fetch_src.sh` clones it into a work folder and copies `src/` aside. License: files from the Android Open Source Project, Apache-2.0; wrappers MIT (the repository's LICENSE.md).
- Patch: `h264bsd-cfg.sed` lowers `MAX_NUM_SEQ_PARAM_SETS` from 32 to 2 and `MAX_NUM_PIC_PARAM_SETS` from 256 to 2 in the copy. Upstream is never edited. `UNPATCHED=1 fetch_src.sh` skips it.
- `streams/prof-*.h264`: made by `mkprofile.sh` (30 frames at 30 fps, `-b:v 600k -maxrate 600k -bufsize 1200k`, baseline and main). `streams/bbb-640x360.h264`: the first 30 pictures of `test/test_640x360.h264` in the h264bsd repository (a clip from "Big Buck Bunny", Blender Foundation, CC BY 3.0 per the film's page; not re-checked).
- `shim/memory.h`: one line, `#include <string.h>`, because the decoder includes `<memory.h>`, which TI's C library lacks.
- Use: `build_host.sh` (Mac reference, prints one `H264R` line per stream and pass), `build_linux.sh` with `linux_main.c` (a native build on a Linux board, `cyc=` in nanoseconds; AM62L), the board builds in `mspm33c321a/tools/build_h264.sh`, `am13e230/tools/build_h264.sh`, `cc35x1/bench/build_h264.sh`; `parse_rec.py` turns a RAM readback into the same lines; `summarize.py` and `make_rows.py` make the tables and the rows in `data/results/`. Write-up for the AM62L: `docs/experiments/2026-10-23-am62l-h264-decode.md`.
