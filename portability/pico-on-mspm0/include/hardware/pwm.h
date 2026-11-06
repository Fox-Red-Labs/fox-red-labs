/* Pico SDK on MSPM0: hardware/pwm.h. A Pico PWM slice is a timer here, its channels A and B are capture-compare 0 and 1, counting
 * up from 0 to the wrap value with the output high while the count is below the level (the Pico's default mode). The level takes
 * effect at the next wrap, as on the Pico. The clock divider divides the 32 MHz bus clock, not the Pico's 125 MHz. */
#ifndef PICO_MSPM0_PWM_H
#define PICO_MSPM0_PWM_H
#include "pico/types.h"
#include "pico_board.h"
typedef struct { uint32_t div16; uint16_t top; } pwm_config;
#define PWM_DEFAULT_IRQ_NUM() PICO_MSPM0_PWM_IRQN
uint pwm_gpio_to_slice_num(uint gpio);
static inline pwm_config pwm_get_default_config(void) { pwm_config c = { 16, 0xffff }; return c; }
static inline void pwm_config_set_clkdiv(pwm_config *c, float div) { c->div16 = (uint32_t) (div * 16.0f); }
void pwm_init(uint slice_num, pwm_config *c, bool start);
void pwm_set_gpio_level(uint gpio, uint16_t level);
void pwm_clear_irq(uint slice_num);
void pwm_set_irq_enabled(uint slice_num, bool enabled);
#endif
