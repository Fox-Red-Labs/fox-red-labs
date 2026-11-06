#ifndef SHIM_INTERP_H
#define SHIM_INTERP_H
#include "pico.h"
/* Software model of one RP2040 interpolator, field names as in the SDK. */
typedef struct { uint32_t accum[2]; uint32_t base[3]; uint32_t add_raw[2]; uint32_t pop[3]; } interp_hw_t;
typedef struct { uint32_t ctrl; } interp_config;
extern interp_hw_t shim_interp0, shim_interp1;
#define interp0_hw (&shim_interp0)
#define interp1_hw (&shim_interp1)
#define interp0 interp0_hw
#define interp1 interp1_hw
static inline interp_config interp_default_config(void) { interp_config c = {0}; return c; }
static inline void interp_config_set_add_raw(interp_config *c, bool v) { (void)c; (void)v; }
static inline void interp_config_set_shift(interp_config *c, uint s) { (void)c; (void)s; }
static inline void interp_config_set_mask(interp_config *c, uint a, uint b) { (void)c; (void)a; (void)b; }
static inline void interp_config_set_cross_input(interp_config *c, bool v) { (void)c; (void)v; }
static inline void interp_set_config(interp_hw_t *i, uint lane, interp_config *c) { (void)i; (void)lane; (void)c; }
#endif
