/*
 * Color player for MSPM0 LaunchPad kits (every board but the LP-MSPM0L1306, which has its own copy in
 * mspm0l1306/color-player/): plays a sequence of colors on the board's user LED, reading the sequence from a table
 * in the last 1 KB flash sector. The browser bench (site/flash/) rewrites only that sector.
 *
 * Built on TI's gpio_toggle_output example for the board (its SysConfig file sets up the LED pins as outputs);
 * this file replaces the example's main source, and board_leds.h (made by tools/webflash/build_board.py from
 * tools/webflash/boards_mspm0.py) says which pins are red, green and blue, and which level lights them.
 *
 * Table at PL_TABLE_ADDR, little-endian 32-bit words:
 *   magic 0x52434C46 ("FLCR"), count (1 to 32), then count pairs {colors, milliseconds}
 *   colors: bit 0 red, bit 1 green, bit 2 blue; 0 is off. A channel the board lacks is ignored.
 * A missing or damaged table plays red, green, blue, off, 300 ms each.
 */
#include "ti_msp_dl_config.h"
#include "board_leds.h"

#define TABLE_MAGIC   (0x52434C46u)
#define MAX_STEPS     (32u)
#define MIN_MS        (20u)
#define MAX_MS        (10000u)
#define CYCLES_PER_MS (CPUCLK_FREQ / 1000)

typedef struct {
    uint32_t colors;
    uint32_t ms;
} step_t;

typedef struct {
    uint32_t magic;
    uint32_t count;
    step_t steps[MAX_STEPS];
} table_t;

static const step_t fallback[] = {{1, 300}, {2, 300}, {4, 300}, {0, 300}};

static void led(GPIO_Regs *port, uint32_t pin, uint32_t on)
{
    if (on == PL_ACTIVE_HIGH) {
        DL_GPIO_setPins(port, pin);
    } else {
        DL_GPIO_clearPins(port, pin);
    }
}

int main(void)
{
    SYSCFG_DL_init();
    while (1) {
        const volatile table_t *t = (const volatile table_t *) PL_TABLE_ADDR;
        const volatile step_t *s = fallback;
        uint32_t n = sizeof(fallback) / sizeof(fallback[0]);
        if (t->magic == TABLE_MAGIC && t->count >= 1u && t->count <= MAX_STEPS) {
            s = t->steps;
            n = t->count;
        }
        for (uint32_t i = 0; i < n; i++) {
            uint32_t ms = s[i].ms;
            ms = ms < MIN_MS ? MIN_MS : (ms > MAX_MS ? MAX_MS : ms);
            uint32_t c = s[i].colors;
            led(PL_R_PORT, PL_R_PIN, c & 1u);
            led(PL_G_PORT, PL_G_PIN, (c >> 1) & 1u);
            led(PL_B_PORT, PL_B_PIN, (c >> 2) & 1u);
            delay_cycles(ms * CYCLES_PER_MS);
        }
    }
}
