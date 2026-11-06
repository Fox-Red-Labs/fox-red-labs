/* What each board provides to the hello example, implemented once in port_dl.c for every
 * DriverLib board (MSPM0 and MSPM33). BSD-3-Clause. */
#ifndef EDU_BOARD_H
#define EDU_BOARD_H
#include <stdint.h>
#include "edumkii.h"

const edumkii_port_t *edu_port(void);
void edu_board_init(void);             /* call right after SYSCFG_DL_init() */
void edu_print(const char *s);         /* console text out (UART TX, 115200) */
uint32_t edu_stamp(void);              /* raw CPU-cycle stamp */
uint32_t edu_cycles_since(uint32_t s); /* cycles since the stamp, wrap-safe for under one wrap */
uint32_t edu_cpu_hz(void);

/* Diagnostics, where the board has them. */
uint16_t edu_sel_analog(void);         /* joystick press pin read as an analog channel (needs EDU_HSADC_SEL_SOC) */
void edu_spi_rate(uint32_t scr);       /* SPI bit rate = clock / ((1 + scr) * 2) (needs EDU_SPI_DMA) */
void edu_use_dma(int on);              /* 1 = bulk writes by DMA, 0 = polled (needs EDU_SPI_DMA) */
#endif
