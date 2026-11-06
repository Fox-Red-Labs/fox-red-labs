/* Hardware and LVGL glue for the LP-MSPM0G3507 + Waveshare 3.5in (ST7796S + FT6336U). */
#ifndef PORT_H
#define PORT_H
#include <stdint.h>

#define LCD_W 320
#define LCD_H 480

/* Counters the port keeps for the on-screen stats line (all times in microseconds). */
typedef struct {
    uint32_t refr_cnt;        /* finished screen refreshes (a refresh is one or more strips) */
    uint32_t refr_us_sum;     /* time from refresh start to refresh ready, summed */
    uint32_t refr_us_last;
    uint32_t refr_us_max;     /* largest single refresh since last reset by the app */
    uint32_t flush_us_sum;    /* time spent sending pixels to the screen, summed */
    uint32_t flush_us_last;
    uint32_t flush_px;        /* pixels sent */
    uint32_t full_redraw_us;  /* refresh time of the last full-screen redraw (0 until asked for) */
    uint8_t  full_pending;    /* the app sets this, then invalidates the screen */
    uint32_t touch_errors;    /* I2C reads the touch chip did not acknowledge */
} port_stats_t;
extern port_stats_t port_stats;

void     port_init(void);          /* clocks are already up (SYSCFG_DL_init); starts SysTick, screen, touch, LVGL display and input */
uint32_t port_ms(void);            /* milliseconds since start (LVGL's tick source) */
uint32_t port_us(void);            /* microseconds since start */
void     port_led(int on, int level_0_to_10);   /* red LED1 on PA0, software PWM from the 1 ms tick */
void     port_uart_puts(const char *s);
uint32_t port_stack_free(void);    /* bytes of stack never touched since boot */
#endif
