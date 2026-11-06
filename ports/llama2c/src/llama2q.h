/* llama2q: int8 (Q8_0) inference for llama2.c checkpoints with no malloc, no file system and no stdio.
 * Fox Red Labs port of runq.c from https://github.com/karpathy/llama2.c (MIT, Copyright (c) 2023 Andrej Karpathy).
 * The arithmetic is runq.c's, line for line; what changed is where the memory comes from (static arrays),
 * where the weights come from (a read-only blob in flash) and how a token row is dequantized (one row, not the whole table). */
#ifndef LLAMA2Q_H
#define LLAMA2Q_H
#include <stdint.h>
#include <stddef.h>

/* Shape of the checkpoint this build is sized for: TinyStories 260K. ll_init() refuses any other header. */
#ifndef LL_DIM
#define LL_DIM 64
#define LL_HIDDEN 172
#define LL_LAYERS 5
#define LL_HEADS 8
#define LL_KV_HEADS 4
#define LL_VOCAB 512
#endif
#ifndef LL_STEPS
#define LL_STEPS 128          /* longest sequence, prompt included: sizes the key and value cache (1280 bytes per step) */
#endif
#define LL_MAX_PROMPT 64

/* Optional cycle counter for timing a forward pass; the target build points it at the DWT counter. */
#ifndef LL_CYCLES
#define LL_CYCLES() 0u
#endif

int ll_init(const uint8_t *model, const uint8_t *tokenizer);   /* 0 on success, a negative code otherwise */
/* Generate up to steps tokens (prompt included). Text goes to emit(); the token ids are left in ll_ids. Returns tokens processed. */
int ll_generate(const char *prompt, int steps, float temperature, float topp, unsigned long long seed, void (*emit)(const char *piece));

extern int ll_ids[LL_STEPS];            /* token fed at each position */
extern uint32_t ll_fwd_cycles[LL_STEPS]; /* LL_CYCLES() spent in each forward pass (wraps at 2^32) */
extern int ll_prompt_tokens;
#endif
