# uLisp for ARM

- Upstream: https://github.com/technoblogy/ulisp-arm, commit `d6427ec6fc9326b7f9a8349f06b94414a7c862cc` (2026-10-07, "Update README.md"). The sketch header reads "uLisp ARM Release 4.9a", 2 March 2026. A Lisp interpreter for ARM boards, written as one Arduino sketch.
- Copied unmodified: `ulisp-arm.ino`, `LICENSE`, `README.md`. Left out: `ulisp-arm-comments.ino` (the same program with long comments). Hashes are in `SHA256SUMS`.
- Licence: MIT, Copyright (c) 2016 David Johnson-Davies (`LICENSE`; the sketch header says the same).
- Used by: `ports/ulisp/`. The sketch is C++ with a fixed list of supported boards. The port copies it to a build folder, adds one board block and one keyword block with `patches/apply.py`, and compiles it with a small Arduino shim. This folder stays byte for byte as published.
