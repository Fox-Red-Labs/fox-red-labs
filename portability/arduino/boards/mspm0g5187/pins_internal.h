/* Shim-private: pin number -> GPIO port, mask and IOMUX index. Not part of the Arduino API. */
#ifndef PINS_INTERNAL_H
#define PINS_INTERNAL_H
#include <stdint.h>
#include "ti_msp_dl_config.h"
#include "variant.h"

extern "C" void compat_bad_pin(uint32_t pin);   /* stops at a breakpoint */
extern "C" void compat_unsupported(const char *what);   /* stops at a breakpoint */

static inline GPIO_Regs *pin_port(uint32_t pin) { return pin < 32 ? GPIOA : GPIOB; }
static inline uint32_t pin_mask(uint32_t pin) { return 1UL << (pin & 31); }
uint32_t pin_iomux(uint32_t pin);   /* IOMUX_PINCMn index; calls compat_bad_pin() if the pin does not exist */
#endif
