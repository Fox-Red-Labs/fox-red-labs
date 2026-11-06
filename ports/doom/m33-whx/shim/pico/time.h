#ifndef SHIM_TIME_H
#define SHIM_TIME_H
#include "pico.h"
typedef uint64_t absolute_time_t;
uint32_t time_us_32(void);
uint64_t time_us_64(void);
static inline uint32_t to_ms_since_boot(absolute_time_t t) { return (uint32_t)(t / 1000u); }
absolute_time_t get_absolute_time(void);
void sleep_ms(uint32_t ms);
void sleep_us(uint64_t us);
static inline uint32_t us_to_ms(uint64_t us) { return (uint32_t)(us / 1000u); }
static inline absolute_time_t make_timeout_time_ms(uint32_t ms) { return get_absolute_time() + (uint64_t)ms * 1000u; }
#endif
