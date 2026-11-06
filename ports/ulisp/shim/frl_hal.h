#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void hal_init(void);
void hal_putc(int c);
int hal_getc(void);                 // next received byte, or -1
uint32_t hal_ms(void);
uint32_t hal_us(void);
void hal_delay_ms(uint32_t ms);
uint32_t hal_cpu_hz(void);
int hal_pin_count(void);
void hal_pin_mode(int idx, int mode);   // idx 0..2; mode 0 input, 1 output, 2 input with pull-up, 3 pull-down
void hal_pin_write(int idx, int v);
int hal_pin_read(int idx);
#ifdef __cplusplus
}
#endif
