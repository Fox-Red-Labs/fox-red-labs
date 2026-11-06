/* CMSIS core stand-in for a Linux or macOS host (no Cortex-M intrinsics): the plain C forms CMSIS-DSP falls back
   to when ARM_MATH_DSP is not defined. 14 lines. */
#ifndef CMSIS_COMPILER_HOST_H
#define CMSIS_COMPILER_HOST_H
#include <stdint.h>
#define __STATIC_INLINE static inline
#define __STATIC_FORCEINLINE static inline __attribute__((always_inline))
#define __ALIGNED(x) __attribute__((aligned(x)))
#define __PACKED __attribute__((packed))
#define __CLZ(x) ((x) == 0 ? 32u : (uint32_t)__builtin_clz(x))
static inline int32_t __SSAT(int32_t val, uint32_t sat) { int32_t max = (1 << (sat - 1)) - 1, min = -max - 1; return val > max ? max : (val < min ? min : val); }
static inline uint32_t __USAT(int32_t val, uint32_t sat) { uint32_t max = (1u << sat) - 1; return val > (int32_t)max ? max : (val < 0 ? 0u : (uint32_t)val); }
void host_bkpt(int code);
#define __BKPT(x) host_bkpt(x)
#endif
