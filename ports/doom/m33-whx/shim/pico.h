/* Minimal stand-in for the Raspberry Pi pico-sdk headers (the SDK is not used on the M33). */
#ifndef SHIM_PICO_H
#define SHIM_PICO_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
typedef unsigned int uint;
#define __not_in_flash_func(f) f
#define __no_inline_not_in_flash_func(f) __attribute__((noinline)) f
#define __scratch_x(n)
#define __scratch_y(n)
#define __not_in_flash(n)
#define __time_critical_func(f) f
#define __noinline __attribute__((noinline))
#define __aligned(x) __attribute__((aligned(x)))
#define __unused __attribute__((unused))
#define __packed __attribute__((packed))
#define __always_inline inline __attribute__((always_inline))
#define __force_inline inline __attribute__((always_inline))
#define __breakpoint() __asm volatile("bkpt #0")
#define __wfi() __asm volatile("wfi")
#define __dmb() __asm volatile("dmb" ::: "memory")
#define __compiler_memory_barrier() __asm volatile("" ::: "memory")
#define PICO_ON_DEVICE_SHIM 1
void panic(const char *fmt, ...) __attribute__((noreturn));
#define TRUE 1
#define FALSE 0

#define XIP_BASE 0x60000000u
#define PICO_FLASH_SIZE_BYTES (8u*1024*1024)
#define CU_REGISTER_DEBUG_PINS(...)
#define CU_SELECT_DEBUG_PINS(x)
#define DEBUG_PINS_SET(x, y)
#define DEBUG_PINS_CLR(x, y)
#define DEBUG_PINS_XOR(x, y)
#define __XSTRING(x) __STRING(x)
#define __STRING(x) #x
static inline uint get_core_num(void) { return 0; }
#define bi_decl(x)
#define bi_program_feature(x) 0
#define __mul_instruction(a, b) ((a) * (b))
typedef volatile uint32_t spin_lock_t;
extern spin_lock_t shim_spin_lock;
#define spin_lock_instance(n) (&shim_spin_lock)
static inline uint32_t spin_lock_blocking(spin_lock_t *l) { (void)l; return 0; }
static inline void spin_unlock(spin_lock_t *l, uint32_t save) { (void)l; (void)save; }
#define __fast_mul(a, b) ((a) * (b))
#define hard_assert(x) do { if (!(x)) __breakpoint(); } while (0)
#ifdef __cplusplus
extern "C" {
#endif
void shim_core1_loop_once(void);
#ifdef __cplusplus
}
#endif
#endif
