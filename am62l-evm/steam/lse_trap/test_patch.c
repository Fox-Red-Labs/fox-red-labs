/* Check for lse_trap.so with LSE_PATCH=1 (and without): every LSE form the patcher rewrites, at sizes 1 to 8
 * bytes, with SP-relative and register bases, an aligned-then-unaligned site (the stub's SIGBUS path), and a
 * two-thread race. Build with -march=armv8.1-a. Prints PASS or the first mismatch, and the time of 1e6 adds. */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#define T(x) do { if (!(x)) { printf("FAIL line %d\n", __LINE__); bad = 1; } } while (0)
static volatile uint64_t race64; static volatile uint32_t race32;
__attribute__((noinline)) static uint32_t add_at(uint32_t *p, uint32_t v) { return __atomic_fetch_add(p, v, __ATOMIC_SEQ_CST); }
static void *worker(void *a) { (void)a; for (int i = 0; i < 300000; i++) { __atomic_fetch_add(&race64, 1, __ATOMIC_ACQ_REL); __atomic_fetch_sub(&race32, 1, __ATOMIC_RELAXED); } return 0; }
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
int main(void) {
    int bad = 0;
    for (int round = 0; round < 3; round++) {     /* round 0 traps and patches, rounds 1-2 run the stubs */
        volatile uint8_t b = 250; volatile uint16_t h = 0xFFF0; volatile uint32_t w = 0xF0; volatile uint64_t x = 5;
        T(__atomic_fetch_add(&b, 10, __ATOMIC_SEQ_CST) == 250 && b == 4);
        T(__atomic_fetch_add(&h, 0x20, __ATOMIC_RELAXED) == 0xFFF0 && h == 0x10);
        T(__atomic_fetch_or(&w, 0x0F, __ATOMIC_SEQ_CST) == 0xF0 && w == 0xFF);
        T(__atomic_fetch_and(&w, 0x3C, __ATOMIC_ACQUIRE) == 0xFF && w == 0x3C);
        T(__atomic_fetch_xor(&w, 0xFF, __ATOMIC_RELEASE) == 0x3C && w == 0xC3);
        T(__atomic_fetch_or(&b, 0x80, __ATOMIC_SEQ_CST) == 4 && b == 0x84);
        T(__atomic_fetch_and(&h, 0x0F, __ATOMIC_SEQ_CST) == 0x10 && h == 0);
        T(__atomic_exchange_n(&x, 9, __ATOMIC_ACQUIRE) == 5 && x == 9);
        T(__atomic_exchange_n(&b, 7, __ATOMIC_SEQ_CST) == 0x84 && b == 7);
        T(__atomic_fetch_add(&x, 0x100000000ULL, __ATOMIC_SEQ_CST) == 9 && x == 0x100000009ULL);
        uint64_t e = 0x100000009ULL; T(__atomic_compare_exchange_n(&x, &e, 11, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST) && x == 11);
        e = 3; T(!__atomic_compare_exchange_n(&x, &e, 12, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST) && e == 11 && x == 11);
        uint32_t ew = 0xC3; T(__atomic_compare_exchange_n(&w, &ew, 1, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE) && w == 1);
        uint16_t eh = 0; T(__atomic_compare_exchange_n(&h, &eh, 0xBEEF, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST) && h == 0xBEEF);
        uint8_t eb = 6; T(!__atomic_compare_exchange_n(&b, &eb, 1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST) && eb == 7 && b == 7);
        {   /* CASP (128-bit pair) stays on the trap path; GCC calls libatomic for it, so it is written out */
            static uint64_t q[2] __attribute__((aligned(16))); q[0] = 7; q[1] = 0;
            register uint64_t e0 __asm__("x4") = 7, e1 __asm__("x5") = 0, n0 __asm__("x6") = 5, n1 __asm__("x7") = 1;
            __asm__ volatile("caspal x4, x5, x6, x7, [%[p]]" : "+r"(e0), "+r"(e1) : "r"(n0), "r"(n1), [p] "r"(q) : "memory");
            T(e0 == 7 && e1 == 0 && q[0] == 5 && q[1] == 1);
            e0 = 9; e1 = 9;
            __asm__ volatile("caspal x4, x5, x6, x7, [%[p]]" : "+r"(e0), "+r"(e1) : "r"(n0), "r"(n1), [p] "r"(q) : "memory");
            T(e0 == 5 && e1 == 1 && q[0] == 5 && q[1] == 1);
        }
        /* the flags must survive a CAS: compare, CAS, then use the compare's result */
        volatile int lhs = round, rhs = 1; uint64_t ee = 11; int lt;
        __asm__ volatile("cmp %w[l], %w[r]\n\tcasal %[e], %[n], [%[p]]\n\tcset %w[lt], lt"
                         : [e] "+r"(ee), [lt] "=r"(lt) : [l] "r"(lhs), [r] "r"(rhs), [n] "r"(13ULL), [p] "r"(&x) : "memory", "cc");
        T(lt == (round < 1) && x == 13 && ee == 11);
        /* SP-based address: the stub must address the caller's stack slot, not its own pushes */
        uint64_t spv;
        __asm__ volatile("mov x9, #40\n\tstr x9, [sp, #-16]!\n\tmov x10, #2\n\tldadd x10, x11, [sp]\n\tldr %[o], [sp], #16\n\tmov %[r], x11"
                         : [o] "=r"(spv), [r] "=r"(e) : : "x9", "x10", "x11", "memory");
        T(spv == 42 && e == 40);
        /* aligned the first time (patched), unaligned the next (the stub faults and falls back) */
        static uint8_t raw[16] __attribute__((aligned(16)));
        memset(raw, 0, sizeof raw);
        uint32_t *al = (uint32_t *)(raw + 4), *un = (uint32_t *)(raw + 5);
        T(add_at(al, 3) == 0 && *al == 3);
        T(add_at(un, 5) == 0x00000000u + (3u >> 8) && 1);
    }
    pthread_t t[2]; for (int i = 0; i < 2; i++) pthread_create(&t[i], 0, worker, 0);
    for (int i = 0; i < 2; i++) pthread_join(t[i], 0);
    T(race64 == 600000 && race32 == (uint32_t)-600000);
    volatile uint64_t c = 0; double t0 = now();
    for (int i = 0; i < 1000000; i++) __atomic_fetch_add(&c, 1, __ATOMIC_SEQ_CST);
    double t1 = now();
    T(c == 1000000);
    printf("%s; 1e6 atomic adds on one thread: %.3f s (%.0f ns each)\n", bad ? "FAIL" : "PASS", t1 - t0, (t1 - t0) * 1e3);
    return bad;
}
