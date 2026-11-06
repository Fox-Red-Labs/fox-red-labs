/* Pico SDK on MSPM0: hardware/gpio.h. Pico GPIO numbers 0 to 29 go through the board's pin table (boards/<board>/pins.c);
 * a number the table does not map is ignored. The function numbers are the RP2040's, so code that stores them still compiles. */
#ifndef PICO_MSPM0_GPIO_H
#define PICO_MSPM0_GPIO_H
#include "pico/types.h"
enum gpio_dir { GPIO_IN = 0, GPIO_OUT = 1 };
typedef enum gpio_function_rp2040 {
    GPIO_FUNC_XIP = 0, GPIO_FUNC_SPI = 1, GPIO_FUNC_UART = 2, GPIO_FUNC_I2C = 3, GPIO_FUNC_PWM = 4, GPIO_FUNC_SIO = 5,
    GPIO_FUNC_PIO0 = 6, GPIO_FUNC_PIO1 = 7, GPIO_FUNC_GPCK = 8, GPIO_FUNC_USB = 9, GPIO_FUNC_NULL = 0x1f,
} gpio_function_t;
void gpio_init(uint gpio);
void gpio_set_dir(uint gpio, bool out);
void gpio_put(uint gpio, bool value);
void gpio_set_function(uint gpio, gpio_function_t fn);
void gpio_pull_up(uint gpio);
#endif
