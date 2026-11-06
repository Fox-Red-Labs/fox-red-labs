/* Port functions beyond the screen's and the card's, used by the examples. */
#ifndef SHARP_PORT_H
#define SHARP_PORT_H
#include <stdint.h>
void     sharp_port_init(void);        /* clocks, pins, SPI at 1 MHz, millisecond tick */
uint32_t sharp_port_millis(void);
uint32_t sharp_port_micros(void);      /* wraps after about 71 minutes */
void     sharp_port_delay_us(uint32_t us);
uint32_t sharp_port_spi_hz(void);       /* the SPI clock now in use */
void     sharp_port_stack_paint(void);  /* call first thing in main, before the other calls */
uint32_t sharp_port_stack_free(void);   /* bytes of stack never touched since the paint */
#endif
