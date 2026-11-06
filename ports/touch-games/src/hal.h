/* The whole hardware interface of the touch games: a small display and a touch panel.
 * A new board needs these eight functions and nothing else. Coordinates are pixels, origin top left, portrait. */
#ifndef HAL_H
#define HAL_H
#include <stdint.h>

#ifndef LCD_W
#define LCD_W 320
#endif
#ifndef LCD_H
#define LCD_H 480
#endif

void     hal_init(void);                                             /* clocks are already up; start screen, touch, timer */
void     hal_lcd_fill(int x, int y, int w, int h, uint16_t rgb565);  /* solid rectangle, already clipped by the caller */
void     hal_lcd_blit(int x, int y, int w, int h, const uint16_t *px); /* w*h RGB565 pixels, row by row */
int      hal_touch(int *x, int *y);                                  /* 1 finger down (x, y set), 0 up, -1 read failed */
uint32_t hal_ms(void);
uint32_t hal_us(void);
void     hal_log(const char *s);                                     /* text out (UART or stdout) */
uint32_t hal_stack_used(void);                                        /* bytes of stack touched since boot, 0 if the hal cannot tell */
void     hal_idle(void);                                             /* sleep until the next tick */
#endif
