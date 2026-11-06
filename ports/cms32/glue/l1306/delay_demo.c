#include "delay_demo.h"
__attribute__((weak)) void delayMS(uint32_t n) { m0_delay_ms(n); }   /* some main.c files define their own */
void delayUS(uint32_t n) { m0_delay_us(n); }
