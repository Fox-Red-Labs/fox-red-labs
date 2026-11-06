/* Pico SDK on MSPM0: hardware/adc.h. 12-bit results against VDDA (3.3 V on the LaunchPad), as on the Pico. Input n is the Pico's
 * ADC input: 0 to 3 are GPIO 26 to 29 through the pin table; 4, the Pico's temperature sensor, is not mapped. */
#ifndef PICO_MSPM0_ADC_H
#define PICO_MSPM0_ADC_H
#include "pico/types.h"
void adc_init(void);
void adc_gpio_init(uint gpio);
void adc_select_input(uint input);
uint16_t adc_read(void);
#endif
