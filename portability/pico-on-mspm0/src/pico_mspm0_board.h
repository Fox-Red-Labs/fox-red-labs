/* Pico SDK on MSPM0: what the layer needs from a board folder (pins.c). Not a Pico SDK header. */
#ifndef PICO_MSPM0_BOARD_TABLE_H
#define PICO_MSPM0_BOARD_TABLE_H
#include <ti/devices/msp/msp.h>
typedef struct { GPIO_Regs *port; uint32_t bit, iomux, pf_uart, pf_i2c, pf_pwm; int8_t pwm, adc; } pico_pin_t;   /* pwm = slice * 2 + channel */
extern const pico_pin_t pico_pins[30];
extern GPTIMER_Regs *const pico_pwm_slices[1];
extern const int8_t pico_adc_inputs[5];
void pico_board_init(void);
#define PICO_PIN(g) ((g) < 30 && pico_pins[g].port ? &pico_pins[g] : (const pico_pin_t *) 0)
#endif
