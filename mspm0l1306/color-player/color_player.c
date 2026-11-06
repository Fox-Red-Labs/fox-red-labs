/*
 * Color player for the LP-MSPM0L1306 LaunchPad kit: plays a sequence of colors on LED2 (the RGB LED), reading
 * the sequence from a table in the last 1 KB flash sector. The browser bench (site/flash/) rewrites only that
 * sector, so a new sequence runs about a second after it is sent, with no compiler.
 *
 * Built on TI's gpio_toggle_output example from the MSPM0 SDK 2.11.00.07 (its SysConfig file sets up the three
 * LED2 pins); this file replaces the example's main source. TI's notice for the example is in NOTICE.txt beside
 * the prebuilt image. Build: python3 tools/webflash/build_player.py
 *
 * Table at 0x0000FC00, little-endian 32-bit words:
 *   magic 0x52434C46 ("FLCR"), count (1 to 32), then count pairs {colors, milliseconds}
 *   colors: bit 0 red (PA26), bit 1 green (PA13), bit 2 blue (PA27); 0 is off
 * A missing or damaged table plays red, green, blue, off, 300 ms each.
 */
#include "ti_msp_dl_config.h"

#define TABLE_ADDR    (0x0000FC00u)
#define TABLE_MAGIC   (0x52434C46u)
#define MAX_STEPS     (32u)
#define MIN_MS        (20u)
#define MAX_MS        (10000u)
#define CYCLES_PER_MS (CPUCLK_FREQ / 1000)

#define RED   GPIO_LEDS_USER_LED_2_PIN   /* PA26 */
#define GREEN GPIO_LEDS_USER_LED_3_PIN   /* PA13 */
#define BLUE  GPIO_LEDS_USER_LED_1_PIN   /* PA27 */

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

int main(void)
{
    SYSCFG_DL_init();
    const uint32_t all = RED | GREEN | BLUE;
    DL_GPIO_clearPins(GPIO_LEDS_PORT, all | GPIO_LEDS_USER_TEST_PIN);

    while (1) {
        const volatile table_t *t = (const volatile table_t *) TABLE_ADDR;
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
            uint32_t on = ((c & 1u) ? RED : 0u) | ((c & 2u) ? GREEN : 0u) | ((c & 4u) ? BLUE : 0u);
            DL_GPIO_clearPins(GPIO_LEDS_PORT, all & ~on);
            DL_GPIO_setPins(GPIO_LEDS_PORT, on);
            delay_cycles(ms * CYCLES_PER_MS);
        }
    }
}
