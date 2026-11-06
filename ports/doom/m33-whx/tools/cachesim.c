/* Trace-driven page cache simulator. Input: trace of 128-byte units (uint32), 0xFFFFFFFF + frame number at frame ends;
 * class map from whx_classes.py. Fully or set-associative LRU. Output per config: misses, by class, per-frame max.
 *   cachesim <trace.bin> <classes.bin> <ncls> <total_reads> <page_bytes> <cache_bytes> <ways> [pin_class_mask]
 * pin_class_mask: bit c set = reads of class c bypass the cache and count as hits (modelled as resident in RAM). */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
int main(int argc, char **argv) {
    if (argc < 8) return 2;
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f) / 4; fseek(f, 0, SEEK_SET);
    uint32_t *t = malloc(n * 4); fread(t, 4, n, f); fclose(f);
    f = fopen(argv[2], "rb"); fseek(f, 0, SEEK_END); long nu = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *cl = malloc(nu); fread(cl, 1, nu, f); fclose(f);
    int ncls = atoi(argv[3]); double total = atof(argv[4]);
    int page = atoi(argv[5]), cbytes = atoi(argv[6]), ways = atoi(argv[7]); uint64_t pin = argc > 8 ? strtoull(argv[8], 0, 0) : 0;
    int shift = 0; while ((128 << shift) < page) shift++;
    int pages = cbytes / page, sets = pages / ways; if (sets < 1) { sets = 1; ways = pages; }
    uint32_t *tag = malloc(sizeof(uint32_t) * sets * ways); uint32_t *age = calloc(sets * ways, 4);
    memset(tag, 0xff, sizeof(uint32_t) * sets * ways);
    long miss = 0, pinned = 0, fm = 0, fmax = 0, frames = 0; uint32_t clock = 0; long cm[64] = {0};
    for (long i = 0; i < n; i++) {
        uint32_t u = t[i];
        if (u == 0xFFFFFFFFu) { i++; frames++; if (fm > fmax) fmax = fm; fm = 0; continue; }
        int c = u < nu ? cl[u] : 0;
        if (pin >> c & 1) { pinned++; continue; }
        uint32_t p = u >> shift; int s = p % sets; uint32_t *tg = tag + s * ways, *ag = age + s * ways; int hit = -1, lru = 0;
        for (int w = 0; w < ways; w++) { if (tg[w] == p) { hit = w; break; } if (ag[w] < ag[lru]) lru = w; }
        clock++;
        if (hit >= 0) { ag[hit] = clock; continue; }
        miss++; fm++; cm[c]++; tg[lru] = p; ag[lru] = clock;
    }
    printf("page %d cache %d ways %d: misses %ld  miss/frame %.1f  max/frame %ld  hit%% %.3f  frames %ld\n", page, cbytes, ways, miss, (double)miss / frames, fmax, 100.0 * (1 - miss / total), frames);
    printf("  by class:"); for (int c = 0; c < ncls; c++) if (cm[c]) printf(" %d:%ld", c, cm[c]); printf("\n");
    return 0;
}
