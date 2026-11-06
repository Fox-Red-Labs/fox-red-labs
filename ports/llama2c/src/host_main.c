/* Host build of llama2q (Mac or Linux): reads the checkpoint and tokenizer from files, prints text like run.c, then the token ids.
 *   host_main <model_q80.bin> <tok512.bin> [-t temp] [-p topp] [-s seed] [-n steps] [-i prompt] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "llama2q.h"

static uint8_t *slurp(const char *path)
{
    FILE *f = fopen(path, "rb"); if (!f) { fprintf(stderr, "cannot open %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = aligned_alloc(16, ((size_t)n + 15) & ~15u); if (fread(b, 1, n, f) != (size_t)n) exit(1); fclose(f); return b;
}
static void emit(const char *piece)
{
    if (piece[0] == '\0') return;
    if (piece[1] == '\0' && !(isprint((unsigned char)piece[0]) || isspace((unsigned char)piece[0]))) return;   /* same filter as safe_printf in run.c */
    fputs(piece, stdout);
}
int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: host_main model_q80.bin tok512.bin [-t T] [-p P] [-s SEED] [-n STEPS] [-i PROMPT]\n"); return 1; }
    float t = 1.0f, p = 0.9f; unsigned long long seed = 42; int steps = 128; const char *prompt = "";
    for (int i = 3; i + 1 < argc; i += 2) {
        if (argv[i][1] == 't') t = atof(argv[i + 1]); else if (argv[i][1] == 'p') p = atof(argv[i + 1]);
        else if (argv[i][1] == 's') seed = atoi(argv[i + 1]); else if (argv[i][1] == 'n') steps = atoi(argv[i + 1]); else if (argv[i][1] == 'i') prompt = argv[i + 1];
    }
    int rc = ll_init(slurp(argv[1]), slurp(argv[2])); if (rc) { fprintf(stderr, "ll_init failed: %d\n", rc); return 1; }
    clock_t c0 = clock();
    int n = ll_generate(prompt, steps, t, p, seed, emit);
    double s = (double)(clock() - c0) / CLOCKS_PER_SEC;
    printf("\n");
    fprintf(stderr, "tokens %d, %.0f tok/s\n", n, n / s);
    fprintf(stderr, "ids:"); for (int i = 0; i < n; i++) fprintf(stderr, " %d", ll_ids[i]); fprintf(stderr, "\n");
    return 0;
}
