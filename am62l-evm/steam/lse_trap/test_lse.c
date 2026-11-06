/* Self-check for lse_trap.so: built with LSE (-march=armv8.1-a), so on a Cortex-A53 every atomic here traps.
 * Expect "PASS" with LD_PRELOAD=lse_trap.so and SIGILL without it. Two threads also race 1e6 increments. */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
static volatile uint64_t c64; static volatile uint32_t c32; static volatile uint16_t c16; static volatile uint8_t c8;
static void *worker(void *a) {
    (void)a;
    for (int i = 0; i < 500000; i++) { __atomic_fetch_add(&c64, 1, __ATOMIC_SEQ_CST); __atomic_fetch_add(&c32, 1, __ATOMIC_RELAXED); }
    return 0;
}
int main(void) {
    int bad = 0;
    uint64_t x = 5; uint64_t o = __atomic_exchange_n(&x, 9, __ATOMIC_ACQUIRE); bad |= (o != 5 || x != 9);
    uint32_t w = 0xF0; __atomic_fetch_or(&w, 0x0F, __ATOMIC_SEQ_CST); bad |= (w != 0xFF);
    __atomic_fetch_and(&w, 0x3C, __ATOMIC_SEQ_CST); bad |= (w != 0x3C);
    __atomic_fetch_xor(&w, 0xFF, __ATOMIC_SEQ_CST); bad |= (w != 0xC3);
    uint64_t e = 9; bad |= !__atomic_compare_exchange_n(&x, &e, 11, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST) || x != 11;
    e = 3; bad |= __atomic_compare_exchange_n(&x, &e, 12, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST) || e != 11;
    c16 = 0xFFFF; __atomic_fetch_add(&c16, 1, __ATOMIC_SEQ_CST); bad |= (c16 != 0);
    c8 = 250; uint8_t ob = __atomic_fetch_add(&c8, 10, __ATOMIC_SEQ_CST); bad |= (ob != 250 || c8 != 4);
    pthread_t t[2]; for (int i = 0; i < 2; i++) pthread_create(&t[i], 0, worker, 0);
    for (int i = 0; i < 2; i++) pthread_join(t[i], 0);
    bad |= (c64 != 1000000 || c32 != 1000000);
    printf("%s c64=%llu c32=%u\n", bad ? "FAIL" : "PASS", (unsigned long long)c64, c32);
    return bad;
}
