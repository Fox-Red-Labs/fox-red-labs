/*
 * CoreMark port for the LP-EM-CC35X1 (CC3551E: Cortex-M33 at 160 MHz, code in external QSPI flash through a cache),
 * running as one FreeRTOS thread from TI's SimpleLink Wi-Fi SDK 10.20.00.39, TI Arm Clang. NOT RUN YET (2026-10-07).
 * Same settings as the LP-EM-CC2340R5 port (cc2340r5/baremetal/coremark) except: the 2 kB work area is
 * static (MEM_STATIC), because the SDK example thread stack is small, and time comes from the DWT cycle counter,
 * because FreeRTOS owns SysTick.
 */
#ifndef CORE_PORTME_H
#define CORE_PORTME_H
#include <stddef.h>
#include <stdint.h>
#define HAS_FLOAT 1
#define HAS_TIME_H 0
#define USE_CLOCK 0
#define HAS_STDIO 0
#define HAS_PRINTF 0
#ifndef COMPILER_VERSION
#define BENCH_STR2(x) #x
#define BENCH_STR(x) BENCH_STR2(x)
#define COMPILER_VERSION "TI Arm Clang " BENCH_STR(__ti_major__) "." BENCH_STR(__ti_minor__) "." BENCH_STR(__ti_patchlevel__)
#endif
#ifndef COMPILER_FLAGS
#define COMPILER_FLAGS "not set"
#endif
#ifndef MEM_LOCATION
#define MEM_LOCATION "STATIC"
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
#define MEM_METHOD MEM_STATIC
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
