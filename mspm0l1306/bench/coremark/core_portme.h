/*
 * CoreMark port for the LP-MSPM0G3507, bare metal (no RTOS), TI Arm Clang.
 * Time comes from SysTick on the CPU clock with a wrap counter (sdk-common/bench_timer.h):
 * one CoreMark tick is one CPU cycle. Output goes to a RAM buffer read by the debugger.
 */
#ifndef CORE_PORTME_H
#define CORE_PORTME_H
#include <stddef.h>
#include <stdint.h>

#ifndef HAS_FLOAT
#define HAS_FLOAT 1
#endif
#define HAS_TIME_H 0
#define USE_CLOCK 0
#define HAS_STDIO 0
#define HAS_PRINTF 0

#ifndef COMPILER_VERSION
#define COMPILER_VERSION "TI Arm Clang " __VERSION__
#endif
#ifndef COMPILER_FLAGS
#define COMPILER_FLAGS "not set"
#endif
#ifndef MEM_LOCATION
#define MEM_LOCATION "STACK"
#endif

typedef signed short ee_s16;
typedef unsigned short ee_u16;
typedef signed int ee_s32;
typedef double ee_f32;
typedef unsigned char ee_u8;
typedef unsigned int ee_u32;
typedef ee_u32 ee_ptr_int;
typedef size_t ee_size_t;
#ifndef NULL
#define NULL ((void *)0)
#endif
#define align_mem(x) (void *)(4 + (((ee_ptr_int)(x)-1) & ~3))

#define CORETIMETYPE uint64_t
typedef uint64_t CORE_TICKS;

#define SEED_METHOD SEED_VOLATILE
#define MEM_METHOD MEM_STACK
#define MULTITHREAD 1
#define USE_PTHREAD 0
#define USE_FORK 0
#define USE_SOCKET 0
#define MAIN_HAS_NOARGC 1
#define MAIN_HAS_NORETURN 0

extern ee_u32 default_num_contexts;

typedef struct CORE_PORTABLE_S {
	ee_u8 portable_id;
} core_portable;

void portable_init(core_portable *p, int *argc, char *argv[]);
void portable_fini(core_portable *p);
int ee_printf(const char *fmt, ...);

#if !defined(PROFILE_RUN) && !defined(PERFORMANCE_RUN) && !defined(VALIDATION_RUN)
#if (TOTAL_DATA_SIZE == 1200)
#define PROFILE_RUN 1
#elif (TOTAL_DATA_SIZE == 2000)
#define PERFORMANCE_RUN 1
#else
#define VALIDATION_RUN 1
#endif
#endif

#endif /* CORE_PORTME_H */
