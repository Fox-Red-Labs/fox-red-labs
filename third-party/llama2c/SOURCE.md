# llama2.c

- Upstream: https://github.com/karpathy/llama2.c, commit `350e04fe35433e6d2941dce5a1f53308f87058eb` (2024-05-29).
- Copied unmodified: `run.c` (float inference), `runq.c` (int8 inference), `export.py`, `Makefile`, `README.md`, `LICENSE` (hashes in `SHA256SUMS`). Left out: the training code, the 32000-token tokenizer, notebooks, images and the Windows files.
- Licence: MIT, Copyright (c) 2023 Andrej Karpathy.
- Model: TinyStories 260K, `stories260K.bin` (1,056,540 bytes) and `tok512.bin` (6,227 bytes) from https://huggingface.co/karpathy/tinyllamas/tree/main/stories260K. The model card on the hub lists the licence as MIT. The files are not copied here; `ports/llama2c/fetch.sh` downloads them into a git-ignored folder and checks their hashes.
- Used by: `ports/llama2c/`.
