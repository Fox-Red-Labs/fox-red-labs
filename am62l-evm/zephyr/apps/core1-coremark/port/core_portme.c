#include "coremark.h"
#if PERFORMANCE_RUN
volatile ee_s32 seed1_volatile = 0x0;
volatile ee_s32 seed2_volatile = 0x0;
volatile ee_s32 seed3_volatile = 0x66;
#endif
volatile ee_s32 seed4_volatile = ITERATIONS;
volatile ee_s32 seed5_volatile = 0;
static uint64_t t_start, t_stop, freq;
static inline uint64_t cnt(void) { uint64_t v; __asm__ volatile("isb; mrs %0, cntvct_el0" : "=r"(v)); return v; }
void start_time(void) { __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq)); t_start = cnt(); }
void stop_time(void) { t_stop = cnt(); }
CORE_TICKS get_time(void) { return t_stop - t_start; }
secs_ret time_in_secs(CORE_TICKS ticks) { return ((secs_ret)ticks) / (secs_ret)freq; }
ee_u32 default_num_contexts = 1;
void portable_init(core_portable *p, int *argc, char *argv[]) { (void)argc; (void)argv; p->portable_id = 1; }
void portable_fini(core_portable *p) { p->portable_id = 0; }
