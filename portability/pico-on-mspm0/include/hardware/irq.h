/* Pico SDK on MSPM0: hardware/irq.h. IRQ numbers are the MSPM0's IRQn values; a handler set here is called from the layer's
 * interrupt entry for that line (only the PWM timer's line is routed, src/pwm.c). */
#ifndef PICO_MSPM0_IRQ_H
#define PICO_MSPM0_IRQ_H
#include "pico/types.h"
typedef void (*irq_handler_t)(void);
void irq_set_exclusive_handler(uint num, irq_handler_t handler);
void irq_set_enabled(uint num, bool enabled);
#endif
