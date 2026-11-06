/* hal for a build-only fit check on the LP-MSPM33C321A: no touch display is on that board, so the screen calls only count
 * pixels and the touch panel never reports a finger. Everything else (games, gestures, drawing, measurement) is the real code.
 * A real SPI driver for the board's own screen replaces the two lcd functions; that adds a few hundred bytes. */
#include "ti_msp_dl_config.h"
#include "hal.h"

extern int main_app(void);
volatile uint32_t stub_px;

static volatile uint32_t ms_ticks;
void SysTick_Handler(void) { ms_ticks++; }
uint32_t hal_ms(void) { return ms_ticks; }
uint32_t hal_us(void)
{
    uint32_t m, v;
    do { m = ms_ticks; v = SysTick->VAL; } while (m != ms_ticks);
    return m * 1000u + (SysTick->LOAD - v) / (CPUCLK_FREQ / 1000000u);
}
uint32_t hal_stack_used(void) { return 0; }
void hal_idle(void) { __WFI(); }
void hal_log(const char *s) { (void)s; }
void hal_lcd_fill(int x, int y, int w, int h, uint16_t c) { (void)x; (void)y; (void)c; stub_px += (uint32_t)w * h; }
void hal_lcd_blit(int x, int y, int w, int h, const uint16_t *px) { (void)x; (void)y; (void)px; stub_px += (uint32_t)w * h; }
int  hal_touch(int *x, int *y) { (void)x; (void)y; return 0; }
void hal_init(void)
{
    SysTick->LOAD = CPUCLK_FREQ / 1000 - 1; SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
}

int main(void) { SYSCFG_DL_init(); main_app(); return 0; }
