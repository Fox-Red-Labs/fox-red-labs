/* CoreMark port for Zephyr on one AArch64 core: time from the generic counter, memory static, output through printk. */
#ifndef CORE_PORTME_H
#define CORE_PORTME_H
#include <stddef.h>
#include <stdint.h>
#include <zephyr/sys/printk.h>
#define HAS_FLOAT 1
#define HAS_TIME_H 0
#define USE_CLOCK 0
#define COMPILER_VERSION "GCC" __VERSION__
#define COMPILER_FLAGS FLAGS_STR
#define MEM_LOCATION "STATIC"
typedef signed short ee_s16; typedef unsigned short ee_u16; typedef signed int ee_s32; typedef double ee_f32;
typedef unsigned char ee_u8; typedef unsigned int ee_u32; typedef ee_u32 ee_ptr_int; typedef size_t ee_size_t;
#define align_mem(x) (void *)(4 + (((ee_ptr_int)(x)-1) & ~3))
typedef uint64_t CORE_TICKS;
#define CORETIMETYPE uint64_t
#define SEED_METHOD SEED_VOLATILE
#define MULTITHREAD 1
#define USE_PTHREAD 0
#define USE_FORK 0
#define USE_SOCKET 0
#define MAIN_HAS_NOARGC 0
#define MAIN_HAS_NORETURN 0
#define ee_printf printk
extern ee_u32 default_num_contexts;
typedef struct CORE_PORTABLE_S { ee_u8 portable_id; } core_portable;
void portable_init(core_portable *p, int *argc, char *argv[]);
void portable_fini(core_portable *p);
#if !defined(PROFILE_RUN) && !defined(PERFORMANCE_RUN) && !defined(VALIDATION_RUN)
#define PERFORMANCE_RUN 1
#endif
#endif
